set -e

PATH__FILE__SRC=awl_tasks/task2.2/main.cpp
PATH__DIR__OUTPUT=outputs
mkdir -p $PATH__DIR__OUTPUT

# generate custom YOLO nvinfer lib
YOLO_LIB=assets/libreyolo9/libnvdsinfer_custom_impl_Yolo.so
if [ ! -f "$YOLO_LIB" ]; then
    echo "File $YOLO_LIB does not exist. Preparing to build..."
    mkdir -p assets/libreyolo9

    if [ ! -d "outputs/DeepStream-Yolo" ]; then
        echo "Cloning DeepStream-Yolo into outputs/..."
        git clone https://github.com/marcoslucianops/DeepStream-Yolo.git outputs/DeepStream-Yolo
    fi

    export CUDA_VER=12.2
    make -C outputs/DeepStream-Yolo/nvdsinfer_custom_impl_Yolo clean
    make -C outputs/DeepStream-Yolo/nvdsinfer_custom_impl_Yolo
    cp outputs/DeepStream-Yolo/nvdsinfer_custom_impl_Yolo/libnvdsinfer_custom_impl_Yolo.so "$YOLO_LIB"
    echo "Successfully built and copied $YOLO_LIB"
fi

# convert ONNX -> TRT
WEIGHT=assets/libreyolo9/LibreYOLO9t.onnx.fp16_max100.trt
if [ ! -f "$WEIGHT" ]; then
    echo "File $WEIGHT does not exist. Preparing to convert..."
    mkdir -p assets/libreyolo9

    /usr/src/tensorrt/bin/trtexec \
        --onnx=assets/libreyolo9/LibreYOLO9t.onnx \
        --saveEngine=$WEIGHT \
        --memPoolSize=workspace:6400 \
        --tacticSources=-cublasLt,+cublas \
        --sparsity=disable --verbose \
        --minShapes=input:1x3x640x640 \
        --optShapes=input:10x3x640x640 \
        --maxShapes=input:100x3x640x640 \
        --fp16
fi


# export GST_DEBUG=2

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

