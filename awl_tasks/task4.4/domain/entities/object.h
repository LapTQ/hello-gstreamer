#ifndef OBJECT_H
#define OBJECT_H

#include "domain/entities/detection_class_type.h"
#include "domain/entities/person_view_type.h"
#include "domain/entities/action_state_type.h"

#include <gst/gst.h>

#include <optional>

struct Object {
    guint camera_id {};
    gint frame_num {};
    guint64 track_id {};
    float x1n {};
    float y1n {};
    float x2n {};
    float y2n {};
    DetectionClassType class_type { DetectionClassType::PERSON };
    std::optional<View> view { std::nullopt };
    std::optional<ActionState> action_state { std::nullopt };
};

#endif