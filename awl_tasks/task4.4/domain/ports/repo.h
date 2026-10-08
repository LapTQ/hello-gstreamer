#ifndef REPO_H
#define REPO_H

#include "domain/entities/object.h"

#include <gst/gst.h>

#include <vector>

class IObjectRepo {
public:
    virtual void add(Object obj) = 0;
    virtual std::vector<Object> get_objects_by_frame(
        guint camera_id, 
        gint frame_num, 
        std::optional<std::vector<DetectionClassType>> class_filter = std::nullopt
    ) = 0;

    virtual ~IObjectRepo() = default;
};


#endif