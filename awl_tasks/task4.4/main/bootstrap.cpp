#include "main/bootstrap.h"
#include "main/settings.h"

#include "domain/ports/repo.h"

#include "app/services/output_parser.h"
#include "app/services/pipeline_utils.h"
#include "app/services/visualize_utils.h"

#include "infra/action_state/action_state_utils.h"
#include "infra/detection/detection_utils.h"
#include "infra/person_view/person_view_utils.h"
#include "infra/repo/fake.h"

#include <gst/gst.h>
#include <optional>
#include <stdexcept>
#include <memory>


std::unique_ptr<IObjectRepo> _create_repo(Settings settings) {
    if (settings.repo_settings.type == "fake") {
        return std::make_unique<FakeObjectRepo>();
    } else {
        throw std::invalid_argument("Unsupported repo type: '" + settings.repo_settings.type + "'. Expected 'fake'.");
    }
}


GstElement* _create_source_bin(Settings settings) {
    GstElement* source {
        create_multi_source_bin(
            "multi_source_bin", 
            settings.source_settings.uris, 
            settings.source_settings.width, 
            settings.source_settings.height, 
            settings.source_settings.live_source
        ) 
    };

    return source;
}


GstElement* _create_pgie_detection(Settings settings) {
    GstElement* detector { gst_element_factory_make("nvinfer", "detector") };
    g_object_set(
        G_OBJECT(detector),
        "config-file-path", settings.detector_settings.config_file_path.c_str(),
        NULL
    );      // 1 vài thuộc tính trong file config có thể được ghi đè thông qua Gst Properties

    return detector;
}


GstElement* _create_tracker(Settings settings) {
    GstElement* tracker { gst_element_factory_make("nvtracker", "tracker") };
    g_object_set(
        G_OBJECT(tracker),
        "tracker-width", settings.tracker_settings.tracker_width,
        "tracker-height", settings.tracker_settings.tracker_height,
        "gpu-id", settings.tracker_settings.gpu_id,
        "ll-lib-file", settings.tracker_settings.ll_lib_file.c_str(),
        "ll-config-file", settings.tracker_settings.ll_config_file.c_str(),
        NULL
    );

    return tracker;
}


GstElement* _create_sgie_person_view(Settings settings) {
    GstElement* person_view_classifier { gst_element_factory_make("nvinfer", "person_view_classifier") };
    g_object_set(
        G_OBJECT(person_view_classifier),
        "config-file-path", settings.person_view_settings.config_file_path.c_str(),
        NULL
    );

    return person_view_classifier;
}


GstElement* _create_sgie_action_state(Settings settings) {
    GstElement* action_state_classifier { gst_element_factory_make("nvinfer", "action_state_classifier") };
    g_object_set(
        G_OBJECT(action_state_classifier),
        "config-file-path", settings.action_state_settings.config_file_path.c_str(),
        NULL
    );

    // Cấp phát processor
    ActionStateProcessor* processor = new ActionStateProcessor(
        settings.action_state_settings.width_ratio,
        settings.action_state_settings.height_ratio,
        settings.action_state_settings.classes_to_rescale,
        settings.action_state_settings.class_offset
    );
    GstPad* sinkpad { gst_element_get_static_pad(action_state_classifier, "sink") };
    gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER, preprocess_for_actionstate, processor, NULL);
    g_object_unref(sinkpad);
    // Gắn probe hậu xử lý (srcpad)
    GstPad* srcpad { gst_element_get_static_pad(action_state_classifier, "src") };
    gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER, postprocess_for_actionstate, processor, NULL);
    g_object_unref(srcpad);

    // Trao vòng đời của processor vào element để delete khi element bị giải phóng
    g_object_set_data_full(
        G_OBJECT(action_state_classifier),
        "action_state_processor",
        processor,
        [](gpointer data) { delete static_cast<ActionStateProcessor*>(data); }
    );

    return action_state_classifier;
}


GstElement* _create_analytics_tap(Settings settings, IObjectRepo& object_repo) {
    GstElement* analytics_tap { gst_element_factory_make("identity", "analytics_tap") };

    auto* detection_parser = new DetectionParser(
        settings.detector_settings.classes
    );
    auto* person_view_parser = new PersonViewParser(
        settings.person_view_settings.gie_unique_id,
        settings.person_view_settings.output_layer_name,
        settings.person_view_settings.views
    );
    auto* action_state_parser = new ActionStateParser(
        settings.action_state_settings.gie_unique_id,
        settings.action_state_settings.output_layer_name,
        settings.action_state_settings.actions
    );
    auto* output_parser = new OutputParser(
        *detection_parser,
        *person_view_parser,
        *action_state_parser,
        object_repo
    );

    GstPad* srcpad { gst_element_get_static_pad(analytics_tap, "src") };
    gst_pad_add_probe(srcpad, GST_PAD_PROBE_TYPE_BUFFER, parse_pipeline_output, output_parser, NULL);
    g_object_unref(srcpad);

    // Gắn vòng đời của từng parser vào element
    g_object_set_data_full(
        G_OBJECT(analytics_tap),
        "detection_parser",
        detection_parser,
        [](gpointer data) { delete static_cast<DetectionParser*>(data); }
    );
    g_object_set_data_full(
        G_OBJECT(analytics_tap),
        "person_view_parser",
        person_view_parser,
        [](gpointer data) { delete static_cast<PersonViewParser*>(data); }
    );
    g_object_set_data_full(
        G_OBJECT(analytics_tap),
        "action_state_parser",
        action_state_parser,
        [](gpointer data) { delete static_cast<ActionStateParser*>(data); }
    );
    g_object_set_data_full(
        G_OBJECT(analytics_tap),
        "output_parser",
        output_parser,
        [](gpointer data) { delete static_cast<OutputParser*>(data); }
    );

    return analytics_tap;
}


GstElement* _create_nvdslogger(Settings settings) {
    GstElement* nvdslogger { gst_element_factory_make("nvdslogger", "nvdslogger") };
    g_object_set(
        G_OBJECT(nvdslogger),
        "fps-measurement-interval-sec", settings.logger_settings.fps_measurement_interval_sec,
        NULL
    );

    return nvdslogger;
}


GstElement* _create_osd(Settings settings, IObjectRepo& object_repo) {
    GstElement* osd { create_osd(settings.osd_settings.width, settings.osd_settings.height) };

    auto* visualizer = new Visualizer(
        object_repo,
        settings.visualizer_settings.view_labels,
        settings.visualizer_settings.action_labels
    );

    GstPad* sinkpad { gst_element_get_static_pad(osd, "sink") };
    gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER, visualize, visualizer, NULL);
    g_object_unref(sinkpad);

    g_object_set_data_full(
        G_OBJECT(osd),
        "visualizer",
        visualizer,
        [](gpointer data) { delete static_cast<Visualizer*>(data); }
    );

    return osd;
}


GstElement* _create_sink(Settings settings) {
    if (settings.sink_settings.type == "display") {
        return gst_element_factory_make("nveglglessink", "sink");
    } else if (settings.sink_settings.type == "fakesink") {
        return gst_element_factory_make("fakesink", "sink");
    } else if (settings.sink_settings.type == "mp4") {
        return create_mp4_filesink("mp4_filesink_bin", settings.sink_settings.output_path);
    } else {
        throw std::invalid_argument("Unsupported sink type: '" + settings.sink_settings.type + "'. Expected 'mp4', 'display', or 'fakesink'.");
    }
}


std::optional<BootstrapRet> bootstrap(Settings settings) {
    std::unique_ptr<IObjectRepo> object_repo { _create_repo(settings) };

    GstElement* source { _create_source_bin(settings) };
    GstElement* detector { _create_pgie_detection(settings) };
    GstElement* tracker { _create_tracker(settings) };
    GstElement* person_view_classifier { _create_sgie_person_view(settings) };
    GstElement* action_state_classifier { _create_sgie_action_state(settings) };
    GstElement* analytics_tap { _create_analytics_tap(settings, *object_repo) };
    GstElement* nvdslogger { _create_nvdslogger(settings) };
    GstElement* osd { _create_osd(settings, *object_repo) };
    GstElement* sink { _create_sink(settings) };

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
        g_printerr("Link error in bootstrap pipeline\n");
        g_object_unref(pipeline);
        return std::nullopt;
    }

    GstBus* bus { gst_element_get_bus(pipeline) };
    GMainLoop* loop { g_main_loop_new(NULL, FALSE) };
    gst_bus_add_watch(bus, handle_bus_message, loop);
    gst_object_unref(bus);

    BootstrapRet ret {
        pipeline,
        loop,
        std::move(object_repo),
    };

    return ret;
}

