
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
    gpointer object         // Đối tượng kế thừa từ GstObject cần giảm tham chiếu
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
source = gst_element_factory_make("videotestsrc", "source");    // ref_count = 1
gst_bin_add(GST_BIN(pipeline), source);                         // pipeline tự động gọi gst_object_ref_sink(source) để chuyển quyền sở hữu cho pipeline, ref_count = 1
// ...
gst_object_unref(pipeline);                                     // pipeline tự động gọi gst_object_unref(source) để giảm ref_count = 0, source được giải phóng
```

> [!NOTE]
> Mặc định CHỈ CÓ MỖI CON SỐ `ref_count`, đối tượng hoàn toàn KHÔNG hề biết ai (hàm nào, biến nào, luồng nào) đang trỏ vào nó! Bên trong struct `GObject`, trường này chỉ đơn giản là một số nguyên 32-bit.
> * Khi bạn gọi:
>   * `gst_object_ref(obj)`: CPU chỉ thực hiện đúng một lệnh nguyên tử (atomic): `ref_count++`.
>   * `gst_object_unref(obj)`: CPU chỉ thực hiện `ref_count--`. Nếu kết quả bằng `0` thì gọi hàm hủy `free()`.