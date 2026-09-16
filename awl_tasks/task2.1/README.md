# NVMM và Zero-Copy

## NVMM

- Trong Gstreamer, một `GstBuffer` chứa con trỏ tới vùng nhớ được cấp phát trên RAM hệ thống.
- NVMM là một tính năng được bổ sung vào để dữ liệu được cấp phát trên VRAM của GPU. VD: khi nhìn thấy:
    ```
    video/x-raw(memory:NVMM)
    ```
    tức là dữ liệu video nằm trên VRAM của GPU.
- Một GstBuffer mang cờ `memory:NVMM` thực chất chỉ chứa con trỏ tới cấu trúc `NvBufSurface`. Cấu trúc này chứa con trỏ với vùng nhớ trên VRAM và các metadata khác.

## Zero-Copy

- Cơ chế Zero-Copy: dữ liệu không bị copy qua lại giữa CPU và GPU khi chúng qua tay các element.
- VD: luồng `h264parse (CPU) -> nvv4l2decoder (GPU) -> nvvideoconvert (GPU) -> nvv4l2h264enc (GPU) -> h264parse (CPU)`
    - quá trình `nvv4l2decoder -> nvvideoconvert -> nvv4l2h264enc` diễn ra hoàn toàn trên GPU mà không bị copy qua lại giữa CPU và GPU.

## Tại sao thay `nvvideoconvert` bằng `videoconvert` lại làm giảm hiệu năng?

- `videoconvert` được viết để làm việc hoàn toàn trên CPU.
- Khi đặt `videoconvert` vào pipeline thay cho `nvvideoconvert`:
    1. GStreamer phải cấp phát bộ nhớ trên RAM CPU, copy dữ liệu từ VRAM về RAM CPU => lãng phí tài nguyên nếu dùng card rời.
    2. `videoconvert` tính toán trên CPU => chậm hơn trên GPU.
    3. Gstreamer phải cấp phát bộ nhớ trên VRAM, copy dữ liệu từ RAM CPU lên VRAM => lãng phí tài nguyên nếu dùng card rời.