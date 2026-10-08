#ifndef DETECTION_UTILS_H
#define DETECTION_UTILS_H

#include "domain/entities/object.h"
#include "domain/entities/detection_class_type.h"
#include "domain/ports/detection_parser.h"

#include "gstnvdsmeta.h"
#include "nvdsmeta.h"
#include <gst/gst.h>

#include <unordered_map>
#include <optional>
#include <vector>
#include <string>

class DetectionParser : public IDetectionParser {
private:
    std::unordered_map<gint, DetectionClassType> _index_to_class_type {};

public:
    DetectionParser(std::unordered_map<gint, DetectionClassType> index_to_class_type)
        : _index_to_class_type { index_to_class_type }
    {}

    std::optional<Object> parse(NvDsFrameMeta* frame_meta, NvDsObjectMeta* obj_meta) override {
        if (this->_index_to_class_type.find(obj_meta->class_id) == this->_index_to_class_type.end()) {
            return std::nullopt;
        }
        
        unsigned int W { frame_meta->pipeline_width };
        unsigned int H { frame_meta->pipeline_height };

        float x1n = obj_meta->rect_params.left / (float)W;
        float y1n = obj_meta->rect_params.top / (float)H;
        float x2n = (obj_meta->rect_params.left + obj_meta->rect_params.width) / (float)W;
        float y2n = (obj_meta->rect_params.top + obj_meta->rect_params.height) / (float)H;

        Object obj {
            frame_meta->source_id,
            frame_meta->frame_num,
            obj_meta->object_id,
            x1n,
            y1n,
            x2n,
            y2n,
            this->_index_to_class_type.at(obj_meta->class_id)
        };

        return obj;
    }
};

#endif