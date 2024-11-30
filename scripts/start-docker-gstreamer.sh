docker run \
    -it \
    -d \
    -v /media:/media \
    -v /home:/home \
    \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v ${HOME}/.Xauthority:/root/.Xauthority \
    -e DISPLAY=$DISPLAY \
    -e QT_X11_NO_MITSHM=1 \
    \
    --device /dev/snd \
    -e PULSE_SERVER=unix:${XDG_RUNTIME_DIR}/pulse/native \
    -v ${XDG_RUNTIME_DIR}/pulse/native:${XDG_RUNTIME_DIR}/pulse/native \
    -v ~/.config/pulse/cookie:/root/.config/pulse/cookie \
    --group-add $(getent group audio | cut -d: -f3) \
    \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    --gpus all \
    --workdir $( pwd ) \
    --name hello-gstreamer \
    nvidia/cuda:12.2.2-devel-ubuntu20.04