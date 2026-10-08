#ifndef ACTION_STATE_PARSER_PORT_H
#define ACTION_STATE_PARSER_PORT_H

#include "domain/entities/action_state.h"

#include "nvdsmeta.h"

#include <optional>

class IActionStateParser {
public:
    virtual std::optional<ActionState> parse(const NvDsObjectMeta* obj_meta) = 0;
    virtual ~IActionStateParser() = default;
};

#endif

