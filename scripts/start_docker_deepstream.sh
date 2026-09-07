curl -fsSL https://nvidia.github.io/libnvidia-container/gpgkey | sudo gpg --dearmor -o /usr/share/keyrings/nvidia-container-toolkit-keyring.gpg \
  && curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
    sed 's#deb https://#deb [signed-by=/usr/share/keyrings/nvidia-container-toolkit-keyring.gpg] https://#g' | \
    sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list
sudo apt-get update

xhost +

docker run \
    -it \
    -d \
    \
    -v /run/media/laptq/data/workspace:/run/media/laptq/data/workspace \
    -v /home/laptq/Downloads:/home/laptq/Downloads \
    \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    -v ${HOME}/.Xauthority:/root/.Xauthority \
    -e DISPLAY=$DISPLAY \
    -e QT_X11_NO_MITSHM=1 \
    \
    --gpus all \
    --runtime nvidia \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    \
    -e PATHD_NVDS=/opt/nvidia/deepstream/deepstream-7.0 \
    \
    --network=host \
    --privileged \
    \
    --workdir $( pwd ) \
    \
    --name hello-gstreamer \
    nvcr.io/nvidia/deepstream:7.0-samples-multiarch


