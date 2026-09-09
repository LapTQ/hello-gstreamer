# luồng RTSP thật
IN_RTSP=rtsp://admin:12345@192.168.3.27/live
# hoặc nếu giả lập từ file với mediamtx
IN_FILE=assets/video_1.mp4
RTSP_IN_LINK=rtmp://localhost:1935/live/stream
IN_RTSP=rtsp://localhost:8554/live/stream
PROTOCOLS="protocols=tcp"
gst-launch-1.0 filesrc location=$IN_FILE ! qtdemux ! h264parse ! flvmux ! rtmp2sink location=$RTSP_IN_LINK

OUT_DIR=outputs

mkdir -p $OUT_DIR

# xuất ra màn hình
gst-launch-1.0 -e rtspsrc location=$IN_RTSP $PROTOCOLS ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvideoconvert ! nveglglessink

# xuất ra file, nếu muốn decode/encode
gst-launch-1.0 -e rtspsrc location=$IN_RTSP $PROTOCOLS ! rtph264depay ! h264parse ! nvv4l2decoder ! nvvideoconvert ! nvv4l2h264enc ! h264parse ! mp4mux ! filesink location=$OUT_DIR/output_rtsp.mp4

