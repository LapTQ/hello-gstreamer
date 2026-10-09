#ifndef SETTINGS_H
#define SETTINGS_H

#include "domain/entities/action_state.h"
#include "domain/entities/detection_class.h"
#include "domain/entities/person_view.h"

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>
#include <unordered_map>

struct SourceSettings {
    std::vector<std::string> uris {};
    int width {};
    int height {};
    bool live_source {};
};

struct DetectorSettings {
    std::string config_file_path {};
    std::unordered_map<int, DetectionClassType> classes {};
};

struct TrackerSettings {
    int tracker_width {};
    int tracker_height {};
    int gpu_id {};
    std::string ll_lib_file {};
    std::string ll_config_file {};
};

struct PersonViewSettings {
    std::string config_file_path {};
    unsigned int gie_unique_id {};
    std::string output_layer_name {};
    std::unordered_map<unsigned int, ViewType> views {};
};

struct ActionStateSettings {
    std::string config_file_path {};
    unsigned int gie_unique_id {};
    std::string output_layer_name {};
    std::unordered_map<unsigned int, ActionStateType> actions {};
    float width_ratio {};
    float height_ratio {};
    std::vector<int> classes_to_rescale {};
    unsigned int class_offset {};
};

struct LoggerSettings {
    int fps_measurement_interval_sec {};
};

struct OsdSettings {
    int width {};
    int height {};
};

struct SinkSettings {
    std::string type {};
    std::string output_path {};
};

struct RepoSettings {
    std::string type {};
};

struct VisualizerSettings {
    std::unordered_map<ViewType, std::string> view_labels {};
    std::unordered_map<ActionStateType, std::string> action_labels {};
};

struct Settings {
    SourceSettings source_settings {};
    DetectorSettings detector_settings {};
    TrackerSettings tracker_settings {};
    PersonViewSettings person_view_settings {};
    ActionStateSettings action_state_settings {};
    LoggerSettings logger_settings {};
    OsdSettings osd_settings {};
    SinkSettings sink_settings {};
    RepoSettings repo_settings {};
    VisualizerSettings visualizer_settings {};

    static Settings from_yaml(const std::string& path);
};


// ================================ YAML to Settings =====================================

inline DetectionClassType string_to_detection_class(const std::string& str) {
    static const std::unordered_map<std::string, DetectionClassType> table = {
        {"PERSON", DetectionClassType::PERSON}
    };
    return table.at(str);
}

inline ViewType string_to_view_type(const std::string& str) {
    static const std::unordered_map<std::string, ViewType> table = {
        {"FRONT", ViewType::FRONT},
        {"SIDE",  ViewType::SIDE},
        {"BACK",  ViewType::BACK}
    };
    return table.at(str);
}

inline ActionStateType string_to_action_state_type(const std::string& str) {
    static const std::unordered_map<std::string, ActionStateType> table = {
        {"HAND_INTO_PANT_POCKET",  ActionStateType::HAND_INTO_PANT_POCKET},
        {"HAND_INTO_SHIRT_POCKET", ActionStateType::HAND_INTO_SHIRT_POCKET},
        {"HAND_INTO_BAG",          ActionStateType::HAND_INTO_BAG},
        {"HAND_INTO_BASKET",       ActionStateType::HAND_INTO_BASKET},
        {"HAND_INTO_SHELF",        ActionStateType::HAND_INTO_SHELF},
        {"HOLDING_PRODUCT",        ActionStateType::HOLDING_PRODUCT},
        {"NOT_HOLDING_PRODUCT",    ActionStateType::NOT_HOLDING_PRODUCT}
    };
    return table.at(str);
}

inline Settings Settings::from_yaml(const std::string& path) {
    YAML::Node node { YAML::LoadFile(path) };
    Settings settings {};

    // 1. Source
    auto source = node["source"];
    settings.source_settings.uris = source["uris"].as<std::vector<std::string>>();
    settings.source_settings.width = source["width"].as<int>();
    settings.source_settings.height = source["height"].as<int>();
    settings.source_settings.live_source = source["live_source"].as<bool>();

    // 2. Detector
    auto detector = node["detector"];
    settings.detector_settings.config_file_path = detector["config_file_path"].as<std::string>();
    for (const auto& kv : detector["classes"]) {
        settings.detector_settings.classes[kv.first.as<int>()] = string_to_detection_class(kv.second.as<std::string>());
    }

    // 3. Tracker
    auto tracker = node["tracker"];
    settings.tracker_settings.tracker_width = tracker["tracker_width"].as<int>();
    settings.tracker_settings.tracker_height = tracker["tracker_height"].as<int>();
    settings.tracker_settings.gpu_id = tracker["gpu_id"].as<int>();
    settings.tracker_settings.ll_lib_file = tracker["ll_lib_file"].as<std::string>();
    settings.tracker_settings.ll_config_file = tracker["ll_config_file"].as<std::string>();

    // 4. Person view
    auto person_view = node["person_view"];
    settings.person_view_settings.config_file_path = person_view["config_file_path"].as<std::string>();
    settings.person_view_settings.gie_unique_id = person_view["gie_unique_id"].as<unsigned int>();
    settings.person_view_settings.output_layer_name = person_view["output_layer_name"].as<std::string>();
    for (const auto& kv : person_view["views"]) {
        settings.person_view_settings.views[kv.first.as<unsigned int>()] = string_to_view_type(kv.second.as<std::string>());
    }

    // 5. Action state (includes processor)
    auto action_state = node["action_state"];
    settings.action_state_settings.config_file_path = action_state["config_file_path"].as<std::string>();
    settings.action_state_settings.gie_unique_id = action_state["gie_unique_id"].as<unsigned int>();
    settings.action_state_settings.output_layer_name = action_state["output_layer_name"].as<std::string>();
    settings.action_state_settings.width_ratio = action_state["width_ratio"].as<float>();
    settings.action_state_settings.height_ratio = action_state["height_ratio"].as<float>();
    settings.action_state_settings.classes_to_rescale = action_state["classes_to_rescale"].as<std::vector<int>>();
    settings.action_state_settings.class_offset = action_state["class_offset"].as<unsigned int>();
    for (const auto& kv : action_state["actions"]) {
        settings.action_state_settings.actions[kv.first.as<unsigned int>()] = string_to_action_state_type(kv.second.as<std::string>());
    }

    // 6. Logger
    settings.logger_settings.fps_measurement_interval_sec = node["logger"]["fps_measurement_interval_sec"].as<int>();

    // 7. OSD
    auto osd = node["osd"];
    settings.osd_settings.width = osd["width"].as<int>();
    settings.osd_settings.height = osd["height"].as<int>();

    // 8. Sink
    auto sink = node["sink"];
    settings.sink_settings.type = sink["type"].as<std::string>();
    settings.sink_settings.output_path = sink["output_path"].as<std::string>();

    // 9. Repo
    settings.repo_settings.type = node["repo"]["type"].as<std::string>();

    // 10. Visualizer
    auto visualizer = node["visualizer"];
    for (const auto& kv : visualizer["view_labels"]) {
        settings.visualizer_settings.view_labels[string_to_view_type(kv.first.as<std::string>())] = kv.second.as<std::string>();
    }
    for (const auto& kv : visualizer["action_labels"]) {
        settings.visualizer_settings.action_labels[string_to_action_state_type(kv.first.as<std::string>())] = kv.second.as<std::string>();
    }

    return settings;
}

#endif