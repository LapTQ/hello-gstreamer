# 1. Clone source code
cd outputs

git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
mkdir build && cd build

# 2. Chỉ định thư mục cài đặt qua CMAKE_INSTALL_PREFIX (dùng đường dẫn tuyệt đối)
INSTALL_DIR="/hello-gstreamer/outputs/yaml-cpp/install"

cmake .. \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DYAML_BUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release

# 3. Biên dịch và copy file vào thư mục đích (không cần sudo)
make -j$(nproc)
make install