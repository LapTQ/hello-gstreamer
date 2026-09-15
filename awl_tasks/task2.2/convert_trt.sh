/usr/src/tensorrt/bin/trtexec \
    --onnx=assets/LibreYOLO9t.onnx \
    --saveEngine=assets/LibreYOLO9t.onnx.fp16_max100.engine \
    --memPoolSize=workspace:6400 \
    --tacticSources=-cublasLt,+cublas \
    --sparsity=disable --verbose \
    --minShapes=input:1x3x640x640 \
    --optShapes=input:10x3x640x640 \
    --maxShapes=input:100x3x640x640 \
    --fp16