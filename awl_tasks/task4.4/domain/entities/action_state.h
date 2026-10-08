#ifndef ACTION_STATE_H
#define ACTION_STATE_H

enum class ActionStateType {
    HAND_INTO_PANT_POCKET,
    HAND_INTO_SHIRT_POCKET,
    HAND_INTO_BAG,
    HAND_INTO_BASKET,
    HAND_INTO_SHELF,
    HOLDING_PRODUCT,
    NOT_HOLDING_PRODUCT,
};

struct ActionState {
    ActionStateType type { ActionStateType::NOT_HOLDING_PRODUCT };
    float score { 0 };
};

#endif