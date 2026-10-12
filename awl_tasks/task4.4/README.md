## 1. Features

- Multi RTSP-Stream with automatic network reconnection.
- Person Detection (LibreYOLO9t).
- Tracking (NvDCF).
- Person View Classification.
- Action State Recognition with person's crop image **extended 10-30%**.
- SQLite database.
- OSD Visualization.
- Video export to MP4.

---

## 2. How to run

```bash
# 1. Configure RTSP camera sources: awl_tasks/task4.4/configs/settings.yaml

# 2. Build all dependencies and run the entire pipeline
bash awl_tasks/task4.4/run.sh
```

**Workflow in `run.sh`:**
- Downloads, builds, and installs dependencies (`yaml-cpp`, `SQLiteCpp`).
- Compiles the custom YOLO parsing library (`libnvdsinfer_custom_impl_Yolo.so`).
- Converts 3 ONNX models to TensorRT FP16.
- Compiles `outputs/build/main.out`.

---

## 3. Outputs

* **Demo Video**: [`outputs/output.mp4`](outputs/output.mp4).
* **Database**: [`outputs/sqlite.db`](outputs/sqlite.db).

---

## 4. Performance Benchmark

*GPU: Jupiter (RTX 4090).*

| Metric | Stream 0 (`192.168.3.26`) | Stream 1 (`192.168.3.21`) | Total |
| :--- | :---: | :---: | :---: |
| **Input Resolution & FPS** | 960 x 640 @ 10 FPS | 960 x 640 @ 5 FPS | |
| **Real-time Processing FPS** (Matches camera rate for RTSP) | **9.74 FPS** | **4.74 FPS** | |
| **Latency** (from streammux -> before OSD) | **18.50 ms** | **18.22 ms** | |
| **GPU Utilization (%)** | — | — | 18% – 24% (`nvidia-smi`) |
| **GPU VRAM** | — | — | ~2.8 GB |
| **CPU Usage** | — | — | ~1.8 CPU cores (`top`) |
| **System RAM** | — | — | ~1.2 GB |

---

## 5. Architecture

```
[RTSP Streams] ──> nvurisrcbin
                         │
                         ▼
                    nvstreammux
                         │
                         ▼
                    nvinfer (PGIE: LibreYOLO9t)
                         │
                         ▼
                    nvtracker
                         │
                         ▼
                    nvinfer (SGIE: MobileNetV3 Person View)
                         │
                         ▼
                    nvinfer (SGIE: EfficientNetv2S Action State)
                         │
                         ▼
                    identity (Sqlite)
                         │
                         ▼
                    nvdslogger
                         │
                         ▼
                    nvdsosd ──> mp4_filesink
```