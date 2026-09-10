#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstmessage.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gststructure.h"
#include "gst/gstutils.h"
#include<gst/gst.h>
#include <glib-unix.h>


static void link_to_h264parse(GstElement* qtdemux, GstPad* pad, GstElement* h264parse) {
    GstPad* h264parse_sinkpad { gst_element_get_static_pad(h264parse, "sink") };

    if (gst_pad_is_linked(h264parse_sinkpad)) {
        gst_object_unref(h264parse_sinkpad);
        return;
    }

    GstCaps* caps { gst_pad_get_current_caps(pad) };
    GstStructure* cap_structure { gst_caps_get_structure(caps, 0) };
    const gchar* name { gst_structure_get_name(cap_structure) };

    // element đứng ngay phía sau (h264parse) chỉ chấp nhận video/x-h264
    if (!g_str_has_prefix(name, "video/x-h264")) {
        gst_caps_unref(caps);
        gst_object_unref(h264parse_sinkpad);
        return;
    }
    // hoặc thay vì phải nhớ chuỗi video/x-h264, có thể dùng `gst_pad_is_linked` trực tiếp xem 2 pad có cắm được vào nhau hay không

    GstPadLinkReturn _link_ret { gst_pad_link(pad, h264parse_sinkpad) };
    if (GST_PAD_LINK_FAILED(_link_ret)) {
        g_printerr("Link qtdemux -> h264parse error");
    }
    
    gst_caps_unref(caps);
    gst_object_unref(h264parse_sinkpad);
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


int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    // extract H.264
    GstElement* source { gst_element_factory_make("filesrc", "source") };
    g_object_set(G_OBJECT(source), "location", "assets/video_1.mp4", NULL);
    GstElement* qtdemux { gst_element_factory_make("qtdemux", "qtdemux") };
    GstElement* parser1 { gst_element_factory_make("h264parse", "parser1") };
    g_signal_connect(qtdemux, "pad-added", G_CALLBACK(link_to_h264parse), parser1);
    
    // decode H.264
    GstElement* decoder { gst_element_factory_make("nvv4l2decoder", "decoder") };
    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    GstElement* sink { gst_element_factory_make("filesink", "sink") };
    g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);
    
    GstElement* pipeline { gst_pipeline_new("pipeline") };
    
    gst_bin_add_many(
        GST_BIN(pipeline),
        source, qtdemux, parser1,
        decoder, converter, //
        // sink,
        encoder, parser2, mp4mux, sink,
        NULL
    );
    
    gboolean _link_success { false };
    _link_success = gst_element_link(source, qtdemux);
    _link_success = _link_success && gst_element_link_many(
        parser1, 
        decoder, converter, //
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