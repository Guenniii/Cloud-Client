#pragma once
#include <Windows.h>
#include "../dependencies/imgui/imgui.h"

namespace Input {
    ImGuiKey VkToImGuiKey(WPARAM vk);
}
