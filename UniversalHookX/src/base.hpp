#pragma once
#include "utils/lifecycle/lifecycle.hpp"

#include "utils/sdk/CMinecraft.h"
#include <Windows.h>

extern CMinecraft* g_Minecraft;

struct Base {
    static void Init( );
    static void Unload( );
    static inline HMODULE hModule;

    static inline std::atomic<bool> m_running{false};
    static inline HMODULE m_hModule = nullptr;

    static inline const char* m_version = "BETA";
};
