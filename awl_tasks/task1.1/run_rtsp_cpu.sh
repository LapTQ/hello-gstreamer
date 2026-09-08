IN_RTSP=rtsp://admin:12345@192.168.3.27/live
OUT_DIR=outputs

mkdir -p $OUT_DIR

# use playbin
gst-launch-1.0 playbin uri=$IN_RTSP

# chia nhỏ element
gst-launch-1.0 rtspsrc location=$IN_RTSP ! decodebin ! videoconvert ! autovideosink

# xuất ra file
gst-launch-1.0 -e rtspsrc location=$IN_RTSP ! decodebin ! videoconvert ! x264enc ! mp4mux ! filesink location=$OUT_DIR/output.mp4

# Bỏ qua bước decode/encode
gst-launch-1.0 -e rtspsrc location=$IN_RTSP ! rtph264depay ! h264parse ! mp4mux ! filesink location=$OUT_DIR/output_rtsp.mp4
