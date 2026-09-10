
# Tạo element

```C
GstElement *
gst_element_factory_make(
    const gchar * factoryname,  // loại element
    const gchar * name          // Tên ta tự đặt cho element (có thể NULL nếu không muốn đặt tên)
)
```

Một số phương thức khác có thể hữu ích:
* `gst_element_factory_find`: tìm factory của element.
* `gst_element_factory_create`: tạo element từ factory.
    
    `gst_element_factory_make` = `gst_element_factory_find` + `gst_element_factory_create`
* `gst_element_factory_create_full`: tạo element từ factory và truyền luôn các `properties` ngay lúc khởi tạo.

Tham khảo:
* [GstElementFactory](https://gstreamer.freedesktop.org/documentation/gstreamer/gstelementfactory.html?gi-language=c)


# Tạo pipeline

```C
GstElement *                // mặc dù là pipeline nhưng vẫn trả về kiểu GstElement
gst_pipeline_new(
    const gchar * name      // Tên ta tự đặt cho pipeline
)
```

Giải phóng pipeline khởi bộ nhớ sau khi sử dụng xong bằng `gst_object_unref`.
    * Tất cả các element con của pipeline cũng sẽ được giải phóng theo.


Tham khảo:
* [GstPipeline](https://gstreamer.freedesktop.org/documentation/gstreamer/gstpipeline.html?gi-language=c)


# Thêm element vào bin

Nhắc lại:
* `GstPipeline` cũng là một `GstBin`.

```C
gboolean
gst_bin_add(
    GstBin * bin,
    GstElement * element
)
```

Thêm đồng thời nhiều element vào 1 bin:

```C
gst_bin_add_many(
    GstBin * bin,
    GstElement * element_1,
    ... ...                     // Các element khác, kết thúc bằng NULL
)
```

Tham khảo:
* [GstBin](https://gstreamer.freedesktop.org/documentation/gstreamer/gstbin.html?gi-language=c)


# Kết nối các element

Kết nối 1 chiều từ `src` sang `dest`:
```C
gboolean
gst_element_link(
    GstElement * src,
    GstElement * dest
)
```

Lưu ý:
* Các element phải thuộc cùng 1 bin trước khi kết nối.


# Thay đổi trạng thái của element

```C
GstStateChangeReturn
gst_element_set_state(
    GstElement * element,
    GstState state
)
```

Các trạng thái `GstState`:
```C
typedef enum {
  GST_STATE_VOID_PENDING        = 0,
  GST_STATE_NULL                = 1,
  GST_STATE_READY               = 2,
  GST_STATE_PAUSED              = 3,
  GST_STATE_PLAYING             = 4
} GstState;
```

Các giá trị trả về `GstStateChangeReturn`:
```C
typedef enum {
  GST_STATE_CHANGE_FAILURE             = 0,
  GST_STATE_CHANGE_SUCCESS             = 1,
  GST_STATE_CHANGE_ASYNC               = 2,
  GST_STATE_CHANGE_NO_PREROLL          = 3
} GstStateChangeReturn;
```


# Giải phóng đối tượng

```C
void
gst_object_unref(
    gpointer object
)
```

Tất cả các đối tượng trong GStreamer kế thừa từ `GstObject` đều được quản lý bộ nhớ thông qua cơ chế **Đếm tham chiếu (Reference Counting)**:
* `gst_object_ref(object)`: Tăng số lượng tham chiếu (`ref_count += 1`).
* `gst_object_unref(object)`: Giảm số lượng tham chiếu (`ref_count -= 1`).
* Khi `ref_count` giảm về `0`: Vùng nhớ của đối tượng mới thực sự được giải phóng khỏi RAM (`free`).

**Trường hợp của Bus, Pad**:
```C
pipeline = gst_pipeline_new("test-pipeline");       // Khi pipeline được tạo ra, bản thân pipeline đã giữ 1 tham chiếu đến bus của nó (ref_count = 1)
bus = gst_element_get_bus(pipeline);                // ngầm gọi gst_object_ref(bus) để tăng số ref_count = 2
// ...
gst_object_unref(bus);                              // Phải tự unref bus ở đây vì ...
gst_object_unref(pipeline);                         // ... pipeline chỉ giải phóng 1 tham chiếu mà nó nắm giữ đối với bus
```

> [!NOTE]
> Trong tài liệu của GStreamer, tất cả các hàm dạng `gst_*_get_*()` trả về con trỏ `GstObject*` đều tăng `ref_count` (transfer full). Người gọi hàm có trách nhiệm phải gọi `gst_object_unref()` khi dùng xong.

**Trường hợp của Element**:
```C
source = gst_element_factory_make("videotestsrc", "source");    // Element này mang một trạng thái đặc biệt gọi là Floating Reference (ref_count = 1, nhưng ở trạng thái "trôi nổi")
gst_bin_add(GST_BIN(pipeline), source);                         // pipeline tự động gọi gst_object_ref_sink(source) để chuyển quyền sở hữu cho pipeline, ref_count = 1
// ...
gst_object_unref(pipeline);                                     // pipeline tự động gọi gst_object_unref(source) để giảm ref_count = 0, source được giải phóng
```

Mặc định CHỈ CÓ MỖI CON SỐ `ref_count`, đối tượng hoàn toàn KHÔNG hề biết ai (hàm nào, biến nào, luồng nào) đang trỏ vào nó! Bên trong struct `GObject`, trường này chỉ đơn giản là một số nguyên.
* Khi bạn gọi:
  * `gst_object_ref(obj)`: CPU chỉ thực hiện đúng một lệnh: `ref_count++`.
  * `gst_object_unref(obj)`: CPU chỉ thực hiện `ref_count--`. Nếu kết quả bằng `0` thì gọi hàm hủy `free()`.

Ngoài `ref_count`, mỗi đối tượng còn có thêm 1 lá cờ trạng thái tên là `FLOATING`. Hàm `gst_object_ref_sink()` thực chất chạy logic như sau:
```C
gpointer gst_object_ref_sink (gpointer object) {
    // 1. Kiểm tra xem đối tượng có đang mang cờ FLOATING hay không?
    if (GST_OBJECT_IS_FLOATING (object)) {
        
        // NẾU CÓ: "Đánh chìm phao" (Gỡ bỏ cờ FLOATING)
        GST_OBJECT_FLAG_UNSET (object, GST_OBJECT_FLAG_FLOATING);
        
        // GIỮ NGUYÊN ref_count = 1, KHÔNG TĂNG!
        return object; 
    }

    // 2. NẾU KHÔNG (đối tượng đã có chủ từ trước):
    // Tăng ref_count lên 1 như hàm ref bình thường
    return gst_object_ref (object);
}
```

# Tương tác với Bus

## Nhóm phương thức đồng bộ (Blocking / Polling)


```C
GstMessage *
gst_bus_timed_pop_filtered(
    GstBus * bus,
    GstClockTime timeout,
    GstMessageType types
)
```
"Chặn" (block) chương trình lại và chờ cho đến khi có một thông điệp thuộc loại bạn quan tâm (ví dụ: `GST_MESSAGE_ERROR` hoặc `GST_MESSAGE_EOS`) xuất hiện trên Bus, hoặc cho đến khi hết thời gian chờ (`timeout`).

### `gst_bus_pop`

```C
GstMessage *
gst_bus_pop (GstBus * bus)
```
Lấy thông điệp đầu tiên ra khỏi hàng đợi của Bus. Hàm này trả về ngay lập tức; nếu không có thông điệp nào, nó trả về NULL. Thường dùng với `while` để kiểm tra Bus liên tục.

```C
GstMessage *
gst_bus_peek (GstBus * bus)
```
Hoạt động giống `pop`, nhưng nó chỉ "nhìn trộm" thông điệp đầu tiên mà không rút nó ra khỏi hàng đợi.

## Nhóm phương thức bất đồng bộ (Asynchronous)

Để cả hai hàm sau hoạt động, ứng dụng của bạn bắt buộc phải đang chạy một vòng lặp sự kiện (thường là GLib Main Loop, hoặc vòng lặp của GTK/Qt).

```C
gboolean
gst_bus_add_watch (
    GstBus * bus,
    GstMessage * message,
    gpointer user_data
)
```
Đăng ký callback và gọi mỗi khi bus nhận được 1 thông điệp, bất kể thông điệp đó là gì (lỗi, cảnh báo, đổi trạng thái, hết video...).
* Hàm callback của bạn phải trả về `TRUE` (hoặc `G_SOURCE_CONTINUE`) để tiếp tục theo dõi Bus. Nếu trả về `FALSE` (hoặc `G_SOURCE_REMOVE`), GStreamer sẽ tự động hủy việc theo dõi.

```C
gst_bus_add_signal_watch (GstBus * bus)
```
Thay vì ép bạn hứng mọi thứ vào một chỗ như `gst_bus_add_watch`, nó sẽ tự động phân loại thông điệp và "bắn" ra các tín hiệu (signal) riêng biệt. Khi đó, bạn có thể chỉ định callback riêng biệt cho từng loại sự kiện thông qua hàm `g_signal_connect`.

VD:
```C
// Chia nhỏ thành các hàm xử lý riêng biệt
static void on_error_cb (GstBus *bus, GstMessage *msg, gpointer data) {
    // Chỉ tập trung xử lý lỗi
}

static void on_eos_cb (GstBus *bus, GstMessage *msg, gpointer data) {
    // Chỉ tập trung xử lý kết thúc luồng
}

// Cách gọi:
gst_bus_add_signal_watch (bus); // 1. Bật chế độ signal

// 2. Nối từng tín hiệu với từng hàm tương ứng
g_signal_connect (bus, "message::error", G_CALLBACK (on_error_cb), user_data);
g_signal_connect (bus, "message::eos", G_CALLBACK (on_eos_cb), user_data);
```

# Liên kết Pad

```C
GstPadLinkReturn
gst_pad_link (
    GstPad * srcpad,
    GstPad * sinkpad
)
```
Cắm một cổng `src` vào một cổng `sink`. Hàm này trả về mã lỗi rất chi tiết (`GstPadLinkReturn`) để bạn biết chính xác tại sao nối thất bại.

```C
gboolean
gst_pad_is_linked (GstPad * pad)
```
Kiểm tra xem cổng này đã được liên kết hay chưa.

```C
GstPad *
gst_pad_get_peer (GstPad * pad)
```
Lấy ra Pad đang được nối ở đầu bên kia.

Một số phương thức khác có thể hữu ích:
* `gst_pad_unlink`

Tham khảo:
* [GstPad](https://gstreamer.freedesktop.org/documentation/gstreamer/gstpad.html?gi-language=c)

# Kiểm tra Caps của Pad

```C
GstCaps *
gst_pad_query_caps (
    GstPad * pad,
    GstCaps * filter)
```
Trả về danh sách tất cả các định dạng mà Pad này hỗ trợ.

```C
GstCaps *
gst_pad_get_current_caps (GstPad * pad)
```
Lấy ra định dạng (Caps) đang được Pad sử dụng thực tế ngay lúc này. 1 Pad tại một thời điểm chỉ có thể mang đúng 1 định dạng duy nhất, do đó mảng GstCaps mà `gst_pad_get_current_caps` trả về chỉ chứa đúng 1 `GstStructure` duy nhất.

```C
GstStructure *
gst_caps_get_structure (
    const GstCaps * caps,
    guint index
)
```
* Một đối tượng `GstCaps` thực chất là một mảng chứa một hoặc nhiều `GstStructure`. 
* Mỗi `GstStructure` lưu các cặp key-value mô tả một định dạng cụ thể (ví dụ: `audio/x-raw`, `format=S16LE`, `rate=44100`, `channels=2`). 
    * Sau khi lấy được cấu trúc này, bạn mới có thể gọi các hàm như `gst_structure_get_int()` để lấy ra con số chiều rộng (width) hoặc chiều cao (height) của video, hay `gst_structure_get_name()` để lấy tên.

# Gắn callback vào Pad

```C
gulong
gst_pad_add_probe (
    GstPad * pad,
    GstPadProbeType mask,
    GstPadProbeCallback callback,
    gpointer user_data,
    GDestroyNotify destroy_data
)
```
Gắn và kích hoạt hàm callback mỗi khi dòng dữ liệu đi qua Pad ở trạng thái cụ thể (probe type).

Một số phương thức khác có thể hữu ích:
* `gst_pad_remove_probe`

