#include <Windows.h>
#include "base.hpp"
#include "console/console.hpp"
#include "hooks/hooks.hpp"
#include "dependencies/minhook/MinHook.h"
#include "utils/sdk/java.hpp"
#include "modules/ModuleManager.hpp"
#include "input/rotation/manual_attack.hpp"
#include "utils/seedcracker/seedcracker_bridge.hpp"

static DWORD WINAPI ClientWorker(LPVOID parameter) {
    Base::m_hModule=static_cast<HMODULE>(parameter);
    Lifecycle::InitTrace(Base::hModule);
    Lifecycle::Trace("DLL started: embedded font ownership fix installed");
    Console::Alloc();
    MH_Initialize();
    p_jni=std::make_unique<JNI>();
    H::Init();
    Base::Init();
    if(p_jni) ManualAttack::Shutdown(p_jni->GetEnv());
    Base::Unload();
    Lifecycle::Trace("Unload requested; joining workers");
    Lifecycle::JoinWorkers();
    Lifecycle::Trace("Workers joined; removing hooks");
    // This waits for render-thread cleanup and all callbacks before freeing code.
    if(!H::Free()) { OutputDebugStringA("[Unload] Cleanup incomplete; DLL remains mapped.\n"); return 0; } // On a teardown error keep the DLL mapped.
    Lifecycle::Trace("Hooks drained; uninitializing MinHook");
    if(MH_Uninitialize()!=MH_OK) {
        OutputDebugStringA("[Unload] MinHook teardown failed; DLL kept mapped.\n");
        return 0;
    }
    Lifecycle::Trace("Clearing modules");
    ModuleManager::GetModules().clear();
    Lifecycle::Trace("Releasing bridge references");
    if(p_jni) SeedCracker::Shutdown(p_jni->GetEnv());
    Lifecycle::Trace("Releasing SDK / detaching worker JNI");
    p_jni.reset(); // JNI cache destructors run before this thread detaches from the VM.
    Lifecycle::Trace("Detaching console");
    Console::Free();
    Lifecycle::Trace("Final FreeLibraryAndExitThread");
    FreeLibraryAndExitThread(Base::hModule,0);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        Base::hModule=module;
        DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,ClientWorker,module,0,nullptr);
        if(thread) CloseHandle(thread); else return FALSE;
    }
    // No waits, hooks, JNI, or FreeLibrary calls under the loader lock.
    return TRUE;
}
