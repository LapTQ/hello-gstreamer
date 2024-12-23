// https://gstreamer.freedesktop.org/documentation/tutorials/basic/dynamic-pipelines.html?gi-language=python

/*
Introduction

We are going to open a file which is multiplexed (or muxed), this is, 
audio and video are stored together inside a container file. 
Some examples of container formats are: AVI, MP4, MKV, etc.
The elements responsible for opening such containers are called demuxers.

If a container embeds multiple streams (one video and two audio tracks, for example), 
the demuxer will separate them and expose them through different output ports. 

The ports through which GStreamer elements communicate with each other are called "pads" (`GstPad`).
- sink pads: through which data enters an element.
- source pads: through which data exits an element.
Naturally, source elements only contain source pads, sink elements only contain sink pads, 
and filter elements contain both. A demuxer contains one sink pad, and multiple source pads,
one for each stream found in the container.
*/

#include <gst/gst.h>


/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline;
    GstElement *source;
    GstElement *convert;
    GstElement *resample;
    GstElement *sink;
} CustomData;

/* Handler for the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *pad, CustomData *data);

// ======================================================================

int main(int argc, char *argv[]) {
    CustomData data;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    gboolean terminate = FALSE;

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    data.source = gst_element_factory_make ("uridecodebin", "source");
    data.convert = gst_element_factory_make ("audioconvert", "convert");
    data.resample = gst_element_factory_make ("audioresample", "resample");
    data.sink = gst_element_factory_make ("autoaudiosink", "sink");

    /*
    `uridecodebin`
        - internally instantiate all the necessary elements (sources, demuxers and decoders)
            to turn a URI into raw audio and/or video streams.
        - It does half the work that playbin does.
        - Since it contains demuxers, its source pads are not initially available 
            and we will need to link to them on the fly.

    `audioconvert` 
        - converting between different audio formats, making sure that this example 
            will work on any platform, since the format produced by the audio decoder 
            might not be the same that the audio sink expects.
    
    `audioresample`
        - converting between different audio sample rates, similarly making sure that this example 
            will work on any platform, since the audio sample rate produced by the 
            audio decoder might not be one that the audio sink supports.
    */

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.source || !data.convert || !data.resample || !data.sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline. Note that we are NOT linking the source at this
    * point. We will do it later. */
    gst_bin_add_many (
        GST_BIN (data.pipeline), 
        data.source, 
        data.convert, 
        data.resample, 
        data.sink, 
        NULL
    );

    if (!gst_element_link_many (data.convert, data.resample, data.sink, NULL)) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Set the URI to play */
    g_object_set (
        data.source, 
        "uri", "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm", 
        NULL
    );

    /* Connect to the pad-added signal */
    g_signal_connect (
        data.source, 
        "pad-added", 
        G_CALLBACK (pad_added_handler), 
        &data
    );
    /*
    `GSignals` 
        - allow you to be notified (by means of a callback) when something interesting has happened.
        - we are attaching to the “pad-added” signal of our `uridecodebin` element.
        - GStreamer will forward the data pointer to the callback. In this case,
            we are passing a pointer to our `CustomData` structure.
        - `uridecodebin` can create as many pads as it sees fit, and for each one, this callback will be called.
        - The signals that a GstElement generates can be found in its documentation or using the `gst-inspect-1.0`.
    */

    /* Start playing */
    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (data.pipeline);
        return -1;
    }

    /* Listen to the bus */
    bus = gst_element_get_bus (data.pipeline);

    do {
        msg = gst_bus_timed_pop_filtered (
            bus, 
            GST_CLOCK_TIME_NONE,
            GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS
        );

        /* Parse message */
        if (msg != NULL) {
            GError *err;
            gchar *debug_info;

            switch (GST_MESSAGE_TYPE (msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error (msg, &err, &debug_info);
                g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error (&err);
                g_free (debug_info);
                terminate = TRUE;
                break;
            case GST_MESSAGE_EOS:
                g_print ("End-Of-Stream reached.\n");
                terminate = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED:
                /* We are only interested in state-changed messages from the pipeline */
                if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data.pipeline)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
                g_print ("Pipeline state changed from %s to %s:\n",
                    gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
                }
                break;
            default:
                /* We should not reach here */
                g_printerr ("Unexpected message received.\n");
                break;
            }
            gst_message_unref (msg);
        }
    } while (!terminate);

    /* Free resources */
    gst_object_unref (bus);
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);
    return 0;
}

/* This function will be called by the pad-added signal */
static void pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data) {
    /*
    `src` 
        - is the GstElement which triggered the signal. 
        - In this example, it is the `uridecodebin`.
    
    `new_pad`
        - is the GstPad that has just been added to the `src` element.

    `data` 
        - is the pointer to the CustomData in this example.
    */

    GstPad *sink_pad = gst_element_get_static_pad (data->convert, "sink");
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked (sink_pad)) {
        g_print ("We are already linked. Ignoring.\n");
        goto exit;
    }

    /* Check the new pad's type */
    new_pad_caps = gst_pad_get_current_caps (new_pad);  
    /* 
    - get the types of data ("capabilities") this new pad is going to output.
    - All possible caps a pad can support can be queried with `gst_pad_query_caps()`.
    */
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    /*
    - A pad can offer many capabilities, and hence GstCaps can contain many GstStructure.
    - Each GstStructure represents a single capability.
    - In this case, we know that the pad will only offer one capability, so we get the first one.
    */
    new_pad_type = gst_structure_get_name (new_pad_struct);
    if (!g_str_has_prefix (new_pad_type, "audio/x-raw")) {
        g_print ("It has type '%s' which is not raw audio. Ignoring.\n", new_pad_type);
        goto exit;
    }
    // If the name is not audio/x-raw, this is not a decoded audio pad, and we are not interested in it.

    /* Attempt the link */
    ret = gst_pad_link (new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED (ret)) {
        g_print ("Type is '%s' but link failed.\n", new_pad_type);
    } else {
        g_print ("Link succeeded (type '%s').\n", new_pad_type);
    }

    exit:
        /* Unreference the new pad's caps, if we got them */
        if (new_pad_caps != NULL)
        gst_caps_unref (new_pad_caps);

        /* Unreference the sink pad */
        gst_object_unref (sink_pad);
}