docker run \
    -it \
    -d \
    -v /media:/media \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v ${HOME}/.Xauthority:/root/.Xauthority \
    -e DISPLAY=$DISPLAY \
    -e QT_X11_NO_MITSHM=1 \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -e DIR_NVDS=/opt/nvidia/deepstream/deepstream \
    --runtime nvidia \
    --workdir $( pwd ) \
    --name hello-deepstream \
    nvcr.io/nvidia/deepstream:7.1-triton-multiarch


