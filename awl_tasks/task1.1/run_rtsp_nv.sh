IN_RTSP=rtsp://admin:12345@192.168.3.27/live
OUT_DIR=outputs

mkdir -p $OUT_DIR

# Có decode/encode
gst-launch-1.0 -e rtspsrc location=$IN_RTSP ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvideoconvert ! nvv4l2h264enc ! h264parse ! mp4mux ! filesink location=$OUT_DIR/output_rtsp.mp4

