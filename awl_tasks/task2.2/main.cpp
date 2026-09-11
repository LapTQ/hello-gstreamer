#include "gst/gstelement.h"
#include "gst/gstelementfactory.h"
#include "gst/gstpad.h"
#include "gst/gstpipeline.h"
#include <cstddef>
#include<gst/gst.h>
#include<vector>
#include<string>

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        "assets/video_1.mp4",
        "rtsp://admin:12345@192.168.3.27/live"
    };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    GstElement* streammux { gst_element_factory_make("nvstreammux", "streammux") };

    gboolean _link_success { false };
    for (std::size_t i_u {0}; i_u < list_uris.size(); ++i_u) {
        GstElement* source { gst_element_factory_make("nvurisrcbin", ("source" + std::to_string(i_u)).c_str())};
        GstPad* srcpad { gst_element_get_static_pad(source, "source") };
        GstPad* sinkpad { gst_element_request_pad_simple(streammux, ("sink_" + std::to_string(i_u)).c_str())};

        _link_success = gst_pad_link(srcpad, sinkpad);
        if (GST_PAD_LINK_FAILED(_link_success)) {
            
        }

        gst_bin_add(GST_BIN(pipeline), source);
    }

}