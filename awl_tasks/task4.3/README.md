This pipeline has the following components:
- Video input: `nvurisrcbin` + `nvstreammux`
- YOLOv9 object detection: `nvinfer`
- Tracking: `nvtracker`
- Performance / FPS measurement: `nvdslogger` (measures and prints FPS every 1 second)
- Asynchronous metadata logging: Custom buffer probe on `tracker` src pad writing to file via a dedicated background thread
- Visualization: `nvvideoconvert` + `nvdsosd`

#### How to run
```bash
bash awl_tasks/task3.4/run.sh
```

- **Current input:** configured in `main.cpp`:
  - `rtsp://admin:12345@192.168.3.26/live`
- **Outputs:**
  - Video file: `outputs/output.mp4` (video overlaying bounding boxes, track IDs, and class labels).
  - Metadata file: `outputs/repo.jsonl` (tracking records in JSON Lines format: `{"track_id": ..., "frame_num": ..., "x1": ..., "y1": ..., "x2": ..., "y2": ...}`).
  - Console: Real-time FPS logs printed by `nvdslogger`.

#### FPS Measurement & File Saving Comparison

To compare the pipeline's FPS **with vs. without saving to file**, you can enable/disable this line:
  ```cpp
  gst_pad_add_probe(tracker_srcpad, GST_PAD_PROBE_TYPE_BUFFER, save_to_repo, &repo, NULL);    // line 310
  ```

`run.sh` automatically:
1. Clones and builds `libnvdsinfer_custom_impl_Yolo.so` if not already present.
2. Generates TensorRT engine `LibreYOLO9t.onnx.fp16_max100.trt` if not already present.
