# docker pull nvcr.io/nvidia/deepstream:7.1-gc-triton-devel
# docker pull nvcr.io/nvidia/deepstream:7.1-samples-multiarch

xhost +
docker run \
    -it \
    -d \
    --gpus all \
    --runtime nvidia \
    --network=host \
    --privileged \
    -v /media:/media \
    -v /home:/home \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v ${HOME}/.Xauthority:/root/.Xauthority \
    -e DISPLAY=$DISPLAY \
    -e QT_X11_NO_MITSHM=1 \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -e PATHD_NVDS=/opt/nvidia/deepstream/deepstream-7.1 \
    --workdir $( pwd ) \
    --name hello-deepstream \
    nvcr.io/nvidia/deepstream:7.1-gc-triton-devel


