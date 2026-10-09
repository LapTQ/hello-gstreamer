set -e

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

# convert Detection ONNX -> TRT
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


# convert PersonView ONNX -> TRT
WEIGHT=assets/personview/MobilenetV3_small_224x224_Satudora10k_base_Imagenet_dynamic.onnx.fp16_max100.trt
if [ ! -f "$WEIGHT" ]; then
    echo "File $WEIGHT does not exist. Preparing to convert..."
    mkdir -p assets/personview

    /usr/src/tensorrt/bin/trtexec \
        --onnx=assets/personview/MobilenetV3_small_224x224_Satudora10k_base_Imagenet_dynamic.onnx \
        --saveEngine=$WEIGHT \
        --memPoolSize=workspace:6400 \
        --tacticSources=-cublasLt,+cublas \
        --sparsity=disable --verbose \
        --minShapes=input:1x3x224x224 \
        --optShapes=input:10x3x224x224 \
        --maxShapes=input:100x3x224x224 \
        --fp16
fi


# convert ActionState ONNX -> TRT
WEIGHT=assets/action_state/best-epoch=62-val_f1=0.571.ckpt.onnx.fp16_max100.trt
if [ ! -f "$WEIGHT" ]; then
    echo "File $WEIGHT does not exist. Preparing to convert..."
    mkdir -p assets/action_state

    /usr/src/tensorrt/bin/trtexec \
        --onnx=assets/action_state/best-epoch=62-val_f1=0.571.ckpt.onnx \
        --saveEngine=$WEIGHT \
        --memPoolSize=workspace:6400 \
        --tacticSources=-cublasLt,+cublas \
        --sparsity=disable --verbose \
        --minShapes=input:1x3x224x224 \
        --optShapes=input:10x3x224x224 \
        --maxShapes=input:100x3x224x224 \
        --fp16
fi

# build and install yaml-cpp
YAML_CPP_LIB="${PATH__DIR__OUTPUT}/yaml-cpp/install/lib/libyaml-cpp.so"
if [ ! -f "$YAML_CPP_LIB" ]; then
    echo "File $YAML_CPP_LIB does not exist. Preparing to build and install yaml-cpp..."
    if [ ! -d "${PATH__DIR__OUTPUT}/yaml-cpp" ]; then
        echo "Cloning yaml-cpp into ${PATH__DIR__OUTPUT}/yaml-cpp..."
        git clone https://github.com/jbeder/yaml-cpp.git "${PATH__DIR__OUTPUT}/yaml-cpp"
    fi

    INSTALL_DIR="$(pwd)/${PATH__DIR__OUTPUT}/yaml-cpp/install"

    cmake \
        -B "${PATH__DIR__OUTPUT}/yaml-cpp/build" \
        -S "${PATH__DIR__OUTPUT}/yaml-cpp" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DYAML_BUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release

    cmake --build "${PATH__DIR__OUTPUT}/yaml-cpp/build" -j$(nproc)
    cmake --build "${PATH__DIR__OUTPUT}/yaml-cpp/build" --target install
    echo "Successfully built and installed yaml-cpp to $INSTALL_DIR"
fi

# export GST_DEBUG=2

# -S: thư mục chứa CMakeLists.txt
# -B: thư mục build
cmake \
    -S awl_tasks/task4.4 \
    -B $PATH__DIR__OUTPUT/build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \

cmake --build $PATH__DIR__OUTPUT/build -j$(nproc)

./$PATH__DIR__OUTPUT/build/main.out

