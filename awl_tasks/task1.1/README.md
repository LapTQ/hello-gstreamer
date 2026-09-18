### 1. Run with MP4 file

```bash
bash awl_tasks/task1.1/run_mp4_nv.sh
```
- **Current input:** `assets/video_1.mp4`
- **Output:**
  - Display directly on screen (`nveglglessink` / `nv3dsink`). *(Note: When running inside Docker, disable/comment out this command if screen display is unavailable).*
  - Or export to file: `outputs/output.mp4`

---

### 2. Run with RTSP stream

```bash
bash awl_tasks/task1.1/run_rtsp_nv.sh
```
- **Current input:** Real RTSP stream `rtsp://admin:12345@192.168.3.27/live` or simulated via mediamtx: `rtsp://localhost:8554/live/stream`.
- **Output:**
  - Display directly on screen (`nveglglessink`). *(Note: When running inside Docker, disable/comment out this command if screen display is unavailable).*
  - Or export to file: `outputs/output_rtsp.mp4` *(Note: RTSP stream runs indefinitely; press `Ctrl + C` to stop).*
