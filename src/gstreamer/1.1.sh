# use playbin
gst-launch-1.0 playbin uri=file:///Users/user/Downloads/hello-gstreamer/assets/video_1.mp4
gst-launch-1.0 playbin uri=rtsp://admin:12345@192.168.3.27/live

# chia nhỏ element
gst-launch-1.0 filesrc location=assets/video_1.mp4 ! decodebin ! videoconvert ! autovideosink
gst-launch-1.0 rtspsrc location=rtsp://admin:12345@192.168.3.27/live ! decodebin ! videoconvert ! autovideosink

# xuất ra file
gst-launch-1.0 filesrc location=assets/video_1.mp4 ! decodebin ! videoconvert ! x264enc ! mp4mux ! filesink location=outputs/output.mp4
gst-launch-1.0 -e rtspsrc location=rtsp://admin:12345@192.168.3.27/live ! decodebin ! videoconvert ! x264enc ! mp4mux ! filesink location=outputs/output.mp4
# Bấm Ctrl + C bình thường sẽ giết ngay tiến trình. `mp4mux` chưa kịp đóng file và chưa ghi `moov atom` (chứa thời lượng, danh sách khung hình, codec...) vào file MP4.
# cờ -e chặn Ctrl + C, gửi sự kiện EOS chạy hết pipeline để mp4mux hoàn tất ghi header rồi mới thoát.

# decodebin: nhận vào mọi loại dữ liệu (MP4, MKV, AVI, luồng RTSP...) và trả ra các khung hình/âm thanh "thô"
# videoconvert: Vì "thô" cũng có nhiều định dạng khác nhau. Hầu hết các decoder xuất ra hình ảnh ở hệ màu YUV, trong khi hầu hết màn hình chỉ nhận RGB. videoconvert chuyển đổi giữa các hệ màu.
# x264enc: Nén các frame hình ảnh thô thành chuẩn H.264.
# mp4mux: Đóng gói luồng H.264 vào định dạng vỏ MP4

# xuất ra rtsp
wget https://github.com/bluenviron/mediamtx/releases/download/v1.21.0/mediamtx_v1.21.0_darwin_amd64.tar.gz
tar -xvf mediamtx_v1.21.0_darwin_amd64.tar.gz 
gst-launch-1.0 -e rtspsrc location=rtsp://admin:12345@192.168.3.27/live ! decodebin ! videoconvert ! x264enc ! flvmux ! rtmp2sink location=rtmp://localhost:1935/live