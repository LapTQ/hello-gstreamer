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

* **Device**: Jupiter (RTX 4090).
* **Input**: 2 video streams: `assets/sample_720p.h264`


| Metric | Stream 0 | Stream 1 | Total |
| :--- | :---: | :---: | :---: |
| **Real-time Processing FPS** | 154.83 FPS | 154.83 FPS | |
| **Latency** (from streammux -> before OSD) | 20.81 ms | 20.81 ms | |
| **GPU Utilization (%)** | — | — | 3% (`nvtop`) |
| **GPU VRAM** | — | — | ~2.8 GB |
| **CPU Usage** | — | — | ~0.3 CPU cores (`htop`) |
| **System RAM** | — | — | ~2.5 GB |

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