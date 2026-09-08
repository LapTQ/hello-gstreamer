mkdir -p outputs

# xuất ra file
gst-launch-1.0 filesrc location=assets/video_1.mp4 ! qtdemux ! h264parse ! nvh264dec ! videoconvert ! x264enc ! h264parse ! mp4mux ! filesink location=outputs/output.mp4
gst-launch-1.0 -e rtspsrc location=rtsp://admin:12345@192.168.3.27/live ! rtph264depay ! h264parse ! mp4mux ! filesink location=outputs/output.mp4
