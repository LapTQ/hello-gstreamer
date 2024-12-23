/* https://chirag4798.medium.com/nvidia-deepstream-101-a-step-by-step-guide-to-creating-your-first-deepstream-application-68148753cf96

filesrc -> qtdemux -> h264parse -> nvv4l2decoder -> nvstreammux -> nvinfer -> nvtracker -> nvvideoconvert -> nvdsosd -> nvvideoconvert -> nvv4l2h264enc -> h264parse -> qtmux -> filesink

- filesrc: read in the input mp4 file.
- qtdemux: demultiplex the audio and video streams to process them separately.
- h264parse: parse the H.264 video stream. It extracts information such as frame rate and resolution.
- nvv4l2decoder: decode the H.264 video stream. It converts the compressed video into raw video frames.
- nvstreammux: multiplex the video stream. It forms batches.
- nvinfer: run the object detection model.
- nvtracker: track object.
- nvvideoconvert: convert the video frames format between Nv12 and RGBA.
- nvdsosd: draws the bounding boxes and labels on the frames.
- nvv4l2h264enc: encode the video frames into H.264 format.
- h264parse: parse the H.264 video stream.
- qtmux: multiplex the video and audio streams into an mp4 file.
- filesink: write the output mp4 file to a location on the system.

*/


#include <gst/gst.h>
#include <glib.h>
#include <stdio.h>
#include <cuda_runtime_api.h>
#include "gstnvdsmeta.h"


#define MUXER_OUTPUT_WIDTH 1920
#define MUXER_OUTPUT_HEIGHT 1080

/* Muxer batch formation timeout, for e.g. 40 millisec. Should ideally be set
 * based on the fastest source's framerate. */
#define MUXER_BATCH_TIMEOUT_USEC 40000

static gboolean bus_call (GstBus * bus, GstMessage * msg, gpointer data){
  GMainLoop *loop = (GMainLoop *) data;
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *error;
      gst_message_parse_error (msg, &error, &debug);
      g_printerr ("ERROR from element %s: %s\n",
          GST_OBJECT_NAME (msg->src), error->message);
      if (debug)
        g_printerr ("Error details: %s\n", debug);
      g_free (debug);
      g_error_free (error);
      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }
  return TRUE;
}

void cb_new_pad (GstElement *qtdemux, GstPad* pad, gpointer data) {
  GstElement* h264parser = (GstElement*) data;
  gchar *name = gst_pad_get_name (pad);
  if (strcmp (name, "video_0") == 0 && 
      !gst_element_link_pads(qtdemux, name, h264parser, "sink")){
    g_printerr ("Could not link %s pad of qtdemux to sink pad of h264parser", name);
  }
}

int main (int argc, char *argv[]){
  GMainLoop *loop = NULL;
  GstElement    *pipeline = NULL, 
                *source = NULL, 
                *qtdemux = NULL,
                *h264parser = NULL, 
                *nvv4l2decoder = NULL, 
                *streammux = NULL, 
                *pgie = NULL, 
                *tracker = NULL, 
                *nvvidconv = NULL, 
                *nvosd = NULL, 
                *nvvidconv2 = NULL, 
                *nvv4l2h264enc = NULL, 
                *h264parser2 = NULL,
                *qtmux = NULL,
                *sink = NULL; 

  GstElement *transform = NULL;
  GstBus *bus = NULL;
  guint bus_watch_id;

  /* Check input arguments */
  if (argc != 2) {
    g_printerr ("Usage: %s </path/to/input/video.mp4>\n", argv[0]);
    return -1;
  }

  // We need to initialise GStreamer with gst_init.
  // The pipeline starts by creating a GMainLoop, which is the main event loop.
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);

  pipeline = gst_pipeline_new ("deepstream_tutorial_app1");

  // The pipeline elements are created using the gst_element_factory_make
  // See: https://gstreamer.freedesktop.org/documentation/gstreamer/gstelementfactory.html?gi-language=python
  source        = gst_element_factory_make ("filesrc", "file-source");
  qtdemux       = gst_element_factory_make ("qtdemux", "qtdemux");
  h264parser    = gst_element_factory_make ("h264parse", "h264-parser");
  nvv4l2decoder = gst_element_factory_make ("nvv4l2decoder", "nvv4l2-decoder");
  streammux     = gst_element_factory_make ("nvstreammux", "stream-muxer");
  pgie          = gst_element_factory_make ("nvinfer", "primary-nvinference-engine");
  tracker       = gst_element_factory_make ("nvtracker", "tracker");
  nvvidconv     = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter");
  nvosd         = gst_element_factory_make ("nvdsosd", "nv-onscreendisplay");
  nvvidconv2    = gst_element_factory_make ("nvvideoconvert", "nvvideo-converter2");
  nvv4l2h264enc = gst_element_factory_make ("nvv4l2h264enc", "nvv4l2h264enc");
  h264parser2   = gst_element_factory_make ("h264parse", "h264parser2");
  qtmux         = gst_element_factory_make ("qtmux", "qtmux");
  sink          = gst_element_factory_make ("filesink", "filesink");

  if (!pipeline || !source || !h264parser || !qtdemux ||
      !nvv4l2decoder || !streammux || !pgie || !tracker || 
      !nvvidconv || !nvosd || !nvvidconv2 || !nvv4l2h264enc || 
      !h264parser2 || !qtmux || !sink) {
    g_printerr ("One element could not be created. Exiting.\n");
    return -1;
  }


  // We need to set properties to the element using g_object_set function
  // See: https://docs.gtk.org/gobject/method.Object.set.html
  g_object_set (
    G_OBJECT (source), // See: https://gstreamer.freedesktop.org/documentation/coreelements/filesrc.html?gi-language=python or try `gst-inspect-1.0 filesrc`
    "location", argv[1],    // path to video file
    NULL
  ); 

  g_object_set (
    G_OBJECT (streammux),  // See: https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvstreammux.html
    "batch-size", 1, 
    "width", MUXER_OUTPUT_WIDTH, 
    "height", MUXER_OUTPUT_HEIGHT,
    "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, 
    NULL
  );

  g_object_set (
    G_OBJECT (pgie),
    "config-file-path", "/opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test1/dstest1_pgie_config.txt", 
    NULL
  );

  g_object_set (
    G_OBJECT (tracker),
    "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so", 
    NULL
  );

  g_object_set (
    G_OBJECT (sink),
    "location", "output.mp4", 
    NULL
  );

  /* we add a message handler */
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);


  // Before linking the elements together, the elements need to be added to the same GST_BIN.
  
  /* Not all elements in pipeline can be linked together with `gst_link_many` function.

  For example, with qtdemux:
  * the video_%u pad and audio_%u pad are only created when the audio and video streams are detected by the qtdemux.
  * Once the audio and video streams are detected, the qtdemux will
    * create the video_%u pad and audio_%u pad.
    * call the callback function which is set for the "pad-added" signal. See  `gst-inspect-1.0 qtdemux`.
        * in this case, the callback function is `cb_new_pad`. The function will be called every time a new pad is created.
        * we will use `g_signal_connect` to connect the signal to the callback function. (See: https://docs.gtk.org/gobject/func.signal_connect.html)
  
  * In our `cb_new_pad`, we will Link "video_0" pad of qtdemux to sink pad of h264Parse.
  */
  g_signal_connect (qtdemux, "pad-added", G_CALLBACK (cb_new_pad), h264parser);

  gst_bin_add_many (
    GST_BIN (pipeline),
    source,
    qtdemux,
    h264parser,
    nvv4l2decoder,
    streammux, 
    pgie,
    tracker,
    nvvidconv, 
    nvosd,
    nvvidconv2,
    nvv4l2h264enc,
    h264parser2,
    qtmux,
    sink, 
    NULL
  );
  
  /* 
  With nvstreammux:
  * sink_%u pad is only created on request.

  Therefore, we need to request the sink pad from the streammux and link it to the src pad of the nvv4l2decoder.
  */
  GstPad *sinkpad, *srcpad;
  gchar pad_name_src[16] = "src";
  gchar pad_name_sink[16] = "sink_0";

  srcpad = gst_element_get_static_pad (nvv4l2decoder, pad_name_src); // Static pad
  if (!srcpad) {
    g_printerr ("Decoder request src pad failed. Exiting.\n");
    return -1;
  }

  sinkpad = gst_element_get_request_pad (streammux, pad_name_sink); // Dynamic pad
  if (!sinkpad) {
    g_printerr ("Streammux request sink pad failed. Exiting.\n");
    return -1;
  }

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {  // Link the pads
      g_printerr ("Failed to link decoder to stream muxer. Exiting.\n");
      return -1;
  }

  /* Unreference the object */
  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  /* 
   * we link the elements together
   * file-source -> qtdemux -> h264-parser -> nvh264-decoder ->
   * nvinfer -> tracker -> nvvidconv -> nvosd -> nvvidconv2 -> 
   * nvh264-encoder -> qtmux -> filesink 
  */
  if (!gst_element_link_many (source, qtdemux, NULL)) {
    g_printerr ("Source and QTDemux could not be linked: 1. Exiting.\n");
    return -1;
  }

  if (!gst_element_link_many (h264parser, nvv4l2decoder, NULL)) {
    g_printerr ("H264Parse and NvV4l2-Decoder could not be linked: 2. Exiting.\n");
    return -1;
  }

  if (!gst_element_link_many (streammux, pgie, tracker, nvvidconv, nvosd, nvvidconv2, nvv4l2h264enc, h264parser2, qtmux, sink, NULL)) {
    g_printerr ("Rest of the pipeline elements could not be linked: 3. Exiting.\n");
    return -1;
  }

  /* Set the pipeline to "playing" state */
  g_print ("Using file: %s\n", argv[1]);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait till pipeline encounters an error or EOS */
  g_print ("Running...\n");
  g_main_loop_run (loop);

  /* Out of the main loop, clean up nicely */
  g_print ("Returned, stopping playback\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  g_print ("Deleting pipeline\n");
  gst_object_unref (GST_OBJECT (pipeline));
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}