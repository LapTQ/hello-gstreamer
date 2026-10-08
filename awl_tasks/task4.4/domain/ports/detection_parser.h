#ifndef DETECTION_PARSER_PORT_H
#define DETECTION_PARSER_PORT_H

#include "domain/entities/object.h"

#include "nvdsmeta.h"

#include <optional>

class IDetectionParser {
public:
    virtual std::optional<Object> parse(NvDsFrameMeta* frame_meta, NvDsObjectMeta* obj_meta) = 0;
    virtual ~IDetectionParser() = default;
};

#endif

