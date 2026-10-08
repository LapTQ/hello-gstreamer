#ifndef OBJECT_REPO_H
#define OBJECT_REPO_H

#include "domain/entities/object.h"

#include <gst/gst.h>

#include <unordered_map>
#include <vector>
#include <algorithm>

class FakeObjectRepo {
private:
    std::unordered_map<guint, std::unordered_map<gint, std::vector<Object>>> _data {};
public:

    void add(Object obj) {
        this->_data[obj.camera_id][obj.frame_num].push_back(obj);
    }

    std::vector<Object> get_objects_by_frame(guint camera_id, gint frame_num, std::optional<std::vector<DetectionClassType>> class_filter = std::nullopt) {
        std::vector<Object> objects {};
        for (Object obj: this->_data[camera_id][frame_num]) {
            if (class_filter.has_value()) {
                if (std::find(class_filter.value().begin(), class_filter.value().end(), obj.class_type) == class_filter.value().end()) {
                    continue;
                }
            }
            objects.push_back(obj);
        }
        return objects;
    }
};


#endif