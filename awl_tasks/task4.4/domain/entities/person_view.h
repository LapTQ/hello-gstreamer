#ifndef PERSON_VIEW_H
#define PERSON_VIEW_H

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