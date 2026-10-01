#pragma once
#include "../utils/theme/theme.hpp"

// Resolve the active palette on each call so theme changes apply immediately.
namespace Menu::Palette {
    inline ImU32 Background() { return Theme::Current().background; }
    inline ImU32 Accent() { return Theme::Current().accent; }
    inline ImU32 Soft() { return Theme::Current().soft; }
    inline ImU32 Text() { return Theme::Current().text; }
    inline ImU32 Muted() { return Theme::Current().muted; }
    inline ImU32 Card() { return Theme::Current().card; }
    inline ImU32 Border() { return Theme::Current().border; }
    inline ImU32 Success() { return Theme::Current().success; }
    inline ImU32 Toggle() { return Theme::Current().toggle; }
    inline ImU32 Label() { return Theme::Current().label; }
}
