IN_FILE=/home/laptq/hello-gstreamer/assets/video_1.mp4
OUT_DIR=outputs

mkdir -p $OUT_DIR

# use playbin
gst-launch-1.0 playbin uri=file://$IN_FILE

# chia nhỏ element
gst-launch-1.0 filesrc location=$IN_FILE ! decodebin ! videoconvert ! autovideosink
# ⚠️ lệnh này chạy trên Docker Image của NVIDIA sẽ bị lỗi.
# Khi NVIDIA đóng gói môi trường DeepStream, họ đã tinh chỉnh và "cắt xén" GStreamer
# để tối ưu dung lượng và tránh các rắc rối về giấy phép bản quyền phần mềm mã nguồn mở.
# Đó là lý do GStreamer trong container này hoàn toàn không biết x264enc (nén video bằng CPU) 
# hay MPEG-4 AAC decoder (giải mã âm thanh bằng CPU) là gì.

# xuất ra file
gst-launch-1.0 -e filesrc location=$IN_FILE ! decodebin ! videoconvert ! x264enc ! mp4mux ! filesink location=$OUT_DIR/output.mp4
# Bấm Ctrl + C bình thường sẽ giết ngay tiến trình. `mp4mux` chưa kịp đóng file và chưa ghi `moov atom` (chứa thời lượng, danh sách khung hình, codec...) vào file MP4.
# cờ -e chặn Ctrl + C, gửi sự kiện EOS chạy hết pipeline để mp4mux hoàn tất ghi header rồi mới thoát.

# decodebin: nhận vào mọi loại dữ liệu (MP4, MKV, AVI, luồng RTSP...) và trả ra các khung hình/âm thanh "thô"
# videoconvert: Vì "thô" cũng có nhiều định dạng khác nhau. Hầu hết các decoder xuất ra hình ảnh ở hệ màu YUV, trong khi hầu hết màn hình chỉ nhận RGB. videoconvert chuyển đổi giữa các hệ màu.
# x264enc: Nén các frame hình ảnh thô thành chuẩn H.264.
# mp4mux: Đóng gói luồng H.264 vào định dạng vỏ MP4