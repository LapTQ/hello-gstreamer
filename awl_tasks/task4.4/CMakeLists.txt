cmake_minimum_required(VERSION 3.16)
project(Task4.4 CXX)

# -std=c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fdiagnostics-color=always -O2 -Wall -Wextra -DNDEBUG -pedantic-errors -Wconversion -Wsign-conversion)

# 3. `pkg-config --cflags --libs gstreamer-1.0` (tìm kiếm thư viện tự động qua pkg-config)
# Lệnh trên thực chất sẽ chạy terminal và in ra -I... -l...
find_package(PkgConfig REQUIRED)                    # Nạp công cụ PkgConfig của CMake
pkg_check_modules(GST REQUIRED gstreamer-1.0)       # Tìm gói "gstreamer-1.0"
# Chữ GST là tiền tố do bạn tự đặt tên, Sau khi tìm thấy gói, CMake sẽ tự động lưu kết quả vào 2 biến: 
# - GST_INCLUDE_DIRS => để bạn cho vào target_include_directories
# - GST_LIBRARIES => cho vào target_link_libraries

# 4. Định nghĩa Target thực thi (main.out)
add_executable(main.out
    main/main.cpp
    main/bootstrap.cpp
)

# 5. -I...
target_include_directories(main.out PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}                          # awl_tasks/task4.4
    /opt/nvidia/deepstream/deepstream/sources/includes   # DeepStream headers
    ${CMAKE_CURRENT_SOURCE_DIR}/../../outputs/yaml-cpp/install/include    # yaml-cpp headers
    ${GST_INCLUDE_DIRS}                                  # GStreamer headers
)

# 6. -L...
target_link_directories(main.out PRIVATE
    /opt/nvidia/deepstream/deepstream/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../outputs/yaml-cpp/install/lib
)

# 7. -l...
target_link_libraries(main.out PRIVATE
    ${GST_LIBRARIES}
    yaml-cpp
    nvdsgst_meta
    nvds_meta
)

# -Wl,-rpath,...
set_target_properties(main.out PROPERTIES
    BUILD_RPATH "/opt/nvidia/deepstream/deepstream/lib;${CMAKE_CURRENT_SOURCE_DIR}/../../outputs/yaml-cpp/install/lib"
)