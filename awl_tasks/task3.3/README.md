This pipeline has the following components:
- Video input: `nvurisrcbin` + `nvstreammux`
- YOLOv9 object detection: `nvinfer`
- Tracking: `nvtracker`
- Line crossing detection & custom OSD: Buffer probe on `tracker` src pad.
- Visualization: `nvvideoconvert` + `nvdsosd`

#### How to run
```bash
bash awl_tasks/task3.3/run.sh
```

- **Current input:** Video file configured in `main.cpp`:
  - `file:///home/laptq/hello-gstreamer/assets/sample_720p.h264`
- **Output:** `outputs/output.mp4` (video overlaying bounding boxes, track IDs, the virtual crossing line, and real-time crossing counter).

`run.sh` automatically:
1. Clones and builds `libnvdsinfer_custom_impl_Yolo.so` if not already present.
2. Generates TensorRT engine `LibreYOLO9t.onnx.fp16_max100.trt` if not already present.
