#ifndef PERSON_VIEW_TYPE_H
#define PERSON_VIEW_TYPE_H

enum class ViewType {
    FRONT,
    SIDE,
    BACK,
};

struct View {
    ViewType type { ViewType::FRONT };
    float score { 0 };
};

#endif