DIR_OUTPUT=outputs
mkdir -p $DIR_OUTPUT

gcc \
    -o $DIR_OUTPUT/main.out \
    examples/ex-1.c \
    `pkg-config --cflags --libs gstreamer-1.0` \

./${DIR_OUTPUT}/main.out
