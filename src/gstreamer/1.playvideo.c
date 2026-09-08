#include<gst/gst.h>

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    GstElement* pipeline;
    pipeline = gst_pipeline_new("playvideo_pipeline");

    GstElement* source;
    source = gst_element_factory_make("filesrc", "source");
    g_object_set(source, "location", "assets/video_1.mp4", NULL);

    GstElement* decode;
    decode = gst_element_factory_make("decodebin", "decode");

    GstElement* convert;
    convert = gst_element_factory_make("videoconvert", "convert");

    GstElement* sink;
    sink = gst_element_factory_make("autovideosink", "sink");

    gst_bin_add_many(GST_BIN(pipeline), source, decode, convert, sink, NULL);

    gst_element_link_many(source, decode, convert, sink, NULL);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;
}
