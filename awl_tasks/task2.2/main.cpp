#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gstpipeline.h"
#include <cstddef>
#include<gst/gst.h>
#include<vector>
#include<string>
#include <glib-unix.h>


static void link_to_nvstreammux(GstElement* source, GstPad* pad, GstPad* nvstreammux_sinkpad) {
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


static gboolean send_eos_to_pipeline(gpointer pipeline){
    g_print("Sending EOS to pipeline...\n");
    gst_element_send_event((GstElement*)pipeline, gst_event_new_eos());
    return G_SOURCE_REMOVE;
}


static gboolean handle_bus_call(GstBus* bus, GstMessage* msg, gpointer loop) {
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


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        "file:///home/laptq/hello-gstreamer/assets/video_1.mp4",
        "rtsp://admin:12345@192.168.3.27/live"
    };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };
    g_object_set(
        G_OBJECT(streammux),
        "batch-size", list_uris.size(),
        "batched-push-timeout", 40000,
        "width", 640,
        "height", 640,
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

    GstElement* tiler { gst_element_factory_make("nvmultistreamtiler", "tiler") };
    g_object_set(
        G_OBJECT(tiler),
        "width", 1280,
        "height", 1280,
        NULL
    );

    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);

    gst_bin_add_many(
        GST_BIN(pipeline),
        streammux,
        tiler,
        converter,
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );

     gboolean _link_success { false };
    _link_success = gst_element_link_many(
        streammux,
        tiler,
        converter,
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
    gst_bus_add_watch(bus, handle_bus_call, loop);
    gst_object_unref(bus);
    
    // Khóa thread ở đây
    g_main_loop_run(loop);
    // Code chỉ chạy tiếp khi g_main_loop_quit() được gọi

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
    g_main_loop_unref(loop);
}