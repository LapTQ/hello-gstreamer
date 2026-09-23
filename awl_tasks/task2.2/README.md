This pipeline has the following components:
- Multi-stream RTSP: `nvurisrcbin` + `nvstreammux`
- YOLOv9 object detection: `nvinfer`
- Tracking: `nvtracker`
- Visualization: `nvmultistreamtiler` + `nvdsosd`

#### How to run
```bash
bash awl_tasks/task2.2/run.sh
```

- **Current inputs:** Multi-stream RTSP sources configured in `main.cpp`:
  - `rtsp://admin:12345@192.168.3.26/live`
  - `rtsp://admin:12345@192.168.3.21/live`
- **Output:** `outputs/output.mp4` *(Note: RTSP streams run indefinitely; you will need to press `Ctrl + C` to stop the pipeline).*

`run.sh` automatically:
1. Clones and builds `libnvdsinfer_custom_impl_Yolo.so` if not already present.
2. Generate TensorRT engine `LibreYOLO9t.onnx.fp16_max100.trt` if not already present.
