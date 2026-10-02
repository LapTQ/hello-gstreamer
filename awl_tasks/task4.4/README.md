This pipeline has the following components:
- RTSP input with auto-reconnection: `nvurisrcbin` (`rtsp-reconnect-attempts`, `rtsp-reconnect-interval`)
- YOLOv9 object detection: `nvinfer`
- Tracking: `nvtracker`
- Performance measurement: `nvdslogger` (measures and prints FPS every 1 second)
- Timestamp OSD probe: Custom buffer probe on `nvosd` sink pad injecting current date & time text (`NvDsDisplayMeta`) onto each frame
- Visualization: `nvvideoconvert` + `nvdsosd`

#### How to run
```bash
bash awl_tasks/task4.3/run.sh
```

- **Current input:** RTSP stream configured in `main.cpp`:
  - `rtsp://admin:12345@192.168.3.26/live`
- **Output:** `outputs/output.mp4` *(Note: RTSP stream runs indefinitely; press `Ctrl + C` to cleanly stop and finalize the video).*

`run.sh` automatically:
1. Clones and builds `libnvdsinfer_custom_impl_Yolo.so` if not already present.
2. Generates TensorRT engine `LibreYOLO9t.onnx.fp16_max100.trt` if not already present.

---

### Simulating Network Disconnection & Auto-Reconnection

#### 1. Install `iptables`
Inside Docker container:
```bash
apt update && apt install -y iptables
```

#### 2. Simulate Network Drop (Block Camera Packets)
Add a firewall rule to drop all incoming packets from the camera IP:
```bash
iptables -A INPUT -s 192.168.3.26 -j DROP
```

#### 3. Restore Network Connection
Delete the drop rule to restore traffic from the camera:
```bash
iptables -D INPUT -s 192.168.3.26 -j DROP
```

