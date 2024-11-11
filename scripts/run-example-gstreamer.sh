PATH_SRC=gstreamer-examples/ex-1-hello-video.c
DIR_OUTPUT=outputs
mkdir -p $DIR_OUTPUT

gcc \
    -o $DIR_OUTPUT/main.out \
    $PATH_SRC \
    `pkg-config --cflags --libs gstreamer-1.0` \

./${DIR_OUTPUT}/main.out
