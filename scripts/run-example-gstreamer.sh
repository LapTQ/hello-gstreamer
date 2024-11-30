# PATH__FILE__SRC=gstreamer-examples/c/ex-1-hello-video.c
# PATH__FILE__SRC=gstreamer-examples/c/ex-2-manual-hello-world.c
PATH__FILE__SRC=gstreamer-examples/c/ex-3-dynamic-pipeline.c
PATH__DIR__OUTPUT=outputs
mkdir -p $PATH__DIR__OUTPUT

gcc \
    -o $PATH__DIR__OUTPUT/main.out \
    $PATH__FILE__SRC \
    `pkg-config --cflags --libs gstreamer-1.0` \

./${PATH__DIR__OUTPUT}/main.out
