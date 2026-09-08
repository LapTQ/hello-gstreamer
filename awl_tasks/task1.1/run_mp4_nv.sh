IN_FILE=/home/laptq/hello-gstreamer/assets/video_1.mp4
OUT_DIR=outputs

mkdir -p $OUT_DIR

gst-launch-1.0 filesrc location=$IN_FILE ! qtdemux ! h264parse ! nvv4l2decoder ! nvvideoconvert ! nvv4l2h264enc ! h264parse ! mp4mux ! filesink location=$OUT_DIR/output.mp4

# Quy trình:
# 1. filesrc: đọc file -> luồng byte nhị phân thô. Không biết là video hay văn bản.
# 2. qtdemux: file MP4 đang trộn cả luồng hình ảnh (H.264), âm thanh (AAC), phụ đề,... (multiplexed). qtdemux tách lấy luồng hình ảnh.
# 3. h264parse: luồng H.264 từ qtdemux thường thiếu các metadata cần thiết (VD: mốc phân tách giữa các frame). h264parse chuẩn hóa luồng H.264 cho các bước sau.
# 4. nvv4l2decoder/avdec_h264: chuyển hình ảnh bị nén H.264 thành ảnh pixel "thô". Với nvv4l2decoder, khi bung nén xong, ảnh thô được giữ nguyên trên VRAM (chuẩn NVMM).
# 5. nvvideoconvert/videoconvert: hầu hết các decoder xuất ra ảnh "thô" ở hệ màu nào đó (VD: NV12, I420). nvvideoconvert/videoconvert chuyển đổi kênh màu/resize tùy theo yêu cầu (Caps) của element phía sau.
# 6. nvv4l2h264enc/x264enc: để lưu thành file, nếu lưu ảnh thô thì rất nặng. Element này nén lại thành chuẩn H.264. Với nvv4l2h264enc, sau khi nén xong, nó chuyển dữ liệu sang CPU.
# 7. h264parse: để lưu thành file, luồng H.264 phải được cấu trúc lại phù hợp với file MP4.
# 8. mp4mux: tạo bảng chỉ mục (moov atom), đóng gói thành cấu trúc file MP4,... xuất ra luồng byte 
# 9. filesink: ghi luồng byte vào file.