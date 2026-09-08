set -e

# Auto-detect GStreamer pkg-config path on macOS if not already in search path
if ! pkg-config --exists gstreamer-1.0 2>/dev/null; then
    if [ -d "/Library/Frameworks/GStreamer.framework/Versions/Current/lib/pkgconfig" ]; then
        export PKG_CONFIG_PATH="/Library/Frameworks/GStreamer.framework/Versions/Current/lib/pkgconfig:${PKG_CONFIG_PATH}"
    elif [ -d "/opt/homebrew/lib/pkgconfig" ]; then
        export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:${PKG_CONFIG_PATH}"
    elif [ -d "/usr/local/lib/pkgconfig" ]; then
        export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}"
    fi
fi

# PATH__FILE__SRC=gstreamer-examples/c/ex-1-hello-video.c
# PATH__FILE__SRC=gstreamer-examples/c/ex-2-manual-hello-world.c
# PATH__FILE__SRC=gstreamer-examples/c/ex-3-dynamic-pipeline.c
PATH__FILE__SRC=src/gstreamer/1.playvideo.c
PATH__DIR__OUTPUT=outputs
mkdir -p $PATH__DIR__OUTPUT

gcc \
    -o $PATH__DIR__OUTPUT/main.out \
    $PATH__FILE__SRC \
    `pkg-config --cflags --libs gstreamer-1.0`

./${PATH__DIR__OUTPUT}/main.out

