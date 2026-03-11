# DeepStream Configuration Architecture - Giải Thích Chi Tiết

## I. NỀN TẢNG: GStreamer và GObject Properties

### GStreamer Elements là gì?
GStreamer là một framework cho xử lý media (audio/video). Các **element** là các component có:
- **Input pads** (sink): nhận dữ liệu từ element khác
- **Output pads** (src): gửi dữ liệu tới element khác
- **Properties** (thuộc tính): các giá trị cấu hình ảnh hưởng tới hoạt động

### GObject Properties
Mỗi GStreamer element là một GObject, và các thuộc tính của nó được quản lý bởi GObject system:

```c
// Cách đặt một property
g_object_set(G_OBJECT(element), "property-name", value, NULL);

// Ví dụ thực tế từ test2
g_object_set(G_OBJECT(nvtracker), "tracker-width", 960, NULL);
g_object_set(G_OBJECT(nvtracker), "ll-config-file", "/path/to/config.yml", NULL);
g_object_set(G_OBJECT(pgie), "config-file-path", "dstest2_pgie_config.txt", NULL);
```

**Quan trọng**: Mỗi property có **loại dữ liệu riêng** (integer, string, boolean, enum, v.v.)

---

## II. TẠI SAO CÓ HAI MỨC CẤU HÌNH? (Câu hỏi 1.1)

### A. Configuration Hierarchy

**DeepStream sử dụng chiến lược 2 tầng**:

```
dstest2_config.yml (Main config - cấp 1)
├── source
│   └── location: ... (thuộc tính của filesrc element)
├── streammux
│   └── batch-size: 1 (thuộc tính của nvstreammux element)
├── tracker
│   ├── tracker-width: 960 (thuộc tính của nvtracker)
│   ├── tracker-height: 544 (thuộc tính của nvtracker)
│   ├── ll-lib-file: ... (thuộc tính của nvtracker)
│   └── ll-config-file: config_tracker_NvDCF_perf.yml (đường dẫn tới tệp phụ)
│
└── primary-gie
    ├── plugin-type: 0 (thuộc tính của nvinfer)
    └── config-file-path: dstest2_pgie_config.yml (đường dẫn tới tệp phụ)
         └── [Tệp phụ: dstest2_pgie_config.yml]
             └── property
                 ├── gpu-id: 0 (thuộc tính của nvinfer)
                 ├── batch-size: 1 (thuộc tính của nvinfer)
                 ├── model-engine-file: ... (thuộc tính của nvinfer)
                 └── ... (nhiều thuộc tính khác)
```

### B. Tại sao thiết kế này?

**Có 2 lý do chính**:

#### 1. **Tính Mô-đun Hóa (Modularity)**

Một số element có **rất nhiều thuộc tính** (5-50+ properties). Nếu đặt tất cả vào một tệp config sẽ:
- Làm file quá dài, khó đọc
- Gây lẫn lộn giữa các element khác nhau
- Khó bảo trì và cập nhật

**Giải pháp**: Tách các element phức tạp vào tệp config riêng biệt.

#### 2. **Tái Sử Dụng (Reusability)**

Xem [dstest2_pgie_config.yml](dstest2_pgie_config.yml):

```yaml
property:
  model-engine-file: ../../../../samples/models/Primary_Detector/resnet18_trafficcamnet.etlt_b1_gpu0_int8.engine
  labelfile-path: ../../../../samples/models/Primary_Detector/labels.txt
  int8-calib-file: ../../../../samples/models/Primary_Detector/cal_trt.bin
  # ... 15+ thuộc tính khác
```

Tệp này có thể **được tái sử dụng bởi nhiều ứng dụng khác nhau**:
- Chỉ cần thay đổi `config-file-path` trong tệp main config
- Không cần chỉnh sửa code C, không cần rebuild

**Ví dụ**:
```yaml
# application_A.yml
primary-gie:
  config-file-path: detector_resnet18.yml

# application_B.yml  
primary-gie:
  config-file-path: detector_yolov4.yml

# Cùng một element (nvinfer), nhưng khác model!
```

---

## III. CẤU TRÚC TỆP CONFIG: "property" SECTION (Câu hỏi 1.2)

### Câu hỏi: Tại sao có một section `property` ở đầu tệp?

```yaml
property:
  gpu-id: 0
  batch-size: 1
  model-engine-file: ...
  # ... (tất cả là thuộc tính của nvinfer element)
```

### Trả lời: **Đây là cách YAML Parser của DeepStream phân tích dữ liệu**

Hãy xem code trong [deepstream_test2_app.c](deepstream_test2_app.c#L432-440):

```c
if (yaml_config) {
    RETURN_ON_PARSER_ERROR(nvds_parse_gie(pgie, argv[1], "primary-gie"));
    RETURN_ON_PARSER_ERROR(nvds_parse_gie(sgie1, argv[1], "secondary-gie1"));
    RETURN_ON_PARSER_ERROR(nvds_parse_gie(sgie2, argv[1], "secondary-gie2"));
    
    RETURN_ON_PARSER_ERROR(nvds_parse_tracker(nvtracker, argv[1], "tracker"));
}
```

Khi gọi `nvds_parse_gie()`, DeepStream's YAML parser:
1. Tìm section `"primary-gie"` trong main config
2. Đọc `config-file-path: dstest2_pgie_config.yml`
3. **Mở tệp phụ này**
4. **Tìm section `"property"`** trong tệp phụ (đây là convention/quy ước)
5. Trích xuất tất cả key-value từ section này
6. Gọi `g_object_set()` cho từng thuộc tính

```
Luồng gán giá trị:
├─ main config (dstest2_config.yml)
│  └─ "primary-gie" section
│     └─ "config-file-path": "dstest2_pgie_config.yml"
│
└─ secondary config (dstest2_pgie_config.yml)
   └─ "property" section
      ├─ "gpu-id": 0 → g_object_set(..., "gpu-id", 0, ...)
      ├─ "batch-size": 1 → g_object_set(..., "batch-size", 1, ...)
      ├─ "model-engine-file": "..." → g_object_set(..., "model-engine-file", "...", ...)
      └─ ... (tất cả các thuộc tính khác)
```

### Section "property" là convention của DeepStream

- **Không phải là GStreamer standard** - nó là quy ước riêng của DeepStream
- **Được xác định bởi `nvds_parse_gie()` function** trong DeepStream's YAML parser library
- Các element khác cũng có quy ước tương tự:
  - `tracker` section dùng từ `nvds_parse_tracker()`
  - `source` section dùng từ `nvds_parse_file_source()`
  - v.v.

**Kết luận**: Tất cả các field dưới `property` **vẫn là thuộc tính của gst element**, chỉ là chúng được nhóm lại trong một section riêng để dễ quản lý.

---

## IV. NVTRACKER'S SPECIAL CASE: ll-lib-file và ll-config-file (Câu hỏi 2)

### A. ll-lib-file: Low-Level Library File

**Định nghĩa**: File `.so` (Shared Object - thư viện động trên Linux) chứa **implementation thực tế của tracking algorithm**.

```yaml
ll-lib-file: /opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so
```

#### Cấu trúc bên trong:

```
nvtracker element (GStreamer plugin)
│
├─ Core logic: xử lý GStreamer pads, metadata, ...
│  (được viết bằng C/C++, biên dịch thành .so file)
│
└─ Tracking algorithm: gọi hàm từ ll-lib-file
   │
   └─ libnvds_nvmultiobjecttracker.so
      ├─ IOU Tracker implementation
      ├─ NvSort implementation
      ├─ NvDCF implementation
      ├─ DeepSORT implementation
      └─ Utility functions
```

#### Tại sao tách thành .so file riêng?

1. **Cho phép thay thế tracking algorithm** mà không cần rebuild nvtracker element
2. **Performance**: Low-level library có thể được tối ưu hóa CUDA/GPU
3. **Licensing/Security**: Có thể cấp license riêng cho phần tracking
4. **Flexibility**: Người dùng có thể swap ra tracker khác

### B. ll-config-file: Low-Level Library Configuration

**Định nghĩa**: File config đặc biệt cho low-level tracker library, chứa các **hyper-parameter của tracking algorithm** (không phải GStreamer properties).

```yaml
ll-config-file: ../../../../samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml
```

#### Ví dụ từ file thực tế:

```yaml
# config_tracker_NvDCF_perf.yml
BaseConfig:
  minDetectorConfidence: 0.0430      # ← Tracker algorithm parameter

TargetManagement:
  maxTargetsPerStream: 150           # ← Tracker algorithm parameter
  minIouDiff4NewTarget: 0.7418       # ← Tracker algorithm parameter
  probationAge: 2                    # ← Tracker algorithm parameter

DataAssociator:
  dataAssociatorType: 0              # ← Tracker algorithm parameter
  minMatchingScore4Overall: 0.4290   # ← Tracker algorithm parameter
  # ... 20+ tham số khác
```

**Những parameter này KHÔNG phải GStreamer properties** vì:
- Chúng là các tham số của tracking algorithm cụ thể (NvDCF, NvSort, IOU, v.v.)
- Mỗi tracker algorithm có một set parameter khác nhau
- Chúng không được expose qua GStreamer's `gst-inspect-1.0` command

#### Luồng hoạt động:

```
g_object_set(nvtracker, "ll-config-file", "config_tracker_NvDCF_perf.yml", NULL)
│
└─ nvtracker element:
   1. Nhận thuộc tính "ll-config-file"
   2. Đọc file YAML
   3. Parse từng section (BaseConfig, TargetManagement, DataAssociator, ...)
   4. Gọi hàm từ ll-lib-file với các tham số này:
      ```c
      // Pseudocode
      tracker_lib->set_min_detector_confidence(0.0430);
      tracker_lib->set_max_targets_per_stream(150);
      tracker_lib->set_min_iou_diff_for_new_target(0.7418);
      // ...
      ```
```

### C. Mối Quan Hệ Giữa ll-lib-file và ll-config-file

```
ll-lib-file ────────────────────────── ll-config-file
(Implementation)                        (Configuration)
│                                       │
├─ NvDCF tracker code                  ├─ NvDCF hyper-parameters
├─ NvSort tracker code                 ├─ NvSort hyper-parameters (tuning)
├─ IOU tracker code                    ├─ IOU hyper-parameters (tuning)
└─ ...                                 └─ ...

Nếu thay ll-config-file từ
  config_tracker_NvDCF_perf.yml 
sang 
  config_tracker_NvDCF_accuracy.yml

→ Tracker algorithm (NvDCF) vẫn giống nhau
→ Nhưng các tham số thay đổi (chọn "performance" vs "accuracy")
```

### D. Khi Nào Cần Các File Này?

**ll-lib-file cần:**
- Chỉ khi element là `nvtracker`
- Bắt buộc - không thể thiếu
- Thường được cài đặt mặc định ở `/opt/nvidia/deepstream/deepstream/lib/`

**ll-config-file cần:**
- Khi muốn tuning tracking algorithm
- Optional - nếu không cung cấp, sẽ dùng default parameters
- Cần thay đổi khi:
  - Chuyển sang tracking algorithm khác (IOU → NvSort → NvDCF)
  - Tuning hyper-parameters cho use-case cụ thể của bạn

**Các element khác cần gì?**
```
nvinfer/nvinferserver:
  - Cần "config-file-path" để chỉ định model, input/output shapes, ...
  - Không cần "ll-lib-file" vì đó là inference library, không plugin

nvvideoconvert:
  - Không cần file config riêng
  - Tất cả properties được đặt trực tiếp

nvdsosd:
  - Không cần file config riêng
```

---

## V. TÓM TẮT VÀ QUY TẮC THIẾT KẾ

### Quy tắc 1: **GStreamer Properties**
- Mỗi element có một set properties cố định
- Xem bằng: `gst-inspect-1.0 element_name`
- Đặt bằng: `g_object_set()` hoặc YAML config
- Ví dụ: `tracker-width`, `gpu-id`, `batch-size`, `config-file-path`

### Quy tắc 2: **Configuration File Hierarchy**
```
Main config (dstest2_config.yml)
  ├─ Các element đơn giản: properties trực tiếp
  │  (source, streammux, sink)
  │
  └─ Các element phức tạp: trỏ tới file phụ
     (tracker → ll-config-file, pgie → config-file-path)
     
Secondary config (dstest2_pgie_config.yml)
  └─ "property" section chứa tất cả GStreamer properties
```

### Quy tắc 3: **ll-lib-file và ll-config-file là đặc biệt**
- **ll-lib-file**: Low-level tracker algorithm implementation
  - File `.so` (compiled code)
  - Contains: NvDCF, NvSort, IOU, DeepSORT implementations
  
- **ll-config-file**: Hyper-parameters cho tracker algorithm
  - File YAML hoặc text
  - Contains: algorithm-specific tuning parameters
  - Các parameter này KHÔNG phải GStreamer properties

### Quy tắc 4: **Khi Nào Nào Dùng Gì**
```
Nếu muốn thay đổi:
├─ Model (cho nvinfer) → Thay config-file-path
├─ Input size (cho nvinfer) → Thay config-file-path → chỉnh property trong file phụ
├─ Tracker algorithm (IOU/NvSort/NvDCF) → Thay ll-lib-file (hiếm) + ll-config-file
├─ Tracker hyper-params → Thay ll-config-file
├─ GPU ID → Thay gpu-id property (có trong main config hoặc file phụ)
└─ Batch size → Thay batch-size property
```

---

## VI. VÍ DỤ THỰC TẾ

### Example 1: Thay Đổi Model cho Primary GIE

**Hiện tại**:
```yaml
# dstest2_config.yml
primary-gie:
  config-file-path: dstest2_pgie_config.yml

# dstest2_pgie_config.yml
property:
  model-engine-file: resnet18_trafficcamnet.etlt_b1_gpu0_int8.engine
  output-blob-names: output_cov/Sigmoid;output_bbox/BiasAdd
```

**Để thay sang YOLOV4**:
1. Tạo file mới: `yolov4_pgie_config.yml`
```yaml
property:
  model-engine-file: yolov4.etlt_b1_gpu0_int8.engine
  output-blob-names: BatchedNMSPlugin_0;keep_count_0
  # ... (các properties khác cho YOLOv4)
```

2. Cập nhật main config:
```yaml
primary-gie:
  config-file-path: yolov4_pgie_config.yml
```

3. Run lại: `./app dstest2_config.yml` ✅ No code changes needed!

### Example 2: Thay Đổi Tracker Algorithm

**Thay từ NvDCF sang NvSort**:

1. Tìm file config của NvSort:
```bash
ls /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_*
```

2. Cập nhật main config:
```yaml
tracker:
  ll-lib-file: /opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so
  ll-config-file: ../../../../samples/configs/deepstream-app/config_tracker_NvSORT.yml
```

3. Run lại: `./app dstest2_config.yml` ✅ Tracking algorithm đã thay đổi!

---

## VII. MỘT SỐ CÂU HỎI THÊM CÓ THỂ CÓ

**Q: Tại sao gst-inspect-1.0 không hiển thị các parameter của ll-config-file?**
A: Vì những parameter đó không phải GStreamer properties - chúng là nội bộ của low-level library. GStreamer chỉ biết về `ll-config-file` (string property), nhưng nó sẽ gửi toàn bộ file tới tracker library để parse.

**Q: Có thể đặt ll-config-file programmatically không?**
A: Có, thông qua `g_object_set()`:
```c
g_object_set(G_OBJECT(nvtracker), "ll-config-file", "/path/to/config.yml", NULL);
```

**Q: Mỗi tracker algorithm cần ll-lib-file khác nhau không?**
A: Không! Cùng một `libnvds_nvmultiobjecttracker.so` chứa implementations của tất cả tracker types (IOU, NvSort, NvDCF, DeepSORT). Chỉ cần thay đổi `ll-config-file` để chọn algorithm.

**Q: Có thể viết ll-config-file riêng không?**
A: Được, nếu bạn hiểu định dạng của nó. Tệp config của NVIDIA được tuning bởi họ, nhưng nếu muốn custom hyper-parameters, bạn có thể sao chép và chỉnh sửa.

