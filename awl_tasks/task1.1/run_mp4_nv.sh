IN_FILE=/home/laptq/hello-gstreamer/assets/video_1.mp4
OUT_DIR=outputs

mkdir -p $OUT_DIR

gst-launch-1.0 filesrc location=$IN_FILE ! qtdemux ! h264parse ! nvv4l2decoder ! nvvideoconvert ! nvv4l2h264enc ! h264parse ! mp4mux ! filesink location=$OUT_DIR/output.mp4