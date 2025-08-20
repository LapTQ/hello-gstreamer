# wget --content-disposition 'https://api.ngc.nvidia.com/v2/resources/org/nvidia/gxf_and_gc/4.1.0/files?redirect=true&path=deepstream-reference-graphs-7.1.deb' -o 'deepstream-reference-graphs-7.1.deb'
# wget --content-disposition 'https://api.ngc.nvidia.com/v2/resources/org/nvidia/gxf_and_gc/4.1.0/files?redirect=true&path=graph_composer-4.1.0_x86_64.deb' -o 'graph_composer-4.1.0_x86_64.deb'


# dpkg -i deepstream-reference-graphs-7.1.deb

cd /opt/nvidia/deepstream/deepstream/reference_graphs/deepstream-test1
# /opt/nvidia/graph-composer/execute_graph.sh \
#     deepstream-test1.yaml \
#     parameters.yaml \
#     -d ../common/target_x86_64.yaml

container_builder build -c ds_test1_container_builder_dgpu.yaml \
        -d x86 -wd $(pwd)