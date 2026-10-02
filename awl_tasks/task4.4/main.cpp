#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gstpipeline.h"
#include "gstnvdsmeta.h"
#include "nvds_roi_meta.h"
#include "nvdsmeta.h"
#include "nvll_osd_struct.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include<gst/gst.h>
#include<vector>
#include<string>
#include <glib-unix.h>
#include<unordered_map>
#include <stdexcept>
#include <chrono>
#include <cstdint>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <thread>


void link_to_nvstreammux(GstElement* source, GstPad* pad, GstPad* nvstreammux_sinkpad) {
    if (gst_pad_is_linked(nvstreammux_sinkpad)) {
        return;
    }

    if (!gst_pad_can_link(pad, nvstreammux_sinkpad)) {
        return;
    }

    GstPadLinkReturn _link_ret { gst_pad_link(pad, nvstreammux_sinkpad) };
    if (GST_PAD_LINK_FAILED(_link_ret)) {
        g_printerr("Link nvurisrcbin -> nvstreammux error");
    }
}


gboolean send_eos_to_nvstreammux(gpointer streammux){
    g_print("Sending EOS to streammux...\n");
    
    // Lặp qua tất cả các sink pad (sink_0, sink_1, ...) của streammux
    for (int i = 0; ; ++i) {
        std::string pad_name = "sink_" + std::to_string(i);
        GstPad* sinkpad { gst_element_get_static_pad((GstElement*)streammux, pad_name.c_str()) };
        if (!sinkpad) {
            break; // Đã duyệt hết các pad
        }
        gst_pad_send_event(sinkpad, gst_event_new_eos());
        gst_object_unref(sinkpad);
    }

    return G_SOURCE_REMOVE;
}


gboolean handle_bus_message(GstBus* bus, GstMessage* msg, gpointer loop) {
    GMainLoop* loop_ { (GMainLoop*)loop };

    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError* err {};
        gchar* debug_info;
        gst_message_parse_error(msg, &err, &debug_info);

        g_printerr("Bus received error from element: %s\n", GST_OBJECT_NAME(msg->src));
        g_printerr("Error message: %s\n", err->message);
        g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");

        g_clear_error(&err);
        g_free(debug_info);

        g_main_loop_quit(loop_);
    }
    else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
        g_print("Bus received EOS.\n");

        g_main_loop_quit(loop_);
    }

    return G_SOURCE_CONTINUE;
}


GstElement* create_multi_source_bin(const std::string& bin_name, const std::vector<std::string>& list_uris, int width, int height, bool live_source) {

    GstElement* bin { gst_pipeline_new(bin_name.c_str()) };

    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "live-source", live_source ? 1 : 0,
        "batch-size", list_uris.size(),
        "width", width,
        "height", height,
        "batched-push-timeout", 40000,
        "nvbuf-memory-type", 2, // 4: iGPU, 2; dGPU
        NULL
    );

    for (std::size_t i_u {0}; i_u < list_uris.size(); ++i_u) {
        GstElement* source { gst_element_factory_make("nvurisrcbin", ("source" + std::to_string(i_u)).c_str())};
        g_object_set(
            G_OBJECT(source), 
            "uri", list_uris[i_u].c_str(),
            "cudadec-memtype", 0,
            // reconnect
            "rtsp-reconnect-attempts", -1,
            "rtsp-reconnect-interval", 15,

            NULL
        );
        
        GstPad* sinkpad { gst_element_request_pad_simple(streammux, ("sink_" + std::to_string(i_u)).c_str())};

        g_signal_connect(source, "pad-added", G_CALLBACK(link_to_nvstreammux), sinkpad);

        gst_bin_add(GST_BIN(bin), source);

        gst_object_unref(sinkpad);
    }

    gst_bin_add(GST_BIN(bin), streammux);

    GstPad* streammux_src_pad { gst_element_get_static_pad(streammux, "src") };
    GstPad* ghost_pad { gst_ghost_pad_new("src", streammux_src_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(streammux_src_pad);

    // đăng ký bắt sự kiện interrupt
    g_unix_signal_add(SIGINT, send_eos_to_nvstreammux, streammux); // g_unix_signal_add hoạt động theo hàng đợi. Khi Ctrl-C, hàm send_eos_to_nvstreammux không chạy ngay mà chờ đến lượt được GMainLoop xử lý. 

    return bin;
}


GstElement* create_osd(int width, int height) {
    GstElement* tiler { gst_element_factory_make("nvmultistreamtiler", "tiler") };
    g_object_set(
        G_OBJECT(tiler),
        "width", width,
        "height", height,
        NULL
    );

    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };

    GstElement* nvosd { gst_element_factory_make("nvdsosd", "nvosd") };
    g_object_set(
        G_OBJECT(nvosd),
        "process-mode", 1,
        "display-text", 1,
        NULL
    );

    GstElement* bin { gst_pipeline_new("osd_bin") };
    gst_bin_add_many(GST_BIN(bin), tiler, converter, nvosd, NULL);
    if (!gst_element_link_many(tiler, converter, nvosd, NULL)) {
        g_printerr("Failed to link elements in osd_bin\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* tiler_sink_pad { gst_element_get_static_pad(tiler, "sink") };
    GstPad* ghost_pad { gst_ghost_pad_new("sink", tiler_sink_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(tiler_sink_pad);

    GstPad* nvosd_src_pad { gst_element_get_static_pad(nvosd, "src") };
    GstPad* ghost_src_pad { gst_ghost_pad_new("src", nvosd_src_pad) };
    gst_element_add_pad(bin, ghost_src_pad);
    gst_object_unref(nvosd_src_pad);

    return bin;
}


GstElement* create_mp4_filesink(const std::string& bin_name, const std::string& output_file_path) {
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser { gst_element_factory_make("h264parse", "parser") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", output_file_path.c_str(), NULL);


    GstElement* bin { gst_bin_new(bin_name.c_str()) };
    gst_bin_add_many(GST_BIN(bin), encoder, parser, mp4mux, sink, NULL);
    if (!gst_element_link_many(encoder, parser, mp4mux, sink, NULL)) {
        g_printerr("Failed to link elements in mp4_filesink_bin\n");
        gst_object_unref(bin);
        return nullptr;
    }

    GstPad* encoder_sink_pad { gst_element_get_static_pad(encoder, "sink") };
    GstPad* ghost_pad { gst_ghost_pad_new("sink", encoder_sink_pad) };
    gst_element_add_pad(bin, ghost_pad);
    gst_object_unref(encoder_sink_pad);

    return bin;
}


class AppBus {

public:
    void on_new_buffer(GstBuffer* buffer) {

    }
};


GstPadProbeReturn publish_gstbuffer(GstPad* pad, GstPadProbeInfo* info, gpointer app_bus_) {
    AppBus* app_bus { (AppBus*)app_bus_ };
    GstBuffer* buffer { (GstBuffer*)info->data };

    app_bus->on_new_buffer(buffer);

    return GST_PAD_PROBE_OK;
}


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        // "file:///home/laptq/hello-gstreamer/assets/sample_720p.h264",
        "rtsp://admin:12345@192.168.3.26/live",
        "rtsp://admin:12345@192.168.3.21/live"
    };

    GstElement* source { 
        create_multi_source_bin(
            "multi_source_bin", 
            list_uris, 
            960, 
            640, 
            true
        ) 
    };

    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", "awl_tasks/task4.4/nvinfer_detector_config_file.yml",
        NULL
    );      // 1 vài thuộc tính trong file config có thể được ghi đè thông qua Gst Properties

    GstElement* tracker { gst_element_factory_make("nvtracker", "tracker") };
    g_object_set(
        G_OBJECT(tracker),
        "tracker-width", 640,
        "tracker-height", 640,
        "gpu-id", 0,
        "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file", "/opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
        NULL
    );

    GstElement* person_view_classifier { gst_element_factory_make("nvinfer", "person_view_classifier") };
    g_object_set(
        G_OBJECT(person_view_classifier),
        "config-file-path", "awl_tasks/task4.4/nvinfer_person_view_config_file.yml",
        NULL
    );

    GstElement* nvdslogger { gst_element_factory_make ("nvdslogger", "nvdslogger") };
    g_object_set(G_OBJECT(nvdslogger), "fps-measurement-interval-sec", 1, NULL);

    
    GstElement* osd { create_osd(1920, 640) };
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* sink { create_mp4_filesink("mp4_filesink_bin", "outputs/output.mp4") };
    // GstElement* sink { gst_element_factory_make("fakesink", "sink") };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        detector,
        tracker,
        nvdslogger,
        osd,
        sink,
        NULL
    );

     gboolean _link_success { false };
    _link_success = gst_element_link_many(
        source,
        detector,
        tracker,
        nvdslogger,
        osd,
        sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }

    AppBus app_bus {};
    GstPad* tracker_srcpad { gst_element_get_static_pad(tracker, "src") };
    gst_pad_add_probe(tracker_srcpad, GST_PAD_PROBE_TYPE_BUFFER, publish_gstbuffer, &app_bus, NULL);
    g_object_unref(tracker_srcpad);

    GstStateChangeReturn _change_state { gst_element_set_state(pipeline, GST_STATE_PLAYING) };
    if (_change_state == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Pipeline playing error\n");
        g_object_unref(pipeline);
        return -1;
    }

    g_print("Pipeline running...\n");
    
    GstBus* bus { gst_element_get_bus(pipeline) };
    GMainLoop* loop { g_main_loop_new(NULL, FALSE) };
    gst_bus_add_watch(bus, handle_bus_message, loop);
    gst_object_unref(bus);
    
    // Khóa thread ở đây
    g_main_loop_run(loop);
    // Code chỉ chạy tiếp khi g_main_loop_quit() được gọi

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
    g_main_loop_unref(loop);
}