#include "domain/entities/action_state_type.h"
#include "domain/entities/detection_class_type.h"
#include "domain/entities/person_view_type.h"

#include "app/services/output_parser.h"
#include "app/services/pipeline_utils.h"
#include "app/services/visualize_utils.h"

#include "infra/action_state/action_state_utils.h"
#include "infra/detection/detection_utils.h"
#include "infra/person_view/person_view_utils.h"
#include "infra/repo/repo.h"

#include <gst/gst.h>

#include <string>
#include <unordered_map>
#include <vector>


int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    std::vector<std::string> list_uris {
        // "file:///home/laptq/hello-gstreamer/assets/sample_720p.h264",
        "rtsp://admin:12345@192.168.3.26/live",
        "rtsp://admin:12345@192.168.3.21/live"
    };

    GstElement* source { 
        create_multi_source_bin(
            "multi_source_bin", 
            list_uris, 
            960, 
            640, 
            true
        ) 
    };

    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", "awl_tasks/task4.4/nvinfer_detector_config_file.yml",
        NULL
    );      // 1 vài thuộc tính trong file config có thể được ghi đè thông qua Gst Properties

    GstElement* tracker { gst_element_factory_make("nvtracker", "tracker") };
    g_object_set(
        G_OBJECT(tracker),
        "tracker-width", 640,
        "tracker-height", 640,
        "gpu-id", 0,
        "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
        "ll-config-file", "/opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
        NULL
    );

    GstElement* person_view_classifier { gst_element_factory_make("nvinfer", "person_view_classifier") };
    g_object_set(
        G_OBJECT(person_view_classifier),
        "config-file-path", "awl_tasks/task4.4/nvinfer_person_view_config_file.yml",
        NULL
    );

    GstElement* action_state_classifier { gst_element_factory_make("nvinfer", "action_state_classifier") };
    g_object_set(
        G_OBJECT(action_state_classifier),
        "config-file-path", "awl_tasks/task4.4/nvinfer_action_state_config_file.yml",
        NULL
    );

    GstElement* analytics_tap { gst_element_factory_make("identity", "analytics_tap") };

    GstElement* nvdslogger { gst_element_factory_make ("nvdslogger", "nvdslogger") };
    g_object_set(G_OBJECT(nvdslogger), "fps-measurement-interval-sec", 1, NULL);

    
    GstElement* osd { create_osd(1920, 640) };
    
    // GstElement* sink { gst_element_factory_make("nveglglessink", "sink") };
    GstElement* sink { create_mp4_filesink("mp4_filesink_bin", "outputs/output.mp4") };
    // GstElement* sink { gst_element_factory_make("fakesink", "sink") };

    GstElement* pipeline { gst_pipeline_new("pipeline") };

    gst_bin_add_many(
        GST_BIN(pipeline),
        source,
        detector,
        tracker,
        person_view_classifier,
        action_state_classifier,
        analytics_tap,
        nvdslogger,
        osd,
        sink,
        NULL
    );

    gboolean _link_success { false };
    _link_success = gst_element_link_many(
        source,
        detector,
        tracker,
        person_view_classifier,
        action_state_classifier,
        analytics_tap,
        nvdslogger,
        osd,
        sink,
        NULL
    );
    if (_link_success != TRUE) {
        g_printerr("Link error\n");
        g_object_unref(pipeline);
        return -1;
    }

    // tiền/hậu xử lý cho action-state
    ActionStateProcessor actionstate_processor {
        1.3f, 1.1f,
        std::vector<gint> {0},
        1000
    };
    GstPad* actionstate_sinkpad { gst_element_get_static_pad(action_state_classifier, "sink") };
    gst_pad_add_probe(actionstate_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, preprocess_for_actionstate, &actionstate_processor, NULL);
    g_object_unref(actionstate_sinkpad);
    // bỏ gắn probe này nếu muốn vẽ lên osd
    GstPad* actionstate_srcpad { gst_element_get_static_pad(action_state_classifier, "src") };
    gst_pad_add_probe(actionstate_srcpad, GST_PAD_PROBE_TYPE_BUFFER, postprocess_for_actionstate, &actionstate_processor, NULL);
    g_object_unref(actionstate_srcpad);

    
    // parse model output
    DetectionParser detection_parser {
        std::unordered_map<gint, DetectionClassType> {
            {0, DetectionClassType::PERSON},
            // {1000, DetectionClassType::PERSON},  // bật nếu muốn vẽ lên osd
        } 
    }; // class_index = 0 (person)
    PersonViewParser person_view_parser {
        2, // gie_unique_id
        "output", // output_layer_name
        {
            {0, ViewType::FRONT},
            {1, ViewType::SIDE},
            {2, ViewType::BACK}
        }
    };
    ActionStateParser action_state_parser {
        3, // gie_unique_id
        "output", // output_layer_name
        {
            {0, ActionStateType::HAND_INTO_PANT_POCKET},
            {1, ActionStateType::HAND_INTO_SHIRT_POCKET},
            {2, ActionStateType::HAND_INTO_BAG},
            {3, ActionStateType::HAND_INTO_BASKET},
            {4, ActionStateType::HAND_INTO_SHELF},
            {5, ActionStateType::HOLDING_PRODUCT},
            {6, ActionStateType::NOT_HOLDING_PRODUCT},
        }
    };
    FakeObjectRepo fake_person_repo {};
    OutputParser parser { detection_parser, person_view_parser, action_state_parser, fake_person_repo };
    Visualizer visualizer { 
        fake_person_repo, 
        {
            {ViewType::FRONT, "Front"}, 
            {ViewType::SIDE, "Side"}, 
            {ViewType::BACK, "Back"}
        },
        {
            {ActionStateType::HAND_INTO_PANT_POCKET, "Hand->pant"},
            {ActionStateType::HAND_INTO_SHIRT_POCKET, "Hand->shirt"},
            {ActionStateType::HAND_INTO_BAG, "Hand->bag"},
            {ActionStateType::HAND_INTO_BASKET, "Hand->basket"},
            {ActionStateType::HAND_INTO_SHELF, "Hand->shelf"},
            {ActionStateType::HOLDING_PRODUCT, "Hold product"},
            {ActionStateType::NOT_HOLDING_PRODUCT, "Not hold product"},
        },
    };
    GstPad* analytics_tap_srcpad { gst_element_get_static_pad(analytics_tap, "src") };
    gst_pad_add_probe(analytics_tap_srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_pipeline_output, &parser, NULL);
    g_object_unref(analytics_tap_srcpad);

    // visualize OSD
    GstPad* osd_sinkpad { gst_element_get_static_pad(osd, "sink") };
    gst_pad_add_probe(osd_sinkpad, GST_PAD_PROBE_TYPE_BUFFER, visualize, &visualizer, NULL);
    g_object_unref(osd_sinkpad);

    GstStateChangeReturn _change_state { gst_element_set_state(pipeline, GST_STATE_PLAYING) };
    if (_change_state == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Pipeline playing error\n");
        g_object_unref(pipeline);
        return -1;
    }

    g_print("Pipeline running...\n");
    
    GstBus* bus { gst_element_get_bus(pipeline) };
    GMainLoop* loop { g_main_loop_new(NULL, FALSE) };
    gst_bus_add_watch(bus, handle_bus_message, loop);
    gst_object_unref(bus);
    
    // Khóa thread ở đây
    g_main_loop_run(loop);
    // Code chỉ chạy tiếp khi g_main_loop_quit() được gọi

    gst_element_set_state(pipeline, GST_STATE_NULL);
    g_object_unref(pipeline);
    g_main_loop_unref(loop);
}