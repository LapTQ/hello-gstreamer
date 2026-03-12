# DeepStream Secondary Preprocess: Vấn Đề Thường Gặp & Giải Pháp

> **Mục đích**: Ghi chép lại các vấn đề đã gặp khi debug secondary preprocess trong DeepStream pipeline.

---


## 1. `attrList` / `attrString` nằm ở đâu trong Metadata Hierarchy?

Hàm `custom_pose_classify_infer_parse` (hoặc bất kỳ `parse-classifier-func-name` nào) trả kết quả qua 2 tham số output:
- `attrList` (`std::vector<NvDsInferAttribute>&`) — danh sách attributes (class, confidence, label)
- `attrString` (`std::string&`) — chuỗi label tổng hợp

**nvinfer tự động** lấy kết quả này, tạo `NvDsClassifierMeta` + `NvDsLabelInfo`, rồi attach vào `obj_meta->classifier_meta_list` của object tương ứng.

### Vị trí trong hierarchy

```
NvDsObjectMeta (per detection)
├── class_id, object_id, confidence
├── rect_params (bbox)
├── obj_label
├── classifier_meta_list (GList*)    ← KẾT QUẢ NẰM Ở ĐÂY
│   └── NvDsClassifierMeta
│       ├── unique_component_id = 7  (= gie-unique-id của SGIE)
│       └── label_info_list (GList*)
│           └── NvDsLabelInfo
│               ├── result_class_id   = attr.attributeValue   (e.g. 5)
│               ├── result_label      = attr.attributeLabel    ("hand reach out")
│               └── result_prob       = attr.attributeConfidence (0.85)
└── obj_user_meta_list
```

### Mapping: parse function output → metadata

| `NvDsInferAttribute` field | → | `NvDsLabelInfo` field |
|---|---|---|
| `attr.attributeValue` | → | `label_info->result_class_id` |
| `attr.attributeLabel` | → | `label_info->result_label` |
| `attr.attributeConfidence` | → | `label_info->result_prob` |
| `attr.attributeIndex` | → | thuộc `NvDsClassifierMeta` nào (nếu có nhiều attr) |

### Cách đọc downstream (trong probe/callback)

```c
for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
    NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)l_obj->data;

    // Iterate qua classifier results
    for (NvDsMetaList *l_cls = obj_meta->classifier_meta_list; l_cls != NULL; l_cls = l_cls->next) {
        NvDsClassifierMeta *cls_meta = (NvDsClassifierMeta *)l_cls->data;
        
        // Lọc theo unique_component_id nếu có nhiều SGIEs
        if (cls_meta->unique_component_id == 7) {  // SGIE pose classification
            for (NvDsMetaList *l_label = cls_meta->label_info_list; l_label != NULL; l_label = l_label->next) {
                NvDsLabelInfo *label_info = (NvDsLabelInfo *)l_label->data;
                
                // ĐÂY LÀ KẾT QUẢ TỪ custom_pose_classify_infer_parse
                g_print("Action: %s (class=%d, conf=%.2f)\n",
                        label_info->result_label,       // = attrString
                        label_info->result_class_id,    // = attr.attributeValue
                        label_info->result_prob);        // = attr.attributeConfidence
            }
        }
    }
}
```

---

## 2. Nhiều SGIEs trên cùng 1 object

### Cấu hình

Mỗi SGIE classifier có `gie-unique-id` khác nhau và cùng `operate-on-gie-id` (trỏ về PGIE):

```yaml
# SGIE 1: Phân loại hành vi (pose classification)
secondary-gie3:
  gie-unique-id: 7
  operate-on-gie-id: 1        # operate on PGIE's detections
  config-file: action_recognition.yml

# SGIE 2: Phân loại quần áo
secondary-gie4:
  gie-unique-id: 8
  operate-on-gie-id: 1        # cùng operate on PGIE's detections
  config-file: clothing_classification.yml

# SGIE 3: Giới tính
secondary-gie5:
  gie-unique-id: 9
  operate-on-gie-id: 1
  config-file: gender_classification.yml
```

### Metadata hierarchy với nhiều SGIEs

Mỗi SGIE tạo một `NvDsClassifierMeta` riêng, **tất cả nằm trong cùng** `classifier_meta_list` của object:

```
NvDsObjectMeta (person #42)
├── classifier_meta_list (GList*)
│   ├── NvDsClassifierMeta  ← SGIE 7 (pose classification)
│   │   ├── unique_component_id = 7        ← PHÂN BIỆT BẰNG CÁI NÀY
│   │   └── label_info_list
│   │       └── NvDsLabelInfo { "hand reach out", class=7, conf=0.85 }
│   │
│   ├── NvDsClassifierMeta  ← SGIE 8 (clothing)
│   │   ├── unique_component_id = 8
│   │   └── label_info_list
│   │       └── NvDsLabelInfo { "red_jacket", class=2, conf=0.91 }
│   │
│   └── NvDsClassifierMeta  ← SGIE 9 (gender)
│       ├── unique_component_id = 9
│       └── label_info_list
│           └── NvDsLabelInfo { "male", class=0, conf=0.78 }
```

### Cách đọc kết quả từ nhiều SGIEs

**`unique_component_id`** chính là key để phân biệt kết quả đến từ SGIE nào:

```c
for (NvDsMetaList *l_cls = obj_meta->classifier_meta_list; 
     l_cls != NULL; l_cls = l_cls->next) {
    
    NvDsClassifierMeta *cls_meta = (NvDsClassifierMeta *)l_cls->data;
    NvDsLabelInfo *label = (NvDsLabelInfo *)cls_meta->label_info_list->data;

    switch (cls_meta->unique_component_id) {
        case 7:  // Pose classification SGIE
            g_print("Action: %s (%.2f)\n", label->result_label, label->result_prob);
            break;
        case 8:  // Clothing SGIE
            g_print("Clothing: %s (%.2f)\n", label->result_label, label->result_prob);
            break;
        case 9:  // Gender SGIE
            g_print("Gender: %s (%.2f)\n", label->result_label, label->result_prob);
            break;
    }
}
```

### Tóm tắt

| Câu hỏi | Trả lời |
|---------|---------|
| Nhiều SGIE trên cùng object? | Dùng cùng `operate-on-gie-id`, khác `gie-unique-id` |
| Kết quả lưu ở đâu? | Tất cả trong `obj_meta->classifier_meta_list` |
| Phân biệt bằng gì? | **`cls_meta->unique_component_id`** = `gie-unique-id` của SGIE tương ứng |

### Lưu ý về `network-type`

| `network-type` | Hành vi |
|---|---|
| `1` (Classifier) | nvinfer gọi `parse-classifier-func-name`, tự tạo `NvDsClassifierMeta` → attach vào `classifier_meta_list` |
| `100` (Other) | nvinfer **KHÔNG** tạo `NvDsClassifierMeta`. Nếu kết hợp `output-tensor-meta: 1`, raw tensor output được attach dưới dạng `NvDsInferTensorMeta` → phải tự parse trong probe downstream |

---

## 3. Cơ chế Block của Secondary Preprocess (TEE + wait_queue)

### Kiến trúc

Pipeline của secondary preprocess có kiến trúc TEE (chia nhánh):
```
                         ┌─→ queue → preprocess_0 → fakesink
Main pipeline → TEE ─────┤
                         ├─→ queue → preprocess_1 → fakesink  (nếu có nhiều preprocess)
                         │
                         └─→ wait_queue ──→ [main pipeline tiếp tục]
```
Cùng 1 GstBuffer được TEE chia ra cho nhiều nhánh cùng lúc. Mỗi nhánh preprocess xử lý song song (parallel) và ghi metadata vào buffer đó (tensor meta, preprocess batch meta...).

**Vấn đề**: Main pipeline (nhánh wait_queue) muốn tiếp tục gửi buffer downstream (tới SGIE, OSD, sink...), nhưng nếu nó gửi trước khi các nhánh preprocess xử lý xong, thì metadata chưa sẵn sàng → SGIE không có tensor để infer.

**Cách xử lý**: GStreamer dùng reference counting cho buffer. Khi TEE chia buffer ra N nhánh, refcount = N. Mỗi nhánh xử lý xong → giảm refcount đi 1.
- refcount > 1 → còn nhánh preprocess chưa xử lý xong → chờ
- refcount == 1 → tất cả nhánh song song đã xong, chỉ còn nhánh wait_queue giữ buffer → thả buffer đi tiếp

Source: `apps-common/deepstream_secondary_preprocess.c`

### Cơ chế đồng bộ: 

Probe `wait_queue_buf_probe` nằm trên src pad của `wait_queue` (nhánh main). Nó block main pipeline cho đến khi preprocess xong:

```c
// deepstream_secondary_preprocess.c — line 29-51
static GstPadProbeReturn
wait_queue_buf_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
    NvDsSecondaryPreProcessBin *bin = (NvDsSecondaryPreProcessBin *)u_data;

    if (info->type & GST_PAD_PROBE_TYPE_BUFFER) {
        g_mutex_lock(&bin->wait_lock);
        while (GST_OBJECT_REFCOUNT_VALUE(GST_BUFFER(info->data)) > 1
            && !bin->stop && !bin->flush) {
            gint64 end_time;
            end_time = g_get_monotonic_time() + G_TIME_SPAN_SECOND / 1000;
            g_cond_wait_until(&bin->wait_cond, &bin->wait_lock, end_time);
        }
        g_mutex_unlock(&bin->wait_lock);
    }

    return GST_PAD_PROBE_OK;
}
```

**Giải thích**:

| Bước | Mô tả |
|------|--------|
| 1 | TEE push **cùng 1 GstBuffer** vào cả 2 nhánh → `refcount = 2` |
| 2 | Nhánh preprocess: nhận buffer, nvdspreprocess bắt đầu xử lý |
| 3 | Nhánh wait_queue: probe trên src pad **block** — chờ `refcount == 1` |
| 4 | Preprocess xong → fakesink unref buffer → `refcount` giảm về 1 |
| 5 | Probe thấy `refcount == 1` → thả buffer → buffer đi downstream (tới SGIE, OSD,...) |

### Nhận định quan trọng

> **Buffer KHÔNG bị mất (drop)**. Kiến trúc TEE + wait_queue đảm bảo mọi buffer đều đi qua cả 2 nhánh. Probe chỉ **block** (trì hoãn) chứ không loại bỏ buffer.

- Queue giữa TEE và preprocess dùng default settings (max-size-buffers=200, **KHÔNG có leaky mode**).
- TEE có thể tiếp tục push buffer mới vào queue khi queue còn chỗ.
- Vì queue có buffer riêng (thread riêng cho src pad), nó có thể tích lũy nhiều buffer trong khi preprocess đang xử lý 1 buffer.

---

## 4. Bug: nvdspreprocess bỏ sót frames cùng source_id trong batch

### Hiện tượng

Khi chạy pipeline với 1 source video, `custom_prepare_tensor_pose_sequence` chỉ được gọi **mỗi 4 frames** thay vì mỗi frame:

```
[APP] frame=2  obj_id=0          ← callback chạy mỗi frame ✓
[APP] frame=3  obj_id=0
[PREPROCESS] frame=4  obj_id=0   ← preprocess chỉ mỗi 4 frame ✗
[APP] frame=4  obj_id=0
[APP] frame=5  obj_id=0
[APP] frame=6  obj_id=0
[APP] frame=7  obj_id=0
[PREPROCESS] frame=8  obj_id=0   ← mỗi 4 frame
```

### Root Cause

Trong `gstnvdspreprocess.cpp`, hàm `gst_nvdspreprocess_on_objects()` (dòng 1810, 1859-1864):

```cpp
std::set<gint> processed_frames;   // set local, reset mỗi buffer

// ... duyệt qua từng frame trong batch_meta->frame_meta_list ...

gint framemeta_map_idx = source_id;   // ← KEY DEDUP = source_id (luôn = 0)

/* skip frame if already processed */
if (processed_frames.find(framemeta_map_idx) != processed_frames.end()) {
    continue;   // ← BỎ QUA frame thứ 2, 3, 4... cùng source_id!
}
processed_frames.insert(framemeta_map_idx);
```

**Vấn đề**: nvdspreprocess dùng **`source_id`** làm key dedup. Khi streammux gom **nhiều frames từ cùng 1 source** vào 1 GstBuffer (batch), plugin chỉ xử lý **frame đầu tiên** và skip toàn bộ các frame còn lại vì `source_id` đã tồn tại trong `processed_frames`.

### Tại sao lại batch 4 frames?

Streammux config:
```yaml
streammux:
  batch-size: 10
  batched-push-timeout: 40000   # 40ms
```

Với 1 source ở ~30fps (33ms/frame), khi pipeline xử lý chậm hơn tốc độ input, streammux tích lũy frames trong timeout period. Kết quả: mỗi GstBuffer chứa ~4 frames từ cùng source_id=0.

### Xác nhận: Bug đã được NVIDIA thừa nhận

Trên forum NVIDIA ([link](https://forums.developer.nvidia.com/t/nvdspreprocess-skip-frames-with-the-same-source-id/338711)), NVIDIA đã xác nhận đây là bug:

> *"Yes. Theoretically, if multiple frames with the same source id appear in the same batch, there will indeed be a problem. The frame_num also needs to be taken into consideration in this scenario. We will fix this issue as soon as possible. Since it's opensource, you can try to fix that too. Thanks"*

### Workarounds

#### Cách 1: Giảm `batched-push-timeout` (đơn giản, hiệu quả)

```yaml
streammux:
  batched-push-timeout: 10000   # 10ms thay vì 40ms
```

→ Streammux gom ít frame hơn → mỗi batch thường chỉ có 1 frame/source → preprocess không bỏ sót.

**Hạn chế**: Tăng overhead vì pipeline xử lý nhiều batch nhỏ thay vì ít batch lớn.

#### Cách 2: Sửa source code nvdspreprocess (fix triệt để)

Thay key dedup từ `source_id` sang tổ hợp `(source_id, frame_num)`:

```diff
- gint framemeta_map_idx = source_id;
+ // Fix: dùng frame_num thay vì source_id để không skip frames cùng source
+ gint framemeta_map_idx = frame_meta->frame_num;
```

Cần rebuild plugin `libnvdsgst_preprocess.so` sau khi sửa.

#### Cách 3: Đặt preprocess serial trên main pipeline (thay đổi kiến trúc)

Thay vì chạy preprocess trên nhánh TEE (parallel), đặt nó **trực tiếp trên main path** (serial):

```
Trước: ... → tracker → [TEE → preprocess → fakesink] → SGIE → ...
Sau:   ... → tracker → preprocess → SGIE → ...
```

Khi đó preprocess nhận từng buffer riêng lẻ, không bị ảnh hưởng bởi batching. Nhưng cần sửa `deepstream_app.c` phần `create_common_elements()`.

---

## 5. Các key config quan trọng khác

### 5.1. `src-ids` và các tham số lọc object trong `nvdspreprocess`

Trong cấu hình `nvdspreprocess` (ví dụ: `config_preprocess_action_recognition.txt`), các tham số định nghĩa trong một group (như `[group-0]`) chỉ có hiệu lực với các source ID được liệt kê trong trường `src-ids`.

```
[group-0]
src-ids=0;1;2;3;4;5;6;7;8;9
custom-input-transformation-function=custom_transform_pose_sequence
process-on-roi=1
process-on-all-objects=1
roi-params-src-0=0;0;100;100
roi-params-src-1=0;0;100;100
input-object-min-width=0
input-object-min-height=0
draw-roi=0
```

- **Vấn đề**: Nếu danh sách `src-ids` không bao gồm tất cả các source đang chạy trong pipeline, các source bị thiếu sẽ không được áp dụng các tùy chỉnh trong group đó.
- **Hệ lụy**: Các source không nằm trong `src-ids` có thể (chưa xác nhận) sẽ sử dụng giá trị mặc định của plugin. Một ví dụ điển hình là `input-object-min-width` và `input-object-min-height`. Nếu trong group bạn đã set bằng `0` để nhận mọi object, nhưng source ID không có trong group, nó sẽ dùng giá trị mặc định (có thể > 0), dẫn đến việc các object nhỏ bị lọc bỏ một cách âm thầm và không được đưa vào xử lý (tensor preparation).

### 5.2. `secondary-reinfer-interval` (nvinfer SGIE)

```yaml
# Trong config SGIE (ví dụ action_recognition.yml)
property:
  secondary-reinfer-interval: 0   # 0 = re-classify mỗi frame cho cùng tracked object
```

- nvinfer (SGIE mode) **cache** kết quả classification cho mỗi tracked object.
- `secondary-reinfer-interval: N` → skip N frames trước khi re-classify cùng object.
- **Default rất lớn** → chỉ classify 1 lần rồi cache mãi.
- `secondary-reinfer-interval: 0` → re-classify mỗi frame.

> **Lưu ý**: Key này KHÁC với `interval` (chỉ control batch skipping, không ảnh hưởng object history cache).

### 5.3. `input-tensor-from-meta` (nvinfer)

```yaml
# Trong config nvinfer SGIE
property:
  input-tensor-from-meta: 1   # Bắt buộc khi dùng nvdspreprocess cung cấp tensor
```

Khi sử dụng `nvdspreprocess` + `nvinfer`, cần set **cả 2 nơi**:
1. `input-tensor-from-meta: 1` trong config YAML của nvinfer
2. `input-tensor-meta: 1` trong `deepstream-app.yml` (vì `g_object_set` override YAML parser)

### 5.4. `unique-id` / `target-unique-ids` (nvdspreprocess ↔ nvinfer matching)

```ini
# config_preprocess.txt
[property]
unique-id=6
target-unique-ids=7    # nvinfer kiểm tra tensor meta match unique_id

# nvinfer config
property:
  gie-unique-id: 7     # phải match target-unique-ids
```

nvinfer kiểm tra `target_unique_ids` trong tensor meta để quyết định có dùng tensor từ preprocess hay không.

---

## 6. Lưu ý về tham số `interval` của nvinfer (PGIE & SGIE)

Liên quan tới tham số `interval` cho `nvinfer`, có 3 điểm quan trọng cần lưu ý:

### 6.1. `interval` mặc định không hoạt động với SGIE và `input-tensor-meta=1`

- **Thực trạng**: Tham số này hiện chỉ được implement và hoạt động cho PGIE. Không có logic bỏ qua frame trong các hàm xử lý của SGIE (bằng chứng là không có code liên quan tới interval trong `gst_nvinfer_process_objects`, `gst_nvinfer_process_tensor_input` hay `should_infer_object`).
- **Cách khắc phục để dùng cho SGIE**:
  Cần bổ sung đoạn code skip frame vào đầu các hàm `gst_nvinfer_process_objects` và `gst_nvinfer_process_tensor_input` trong mã nguồn `gstnvinfer.cpp` (trên máy chủ nằm tại đường dẫn `/opt/nvidia/deepstream/deepstream/sources/gst-plugins/gst-nvinfer/gstnvinfer.cpp`):

  ```cpp
  // laptq =======================
  if (nvinfer->interval_counter++ % (nvinfer->interval + 1) > 0) {
    // std::cout << "[GSTNVINFER] SKIPPING gst_nvinfer_process_tensor_input on nvinfer->interval_counter = " << nvinfer->interval_counter - 1 << " | nvinfer->interval + 1 = " << nvinfer->interval + 1 << "\n";
    return GST_FLOW_OK;
  }
  // =============================
  ```

- **Build và cập nhật plugin**:
  Sau khi sửa source code, cần build lại thư viện `libnvdsgst_infer.so` và ghi đè vào thư mục hệ thống:
  ```bash
  cd /opt/nvidia/deepstream/deepstream/sources/gst-plugins/gst-nvinfer
  make
  # Copy (ghi đè) file thư viện mới vào đúng vị trí
  cp libnvdsgst_infer.so /opt/nvidia/deepstream/deepstream/lib/gst-plugins/
  ```

### 6.2. Đồng bộ `interval` với các Downstream Elements

Khi bật `interval` cho 1 SGIE mà ở phía downstream có các SGIE hoặc `nvdspreprocess` khác dùng kết quả infer của SGIE đó, cần lưu ý:
- **Tất cả các object ở mọi frame** đều được truyền vào các downstream element này.
- Do đó, để downstream elements hoạt động đúng đồng nhịp với interval của upstream SGIE:
  - **Với downstream SGIE**: Cần bật tham số `interval` tương đương với upstream SGIE.
  - **Với downstream `nvdspreprocess`**: Cần lưu ý logic xử lý khi nhận buffer. Tất cả các object đều được truyền vào hàm preprocess, nhưng **chỉ có object nào không bị skip mới được gắn metadata** từ upstream SGIE vào buffer. Cần chú ý cẩn thận khi code xử lý tiền xử lý.

### 6.3. Hiện tượng nhấp nháy bbox ở PGIE và vai trò của Tracker

- Khi bật `interval` với PGIE và visualize lên màn hình, ta sẽ thấy các bounding box bị chớp nháy (flicker) liên tục do không phải frame nào cũng được detect.
- **Bật interval cho PGIE detector nhưng thấy frame nào cũng có box?**: Nếu đặt `nvtracker` ở ngay phía sau PGIE, dù không phải frame nào cũng được detect, box **lúc nào cũng được trả về** do `nvtracker` sẽ tự động sinh và nội suy (interpolate) tọa độ dự đoán cho các frame bị khuyết, giúp đối tượng di chuyển mượt mà trơn tru, bbox không bị chớp hay biến mất trên màn hình.
