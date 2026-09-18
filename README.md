## Environment

Clone and navigate to the repo
```
git clone https://github.com/lap-tran_awl/hello-gstreamer
cd hello-gstreamer
```

Start docker:
```bash
docker run \
    -it \
    -d \
    -v "$(pwd):/hello-gstreamer" \
    --workdir /hello-gstreamer \
    --gpus all \
    --runtime nvidia \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    --privileged \
    --name laptq_ds \
    nvcr.io/nvidia/deepstream:7.0-triton-multiarch
```