IN_FILE=/home/laptq/hello-gstreamer/assets/video_1.mp4
OUT_DIR=outputs

mkdir -p $OUT_DIR

# xuất ra màn hình
SINK=nveglglessink # DGPU x86
SINK=nv3dsink      # DGPU arm64 hoặc IGPU
gst-launch-1.0 filesrc location=$IN_FILE ! qtdemux ! h264parse ! nvv4l2decoder ! nvvideoconvert ! $SINK

# xuất ra file
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

# `nvv4l2h264enc -> h264parse -> mp4mux` và `x264enc -> mp4mux`
# Tại sao x264enc không cần h264parse như nvv4l2h264enc?
# Câu trả lời nằm ở khả năng hỗ trợ định dạng đầu ra của 2 bộ nén (Encoder) này.
# - chuẩn nén H.264 có 2 "phong cách" sắp xếp dữ liệu (gọi là stream-format):
#   + byte-stream: Sắp xếp dữ liệu thành một chuỗi liên tục, các khung hình được ngăn cách bằng một đoạn mã vạch (start codes, ví dụ 00 00 00 01). Kiểu này rất thô, thích hợp để bắn qua mạng (RTSP) hoặc đẩy thẳng vào chip phần cứng.
#   + avc: Gọn gàng hơn. Thông tin mô tả môi trường (SPS/PPS) được gom lại để lên đầu, các khung hình được gắn "thẻ độ dài" ở trước
# - `mp4mux` chỉ nhận H.264 dạng avc.
# - `x264enc` có thể xuất ra cả 2 định dạng byte-stream hoặc avc.
# - `nvv4l2h264enc` chỉ xuất ra duy nhất luồng H.264 thô ở dạng byte-stream.
# => h264parse cần đứng ở giữa nvv4l2h264enc để chuyển luồng dạng byte-stream sang dạng avc.