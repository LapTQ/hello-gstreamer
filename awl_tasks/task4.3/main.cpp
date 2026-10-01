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
    g_print("Sending EOS to pipeline...\n");
    GstPad* sinkpad { gst_element_get_static_pad((GstElement*)streammux, "sink_0") };
    if (sinkpad) {
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


GstPadProbeReturn draw_time_on_frame(GstPad* pad, GstPadProbeInfo* info, gpointer nothing) {
    GstBuffer* buffer { (GstBuffer*)info->data };
    NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };

    NvDsFrameMeta* frame_meta {};
    for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
        frame_meta = (NvDsFrameMeta*)i_frame->data;
        
        std::time_t t { std::time(nullptr) };
        std::tm tm { *std::localtime(&t) };
        char time_str[64] {};
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);
        
        NvDsDisplayMeta* display_meta { nvds_acquire_display_meta_from_pool(batch_meta) };
        display_meta->num_labels = 1;
        NvOSD_TextParams* text_params = &display_meta->text_params[0];
        text_params->x_offset = 10;
        text_params->y_offset = 10;
        text_params->display_text = (char*)g_malloc0(64);
        snprintf(text_params->display_text, 64, "%s", time_str);
        
        text_params->font_params.font_name =  (char*)g_malloc0(64);
        snprintf(text_params->font_params.font_name, 64, "Serif");
        text_params->font_params.font_size = 30;
        text_params->font_params.font_color = {0.0, 1.0, 0.0, 1.0};

        nvds_add_display_meta_to_frame(frame_meta, display_meta);
    }

    return GST_PAD_PROBE_OK;
}


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    GstElement* source { gst_element_factory_make("nvurisrcbin", "source") };
    g_object_set(
        G_OBJECT(source), 
        "uri", "rtsp://admin:12345@192.168.3.26/live",
        "cudadec-memtype", 0,

        // reconnect
        "rtsp-reconnect-attempts", -1,
        "rtsp-reconnect-interval", 15,
        NULL
    );
    
    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "live-source", 1,
        "batch-size", 1,
        "width", 960,
        "height", 640,
        "batched-push-timeout", 40000,
        "nvbuf-memory-type", 2, // 4: iGPU, 2; dGPU
        NULL
    );

    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", "awl_tasks/task2.2/nvinfer_detector_config_file.yml",
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

    GstElement* nvdslogger { gst_element_factory_make ("nvdslogger", "nvdslogger") };
    g_object_set(G_OBJECT(nvdslogger), "fps-measurement-interval-sec", 1, NULL);

    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };

    GstElement* nvosd { gst_element_factory_make("nvdsosd", "nvosd") };
    g_object_set(
        G_OBJECT(nvosd),
        "process-mode", 1,
        "display-text", 1,
        NULL
    );
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        streammux,
        detector,
        tracker,
        nvdslogger,
        converter,
        nvosd,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );

    // link source -> streammux
    GstPad* sinkpad { gst_element_request_pad_simple(streammux, "sink_0") };
    g_signal_connect(source, "pad-added", G_CALLBACK(link_to_nvstreammux), sinkpad);
    gst_object_unref(sinkpad);

     gboolean _link_success { false };
    _link_success = gst_element_link_many(
        streammux,
        detector,
        tracker,
        nvdslogger,
        converter,
        nvosd,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }

    // đăng ký bắt sự kiện interrupt
    g_unix_signal_add(SIGINT, send_eos_to_nvstreammux, streammux); // g_unix_signal_add hoạt động theo hàng đợi. Khi Ctrl-C, hàm send_eos_to_nvstreammux không chạy ngay mà chờ đến lượt được GMainLoop xử lý. 

    GstPad* osd_sink_pad { gst_element_get_static_pad(nvosd, "sink") };
    gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER, draw_time_on_frame, NULL, NULL);
    g_object_unref(osd_sink_pad);

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