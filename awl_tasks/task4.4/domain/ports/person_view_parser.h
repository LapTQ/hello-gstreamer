#ifndef PERSON_VIEW_PARSER_PORT_H
#define PERSON_VIEW_PARSER_PORT_H

#include "domain/entities/person_view_type.h"

#include "nvdsmeta.h"

#include <optional>

class IPersonViewParser {
public:
    virtual std::optional<View> parse(const NvDsObjectMeta* obj_meta) = 0;
    virtual ~IPersonViewParser() = default;
};

#endif

