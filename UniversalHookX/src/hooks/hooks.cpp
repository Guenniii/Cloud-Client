#include <cstdio>
#include <mutex>
#include <thread>
#include "../base.hpp"
#include "hooks.hpp"
#include "../dependencies/imgui/imgui.h"

#include "backend/opengl/hook_opengl.hpp"
#include "backend/vulkan/hook_vulkan.hpp"

#include "../console/console.hpp"
#include "../menu/menu.hpp"
#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"

#include "../modules/settings.hpp"
#include "../input/combat/auto_mace.hpp"
#include "../input/combat/breach_swap.hpp"
#include "../input/combat/safe_anchor.hpp"
#include "../input/utility_suite.hpp"

#include "../utils/sdk/java.hpp"
#include "../utils/config/config.hpp" 

static HWND g_hWindow = NULL;
static std::mutex g_mReinitHooksGuard;

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Lifecycle::Callback callback;
    if(H::bShuttingDown) return CallWindowProc(oWndProc,hWnd,uMsg,wParam,lParam);
    if(uMsg==WM_KEYDOWN && wParam==VK_END && !Config::capturing_menu_key && Config::capturing_module_key<0 && !(lParam & (1LL<<30))) { Base::Unload(); return 0; }
    if (uMsg == WM_KEYDOWN) {
        if (!Config::capturing_menu_key && Config::capturing_module_key < 0 && !(lParam & (1LL<<30)) && (int)wParam == Config::menu_keybind) {
            Menu_Enabled = !Menu_Enabled;

            
            return 0; // ← return gehört INNERHALB der if (wParam == VK_DELETE) Klammer
        } else if (wParam == VK_ESCAPE) { // ← neu
            if (Menu_Enabled && !Config::capturing_menu_key && Config::capturing_module_key < 0) {
                Menu_Enabled = false;
            }
        }
        

    } 

    LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (Menu_Enabled && ImGui::GetCurrentContext()) {
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

    static bool anchorPressConsumed=false;
    if (uMsg==WM_KILLFOCUS) anchorPressConsumed=false;
    if (uMsg==WM_RBUTTONUP && anchorPressConsumed) { anchorPressConsumed=false; return 0; }
    if ((uMsg==WM_RBUTTONDOWN || uMsg==WM_RBUTTONDBLCLK) && SafeAnchor_Enabled && !Menu_Enabled
        && !Config::capturing_menu_key && Config::capturing_module_key<0 && GetForegroundWindow()==hWnd
        && SafeAnchor::TryClick()) { anchorPressConsumed=true; return 0; }
    static bool macePressConsumed=false;
    if (uMsg==WM_KILLFOCUS) macePressConsumed=false;
    if (uMsg==WM_LBUTTONUP && macePressConsumed) { macePressConsumed=false; return 0; }
    if ((uMsg==WM_LBUTTONDOWN || uMsg==WM_LBUTTONDBLCLK) && (AutoMace_Enabled || BreachSwap_Enabled || ShieldBreaker_Enabled) && !Menu_Enabled
        && !Config::capturing_menu_key && Config::capturing_module_key<0 && GetForegroundWindow()==hWnd
        && (UtilitySuite::TryShieldClick() || AutoMace::TryClick() || BreachSwap::TryClick())) { macePressConsumed=true; return 0; }
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

        // Dynamische Auswahl der Grafik-API beim Initialisieren
        RenderingBackend_t eRenderingBackend = U::DetectRenderingBackend( );
        U::SetRenderingBackend(eRenderingBackend);
        LOG("[+] Selected Rendering Backend: %s\n", U::RenderingBackendToStr( ));

        switch (eRenderingBackend) {
            case OPENGL:
                GL::Hook(g_hWindow);
                break;
            case VULKAN:
                VK::Hook(g_hWindow);
                break;
            default:
                LOG("[!] Error: No valid or supported rendering backend selected!\n");
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

    bool Free( ) {
        bShuttingDown=true;
        // Restore the window procedure before destroying ImGui.
        if(oWndProc && IsWindow(g_hWindow)) {
            if(reinterpret_cast<WNDPROC>(GetWindowLongPtr(g_hWindow,GWLP_WNDPROC))!=WndProc) {
                LOG("[Unload] Another window subclass is active; DLL remains mapped.\n"); return false;
            }
            SetLastError(0);
            if(!SetWindowLongPtr(g_hWindow,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(oWndProc)) && GetLastError()!=0) return false;
        }
        // Clean up GL/Vulkan on the next presentation, where the proper context exists.
        if(U::GetRenderingBackend()==NONE) Lifecycle::renderStopped=true;
        Lifecycle::Trace("Waiting for render-thread cleanup");
        while(!Lifecycle::renderStopped) Sleep(10);
        Lifecycle::Trace("Render-thread cleanup finished; disabling hooks");
        if(MH_DisableHook(MH_ALL_HOOKS)!=MH_OK) {
            LOG("[Unload] Disabling hooks failed; DLL remains mapped.\n");return false;
        }
        while(Lifecycle::callbacks.load()!=0) Sleep(1);
        Lifecycle::Trace("Checking callback epilogues");
        bool ready=Lifecycle::WaitOutsideImage(Base::hModule);
        Lifecycle::Trace(ready?"Callbacks outside DLL":"Cannot verify callback exit; DLL remains mapped");
        return ready;
    }
} // namespace Hooks
