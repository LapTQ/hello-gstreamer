#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gstpipeline.h"
#include "gstnvdsmeta.h"
#include "nvds_roi_meta.h"
#include <cstddef>
#include<gst/gst.h>
#include<vector>
#include<string>
#include <glib-unix.h>


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


gboolean send_eos_to_pipeline(gpointer pipeline){
    g_print("Sending EOS to pipeline...\n");
    gst_element_send_event((GstElement*)pipeline, gst_event_new_eos());
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


class Logger {
public:
    void info(const gchar* msg) {
        g_print("%s", msg);
    }
};

GstPadProbeReturn log_detection_result(GstPad* pad, GstPadProbeInfo* info, gpointer logger_) {
    Logger* logger { (Logger*)logger_ };
    
    GstBuffer* buffer { (GstBuffer*)info->data };
    NvDsBatchMeta* batch_meta { gst_buffer_get_nvds_batch_meta(buffer) };

    NvDsFrameMeta* frame_meta {};
    NvDsObjectMeta* obj_meta {};
    for (GList* i_frame { batch_meta->frame_meta_list }; i_frame != NULL; i_frame = i_frame->next) {
        frame_meta = (NvDsFrameMeta*)i_frame->data;

        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };
        
        logger->info("\n_________________________________________________________________\n");
        logger->info(("Source: " + std::to_string(frame_meta->source_id) + ", Frame ID: " + std::to_string(frame_meta->frame_num) + ", W: " + std::to_string(W) + ", H: " + std::to_string(H) + "\n").c_str());
        logger->info("_________________________________________________________________\nTrack | Class           | Box [x1n, y1n, x2n, y2n]\t | Conf |\n");
        for (GList* i_obj { frame_meta->obj_meta_list }; i_obj != NULL; i_obj = i_obj->next) {
            obj_meta = (NvDsObjectMeta*)i_obj->data;

            float x1 = (float)obj_meta->rect_params.left / W;
            float y1 = (float)obj_meta->rect_params.top / H;
            float x2 = (float)(obj_meta->rect_params.left + obj_meta->rect_params.width) / W;
            float y2 = (float)(obj_meta->rect_params.top + obj_meta->rect_params.height) / H;

            char box_str[128];
            std::snprintf(
                box_str, sizeof(box_str), 
                "%-5lu | %-15s | [%.2f, %.2f, %.2f, %.2f]   \t | %.2f |\n",
                obj_meta->object_id,
                obj_meta->obj_label,
                x1, y1, x2, y2,
                obj_meta->confidence
            );
            logger->info(box_str);
        }
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        // "file:///home/laptq/hello-gstreamer/assets/sample_720p.h264",
        "rtsp://admin:12345@192.168.3.26/live",
        "rtsp://admin:12345@192.168.3.21/live"
    };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "batch-size", list_uris.size(),
        "width", 640,
        "height", 640,
        "batched-push-timeout", 40000,
        "nvbuf-memory-type", 2, // 4: iGPU, 2; dGPU
        NULL
    );

    for (std::size_t i_u {0}; i_u < list_uris.size(); ++i_u) {
        GstElement* source { gst_element_factory_make("nvurisrcbin", ("source" + std::to_string(i_u)).c_str())};
        g_object_set(
            G_OBJECT(source), 
            "uri", list_uris[i_u].c_str(),
            // "file-loop", TRUE,  // nếu bật, cần gửi eos đến mọi sink pad của nvstreammux
            "cudadec-memtype", 0,
            NULL
        );
        
        GstPad* sinkpad { gst_element_request_pad_simple(streammux, ("sink_" + std::to_string(i_u)).c_str())};

        g_signal_connect(source, "pad-added", G_CALLBACK(link_to_nvstreammux), sinkpad);

        gst_bin_add(GST_BIN(pipeline), source);

        gst_object_unref(sinkpad);
    }

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
    Logger logger { };
    GstPad* tracker_srcpad { gst_element_get_static_pad(tracker, "src") };
    gst_pad_add_probe(tracker_srcpad, GST_PAD_PROBE_TYPE_BUFFER, log_detection_result, &logger, NULL);
    g_object_unref(tracker_srcpad);

    GstElement* tiler { gst_element_factory_make("nvmultistreamtiler", "tiler") };
    g_object_set(
        G_OBJECT(tiler),
        "width", 1280,
        "height", 640,
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
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);

    gst_bin_add_many(
        GST_BIN(pipeline),
        streammux,
        detector,
        tracker,
        tiler,
        converter,
        nvosd,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );

     gboolean _link_success { false };
    _link_success = gst_element_link_many(
        streammux,
        detector,
        tracker,
        tiler,
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
    g_unix_signal_add(SIGINT, send_eos_to_pipeline, pipeline); // g_unix_signal_add hoạt động theo hàng đợi. Khi Ctrl-C, hàm send_eos_to_pipeline không chạy ngay mà chờ đến lượt được GMainLoop xử lý. 

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