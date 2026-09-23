This pipeline has the following components:
- Multi-stream RTSP: `nvurisrcbin` + `nvstreammux`
- YOLOv9 object detection: `nvinfer`
- Tracking: `nvtracker`
- Metadata logging probe: Custom buffer probe on `tracker` src pad.
- Visualization: `nvmultistreamtiler` + `nvdsosd`

#### How to run
```bash
bash awl_tasks/task3.1/run.sh
```

- **Current inputs:** Multi-stream RTSP sources configured in `main.cpp`:
  - `rtsp://admin:12345@192.168.3.26/live`
  - `rtsp://admin:12345@192.168.3.21/live`
- **Output:**
  - Console logs: Real-time detection and tracking metadata (Source ID, Frame ID, Track ID, Class, Normalized Box, Confidence)
  - Video: `outputs/output.mp4` *(Note: RTSP streams run indefinitely; you will need to press `Ctrl + C` to stop the pipeline).*

`run.sh` automatically:
1. Clones and builds `libnvdsinfer_custom_impl_Yolo.so` if not already present.
2. Generates TensorRT engine `LibreYOLO9t.onnx.fp16_max100.trt` if not already present.
