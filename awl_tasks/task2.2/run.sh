PATH__FILE__SRC=awl_tasks/task2.2/main.cpp
PATH__DIR__OUTPUT=outputs
mkdir -p $PATH__DIR__OUTPUT

# # generate custom YOLO nvinfer lib
# export CUDA_VER=12.2
# cd externals/DeepStream-Yolo
# make -C nvdsinfer_custom_impl_Yolo clean && make -C nvdsinfer_custom_impl_Yolo
# mv nvdsinfer_custom_impl_Yolo/libnvdsinfer_custom_impl_Yolo.so ../../assets/libreyolo9/
# cd ../..

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

