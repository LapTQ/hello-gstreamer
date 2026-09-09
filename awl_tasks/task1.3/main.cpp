// gst-launch-1.0 multifilesrc location=assets/sample_720p.h264 loop=true ! h264parse ! flvmux ! rtmp2sink location=rtmp://localhost:1935/live/stream


#include "gst/gstcaps.h"
#include "gst/gstelement.h"
#include "gst/gstmessage.h"
#include "gst/gstobject.h"
#include "gst/gstpad.h"
#include "gst/gststructure.h"
#include<gst/gst.h>


static void link_to_rtph264depay(GstElement* rtspsrc, GstPad* pad, GstElement* rtph264depay) {
    GstPad* rtph264depay_sinkpad { gst_element_get_static_pad(rtph264depay, "sink") };

    if (gst_pad_is_linked(rtph264depay_sinkpad)) {
        gst_object_unref(rtph264depay_sinkpad);
        return;
    }

    GstCaps* caps { gst_pad_get_current_caps(pad) };
    GstStructure* cap_structure { gst_caps_get_structure(caps, 0) };
    const gchar* cap_type { gst_structure_get_name(cap_structure) };
    if (!g_str_has_prefix(cap_type, "application/x-rtp")) {
        gst_object_unref(caps);
        gst_object_unref(rtph264depay_sinkpad);
        return;
    }

    GstPadLinkReturn _link_ret { gst_pad_link(pad, rtph264depay_sinkpad) };
    if (GST_PAD_LINK_FAILED(_link_ret)) {
        g_printerr("Link rtph264depay error");
    }

    gst_object_unref(rtph264depay_sinkpad);
}


int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    // extract H.264
    // GstElement* source { gst_element_factory_make("filesrc", "source") };
    // g_object_set(G_OBJECT(source), "location", "assets/sample_720p.h264", NULL);
    // GstElement* qtdemux { gst_element_factory_make("qtdemux", "qtdemux") };
    GstElement* source { gst_element_factory_make("rtspsrc", "source") };
    g_object_set(G_OBJECT(source), "location", "rtsp://127.0.0.1:8554/live/stream", NULL);
    g_object_set(G_OBJECT(source), "protocols", "tcp", NULL);
    GstElement* rtph264depay { gst_element_factory_make("rtph264depay", "rtph264depay") };
    g_signal_connect(source, "pad-added", G_CALLBACK(link_to_rtph264depay), rtph264depay);


    // decode H.264
    GstElement* parser1 { gst_element_factory_make("h264parse", "parser1") };
    GstElement* decoder { gst_element_factory_make("nvv4l2decoder", "decoder") };
    GstElement* converter { gst_element_factory_make("nvvideoconvert", "converter") };

    GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    // GstElement* encoder { gst_element_factory_make("nvv4l2h264enc", "encoder") };
    // GstElement* parser2 { gst_element_factory_make("h264parse", "parser2") };
    // GstElement* mp4mux { gst_element_factory_make("mp4mux", "mp4mux") };
    // GstElement* sink { gst_element_factory_make("filesink", "sink") };
    // g_object_set(G_OBJECT(sink), "location", "outputs/output.mp4", NULL);

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source, rtph264depay,
        parser1, decoder, converter, //
        sink,
        // encoder, parser2, mp4mux, sink,
        NULL
    );

    gboolean _link_success = gst_element_link_many(
        rtph264depay,
        parser1, decoder, converter, //
        sink,
        // encoder, parser2, mp4mux, sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }
    
    GstStateChangeReturn _change_state { gst_element_set_state(pipeline, GST_STATE_PLAYING) };
    if (_change_state == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Pipeline playing error\n");
        g_object_unref(pipeline);
        return -1;
    }
    
    GstBus* bus { gst_element_get_bus(pipeline) };
    
    GstMessage* msg { gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)) };
    
    if (msg != NULL) {

        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* err {};
            gchar* debug_info;
            gst_message_parse_error(msg, &err, &debug_info);

            g_printerr("Error from element: %s\n", GST_OBJECT_NAME(msg->src));
            g_printerr("Error message: %s\n", err->message);
            g_printerr("Debug info: %s\n", debug_info ? debug_info : "none");

            g_clear_error(&err);
            g_free(debug_info);
        }
        else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS) {
            g_print("EOS\n");
        }
        else {
            g_printerr("Unexpected error\n");
        }
        gst_message_unref(msg);
    }
    
    g_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);

}