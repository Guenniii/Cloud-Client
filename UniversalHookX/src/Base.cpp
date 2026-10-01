#include "base.hpp"
#include "console/console.hpp"
#include "modules/ModuleManager.hpp"
#include <thread>
#include "utils/sdk/CMinecraft.h"
#include "utils/utils.hpp"
#include "hooks/hooks.hpp"
#include "dependencies/minhook/MinHook.h"
#include <chrono>


void Base::Init( ) {
    ModuleManager::Init( );
    LOG("[+] Base: Initialization complete.\n");
    ;
    Base::m_running = true;

    while (Base::m_running && !Lifecycle::requested) {
        ModuleManager::UpdateModules( );
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}



void Base::Unload( ) {
    // A request only: never terminate the window/render thread that called us.
    Lifecycle::requested=true;
    H::bShuttingDown=true;
    Base::m_running=false;
}
