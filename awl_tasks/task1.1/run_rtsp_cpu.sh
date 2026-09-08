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

# Quy trình:
# (đọc task1.1/run_mp4_nv.sh trước)
# rtspsrc: 1 hình ảnh H.264 truyền qua mạng bị băm nhỏ và nhồi vào hàng vạn gói tin RTP để truyền qua mạng. rtspsrc trả về dòng các gói tin RTP này (application/x-rtp)
# rtph264depay: trích xuất phần H.264 trong gói RTP, gom các mẩu H.264 bị băm lại thành 1 khung hình H.264 hoàn chỉnh, xuất ra luồng H.264 (thường ở dạng byte-stream).
# ... phần còn lại tương tự như trong ghi chú task1.1/run_mp4_nv.sh.