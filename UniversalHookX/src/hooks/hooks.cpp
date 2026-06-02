#include <cstdio>
#include <mutex>
#include <thread>

#include "hooks.hpp"
#include "../dependencies/imgui/imgui.h"

#include "backend/opengl/hook_opengl.hpp"
#include "backend/vulkan/hook_vulkan.hpp"

#include "../console/console.hpp"
#include "../menu/menu.hpp"
#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"

#include "../modules/settings.hpp"

#include "../utils/sdk/java.hpp"


static HWND g_hWindow = NULL;
static std::mutex g_mReinitHooksGuard;

static DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam) {
    std::lock_guard<std::mutex> guard{g_mReinitHooksGuard};

    LOG("[!] Hooks will reinitialize!\n");

    HWND hNewWindow = U::GetProcessWindow( );
    while (hNewWindow == reinterpret_cast<HWND>(lpParam)) {
        hNewWindow = U::GetProcessWindow( );
    }

    H::bShuttingDown = true;

    H::Free( );
    H::Init( );

    H::bShuttingDown = false;

    return 0;
}

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_DELETE) {
            Menu_Enabled = !Menu_Enabled;

            ImGuiIO& io = ImGui::GetIO( );
            io.MouseDrawCursor = Menu_Enabled;

            if (Menu_Enabled) {
            //    ClipCursor(nullptr);
                ShowCursor(TRUE);
            } else {
                ShowCursor(FALSE);
            }
            return 0; // ← return gehört INNERHALB der if (wParam == VK_DELETE) Klammer
        } else if (wParam == VK_ESCAPE) { // ← neu
            if (Menu_Enabled) {
                Menu_Enabled = false;
                ImGuiIO& io = ImGui::GetIO( );
                io.MouseDrawCursor = false;
                ShowCursor(FALSE);
                return 0;
            }
        }
        
         else if (wParam == VK_PRIOR) {
            H::bShuttingDown = true;
            U::UnloadDLL( );
            return 0;
        }
    } 

    LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (Menu_Enabled) {
        // Rückgabewert prüfen - wenn ImGui den Input will, NICHT weiterleiten
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;

        // Zusätzlich: Maus-Events blockieren wenn ImGui sie haben will
        ImGuiIO& io = ImGui::GetIO( );
        switch (uMsg) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MOUSEWHEEL:
            case WM_MOUSEMOVE:
                if (io.WantCaptureMouse)
                    return 0;
                break;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_CHAR:
                if (io.WantCaptureKeyboard)
                    return 0;
                break;
        }
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

namespace Hooks {
    void Init( ) {
        g_hWindow = U::GetProcessWindow( );

#ifdef DISABLE_LOGGING_CONSOLE
        bool bNoConsole = GetConsoleWindow( ) == NULL;
        if (bNoConsole) {
            AllocConsole( );
        }
#endif

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
            case OPENGL:
                GL::Hook(g_hWindow);
                break;
            case VULKAN:
                VK::Hook(g_hWindow);
                break;
        }

#ifdef DISABLE_LOGGING_CONSOLE
        if (bNoConsole) {
            FreeConsole( );
        }
#endif

        // ==========================================================
        // JNI / MINECRAFT HOOK REGISTRIERUNG
        // ==========================================================
        // Hinweis: Dieser Part setzt voraus, dass p_jni bereitsteht.
        // Wir suchen die Klasse des LocalPlayer im Client.
        if (p_jni && p_jni->GetEnv( )) {
            JNIEnv* env = p_jni->GetEnv( );

            // Klasse im Snapshot finden (Beispielhaftes Standard-Mapping)
            jclass localPlayerClass = env->FindClass("net/minecraft/client/player/LocalPlayer");
            if (localPlayerClass) {
                // In modernen Java-Umgebungen nutzen JNI-Clients oft JNINativeMethod-Swapping.
                // Falls du MinHook für die internen JVM-Methoden nutzt, sieht das so aus:

                // 1. Hole die Speicheradresse der Methode (benötigt dein SDK-Spezifisches Mapping-System)
                // LPVOID p_aiStep_Address = p_jni->GetMethodAddress("net/minecraft/client/player/LocalPlayer", "aiStep", "()V");

                // 2. Erstelle den MinHook (falls anwendbar auf die JNI-Kompilierung)
                // MH_CreateHook(p_aiStep_Address, &hk_aiStep, reinterpret_cast<LPVOID*>(&o_aiStep));
                // MH_EnableHook(p_aiStep_Address);

                env->DeleteLocalRef(localPlayerClass);
            }
        }


        oWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Free( ) {
        if (oWndProc) {
            SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        }

        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Menu_Enabled = false;
        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend( );
        switch (eRenderingBackend) {
                break;
            case OPENGL:
                GL::Unhook( );
                break;
            case VULKAN:
                VK::Unhook( );
                break;
        }
    }
} // namespace Hooks
