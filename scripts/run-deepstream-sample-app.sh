# guide for Sample App 1 is in /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test1/README

# ==================================== Outside container ====================================
# xhost +


# ==================================== Inside container ====================================


# ------------- Sample app test 1 -------------
# cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test1
# export CUDA_VER=12.6
# make

# # run, either of 2 ways:
# # ./deepstream-test1-app /opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.h264
# ./deepstream-test1-app dstest1_config.yml

# make clean


# ------------- Sample app test 2 -------------
# cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test2
# export CUDA_VER=12.6
# make

# # run, either of 2 ways:
# # ./deepstream-test2-app /opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.h264
# ./deepstream-test2-app dstest2_config.yml

# make clean


# ------------- Sample app test 3 -------------
cd /opt/nvidia/deepstream/deepstream/sources/apps/sample_apps/deepstream-test3
export CUDA_VER=12.6
make

# run, either of 2 ways:
# ./deepstream-test3-app file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_720p.h264 file:///opt/nvidia/deepstream/deepstream/samples/streams/sample_qHD.h264
./deepstream-test3-app dstest3_config.yml

make clean