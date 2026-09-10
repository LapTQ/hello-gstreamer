# Giả lập RTSP từ file bằng mediamtx
# gst-launch-1.0 multifilesrc location=assets/sample_720p.h264 loop=true ! h264parse ! flvmux ! rtmp2sink location=rtmp://localhost:1935/live/stream

PATH__FILE__SRC=awl_tasks/task1.3/main_rtsp.cpp
PATH__DIR__OUTPUT=outputs
mkdir -p $PATH__DIR__OUTPUT

g++ \
    -fdiagnostics-color=always \
    -g \
    -ggdb \
    -O2 \
    -DNDEBUG \
    -pedantic-errors \
    -Wall \
    -Wextra \
    -Wconversion \
    -Wsign-conversion \
    -std=c++17 \
    -o $PATH__DIR__OUTPUT/main.out \
    $PATH__FILE__SRC \
    `pkg-config --cflags --libs gstreamer-1.0`

./${PATH__DIR__OUTPUT}/main.out

