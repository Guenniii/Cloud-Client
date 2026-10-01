#pragma once
#include "../../menu/widgets.hpp"
#include "../theme/theme.hpp"
#include <cmath>

namespace EspGlow {
inline void Line(ImDrawList* draw,ImVec2 a,ImVec2 b,float width,float strength=1.0f) {
    float previous=0;
    for(int layer=6;layer>=1;--layer) {
        float t=layer/6.0f;
        float opacity=.28f*strength*std::exp(-4.5f*t*t);
        float alpha=(opacity-previous)/(1-previous); previous=opacity;
        draw->AddLine(a,b,Menu::Widgets::ColA(Theme::Current().accent,alpha),width+t*14);
    }
}
}
