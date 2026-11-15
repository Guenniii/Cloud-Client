#include "key_names.hpp"
#include <Windows.h>

namespace Menu {
std::string GetKeyName(int vk) {
    if (vk == 0)
        return "";

    // Common control keys have stable English names on every Windows locale.
    if(vk>=VK_F1&&vk<=VK_F24)return "F"+std::to_string(vk-VK_F1+1);
    if(vk>=VK_NUMPAD0&&vk<=VK_NUMPAD9)return "Num "+std::to_string(vk-VK_NUMPAD0);
    switch(vk){
        case VK_LBUTTON:return "Mouse 1";case VK_RBUTTON:return "Mouse 2";case VK_MBUTTON:return "Mouse 3";
        case VK_XBUTTON1:return "Mouse 4";case VK_XBUTTON2:return "Mouse 5";
        case VK_ESCAPE:return "Esc";case VK_RETURN:return "Enter";case VK_SPACE:return "Space";
        case VK_TAB:return "Tab";case VK_BACK:return "Backspace";case VK_DELETE:return "Delete";
        case VK_INSERT:return "Insert";case VK_HOME:return "Home";case VK_END:return "End";
        case VK_PRIOR:return "Page Up";case VK_NEXT:return "Page Down";
        case VK_LEFT:return "Left";case VK_RIGHT:return "Right";case VK_UP:return "Up";case VK_DOWN:return "Down";
        case VK_SHIFT:return "Shift";case VK_LSHIFT:return "Left Shift";case VK_RSHIFT:return "Right Shift";
        case VK_CONTROL:return "Ctrl";case VK_LCONTROL:return "Left Ctrl";case VK_RCONTROL:return "Right Ctrl";
        case VK_MENU:return "Alt";case VK_LMENU:return "Left Alt";case VK_RMENU:return "Right Alt";
        case VK_CAPITAL:return "Caps Lock";case VK_NUMLOCK:return "Num Lock";case VK_SCROLL:return "Scroll Lock";
        case VK_PAUSE:return "Pause";case VK_SNAPSHOT:return "Print Screen";
        case VK_LWIN:return "Left Win";case VK_RWIN:return "Right Win";
    }

    UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    switch (vk) {
        case VK_LEFT:
        case VK_UP:
        case VK_RIGHT:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_END:
        case VK_HOME:
        case VK_INSERT:
        case VK_DELETE:
        case VK_DIVIDE:
        case VK_NUMLOCK:
            scanCode |= 0x100;
            break;
    }

    char name[32] = {0};
    LONG lParam = scanCode << 16;
    if (GetKeyNameTextA(lParam, name, sizeof(name)) > 0)
        return std::string(name);
    return "Key";
}
}
