#include "key_names.hpp"
#include <Windows.h>

namespace Menu {
std::string GetKeyName(int vk) {
    if (vk == 0)
        return "";

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
