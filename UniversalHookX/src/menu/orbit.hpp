#pragma once

// Click-GUI v2 "Orbit" (spec: docs/clickgui-design-v2.md, plan: docs/clickgui-v2-plan.md).
#include "login.hpp"

namespace Orbit {
    void Draw();
    bool TakesLoginHandoff(); // true on the first frames after the login morph: the menu must render fully opaque
    bool DrawLogin(Menu::Login::View& v); // true when the user submits (button or Enter in the password field)
}
