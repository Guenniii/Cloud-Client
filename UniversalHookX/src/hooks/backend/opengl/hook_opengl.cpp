#include "../../../backend.hpp"
#include "../../../console/console.hpp"
#include <Windows.h>
#include "hook_opengl.hpp"
#ifdef ENABLE_BACKEND_OPENGL
#include <GL/gl.h>
#include <atomic>
#include <mutex>
#include "../../../dependencies/imgui/imgui_impl_opengl3.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"
#include "../../hooks.hpp"
#include "../../../menu/menu.hpp"
#include "../../../menu/resources.hpp"
#pragma comment(lib, "opengl32.lib")

namespace {
using Swap = BOOL (WINAPI*)(HDC);
Swap originalGdi = nullptr, originalWgl = nullptr;
void* targets[2]{};
using SwapLayer = BOOL (WINAPI*)(HDC,UINT);
Swap originalDriverSwap=nullptr;
SwapLayer originalDriverLayerSwap=nullptr;
void* driverTargets[2]{};
bool driverInstalled=false,driverAttempted=false;
thread_local bool insideDriverSwap=false;
std::atomic<bool> probeCalled{false};
bool probing = false;
HWND window = nullptr;
HGLRC context = nullptr; // Context owned by Minecraft; never delete it.
HGLRC menuContext = nullptr;
HDC menuDC = nullptr;
struct CurrentContext {
    HDC previousDC = wglGetCurrentDC();
    HGLRC previous = wglGetCurrentContext();
    bool active;
    CurrentContext(HDC dc, HGLRC rc) : active(wglMakeCurrent(dc, rc) != FALSE) {}
    ~CurrentContext(){ if(active) wglMakeCurrent(previousDC, previous); }
};
HGLRC CreateMenuContext(HDC dc) {
    using Create = HGLRC (WINAPI*)(HDC,HGLRC,const int*);
    auto create = reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if(create && reinterpret_cast<intptr_t>(create)>3 && reinterpret_cast<intptr_t>(create)!=-1) {
        const int attributes[]={0x2091,3,0x2092,2,0x9126,1,0}; // OpenGL 3.2 core
        if(auto rc=create(dc,nullptr,attributes)) return rc;
    }
    return wglCreateContext(dc);
}
// Context-local objects must be destroyed in the context which created them.
bool ReleaseRenderer() {
    if(!menuContext) return true;
    {
        CurrentContext current(menuDC,menuContext);
        if(!current.active) return false;
        Menu::Resources::ReleaseTextures(false);
        if(ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplOpenGL3_Shutdown();
    }
    if(!wglDeleteContext(menuContext)) return false;
    menuContext=nullptr;menuDC=nullptr;context=nullptr;
    return true;
}
thread_local bool insideSwap = false;
using BindFramebuffer = void (APIENTRY*)(GLenum, GLuint);
BindFramebuffer bindFramebuffer = nullptr;
using BindBuffer = void (APIENTRY*)(GLenum, GLuint);
BindBuffer bindBuffer = nullptr;
// The bundled ImGui font uploader does not preserve pixel-unpack state.
struct PixelUnpack {
    GLint pbo=0, alignment=0, row=0, rows=0, columns=0;
    PixelUnpack() {
        glGetIntegerv(0x88EF, &pbo);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &rows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &columns);
        bindBuffer(0x88EC, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    }
    ~PixelUnpack() {
        bindBuffer(0x88EC, pbo);
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, rows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, columns);
    }
};

// ImGui restores its pipeline state, but not framebuffer, color mask or sRGB.
// Draw into the window back buffer even when Minecraft leaves an FBO bound.
struct WindowTarget {
    GLint framebuffer = 0, drawBuffer = 0;
    GLboolean colorMask[4]{}, srgb = GL_FALSE;
    WindowTarget() {
        glGetIntegerv(0x8CA6, &framebuffer); // DRAW_FRAMEBUFFER_BINDING
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        srgb = glIsEnabled(0x8DB9);
        bindFramebuffer(0x8CA9, 0); // DRAW_FRAMEBUFFER; leave READ_FRAMEBUFFER alone
        glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
        glDrawBuffer(GL_BACK);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(0x8DB9);
    }
    ~WindowTarget() {
        glDrawBuffer(drawBuffer);
        bindFramebuffer(0x8CA9, framebuffer);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        if (srgb) glEnable(0x8DB9); else glDisable(0x8DB9);
    }
};

void Render(HDC dc) {
    if (WindowFromDC(dc) != window || !wglGetCurrentContext()) return;
    std::lock_guard<std::recursive_mutex> lock(Lifecycle::renderMutex);
    if (H::bShuttingDown) {
        if (Lifecycle::workersStopped && !Lifecycle::renderStopped) {
            // Release our GL objects first, then stop JNI features in the game's context.
            if(!ReleaseRenderer()) return;
            Menu::Shutdown(false);
            GL::Unhook();
            Lifecycle::renderStopped = true;
        }
        return;
    }
    if (!ImGui::GetCurrentContext()) return;
    HGLRC gameContext=wglGetCurrentContext();
    if(context && context!=gameContext) {
        if(!ReleaseRenderer()) return;
    }
    if(!menuContext) {
        menuContext=CreateMenuContext(dc);
        if(!menuContext) { LOG("[OpenGL] Could not create menu context: %lu\n",GetLastError());return; }
        menuDC=dc;
    }
    {
        CurrentContext current(dc,menuContext);
        if(!current.active) return;
        if (!ImGui::GetIO().BackendRendererUserData) {
            bindFramebuffer = reinterpret_cast<BindFramebuffer>(wglGetProcAddress("glBindFramebuffer"));
            bindBuffer = reinterpret_cast<BindBuffer>(wglGetProcAddress("glBindBuffer"));
            if (!bindFramebuffer || !bindBuffer || !ImGui_ImplOpenGL3_Init("#version 150")) return;
            context=gameContext;
            LOG("[OpenGL] Own menu context ready: %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
            Menu::Images();
        }
        PixelUnpack unpack;
        ImGui_ImplOpenGL3_NewFrame();
    }
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    // JNI feature calls remain in Minecraft's context on the original render thread.
    Menu::Render();
    ImGui::Render();
    {
        CurrentContext current(dc,menuContext);
        if(!current.active) return;
        WindowTarget target;
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glFlush(); // Submit this context's draw commands before restoring the game context.
    }
    // The original swap/capture hook always sees Minecraft's original context.
}
void ReportCaptureOrder() {
    static bool observed=false;static ULONGLONG next=0;
    if(observed||GetTickCount64()<next)return;next=GetTickCount64()+5000;
    HMODULE obs=GetModuleHandleW(L"graphics-hook64.dll");if(!obs)return;
    void* frames[24]{};USHORT count=CaptureStackBackTrace(0,24,frames,nullptr);
    for(USHORT i=0;i<count;++i){
        HMODULE owner=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(frames[i]),&owner);
        if(owner==obs){observed=true;LOG("[OpenGL] OBS hook precedes client rendering; use capture-overlays OFF.\n");return;}
    }
}
BOOL WINAPI DriverSwap(HDC dc) {
    Lifecycle::Callback callback;
    if(insideDriverSwap)return originalDriverSwap(dc);
    struct Guard{Guard(){insideDriverSwap=true;}~Guard(){insideDriverSwap=false;}} guard;
    if(WindowFromDC(dc)==window){ReportCaptureOrder();Render(dc);}
    return originalDriverSwap(dc);
}
BOOL WINAPI DriverLayerSwap(HDC dc,UINT planes) {
    Lifecycle::Callback callback;
    if(insideDriverSwap || !(planes&WGL_SWAP_MAIN_PLANE))return originalDriverLayerSwap(dc,planes);
    struct Guard{Guard(){insideDriverSwap=true;}~Guard(){insideDriverSwap=false;}} guard;
    if(WindowFromDC(dc)==window){ReportCaptureOrder();Render(dc);}
    return originalDriverLayerSwap(dc,planes);
}
bool InstallDriverHooks(HDC dc) {
    std::lock_guard<std::recursive_mutex> lock(Lifecycle::renderMutex);
    if(driverInstalled)return true;
    if(driverAttempted)return false;
    if(WindowFromDC(dc)!=window || !wglGetCurrentContext())return false;
    driverAttempted=true;
    // Resolve from the active context, never guess a driver DLL or scan its code.
    auto proc=wglGetProcAddress("glBindFramebuffer");HMODULE driver=nullptr;
    if(!proc || reinterpret_cast<intptr_t>(proc)<=3 || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(proc),&driver))return false;
    auto swap=reinterpret_cast<void*>(GetProcAddress(driver,"DrvSwapBuffers"));
    auto layer=reinterpret_cast<void*>(GetProcAddress(driver,"DrvSwapLayerBuffers"));
    if(!swap){LOG("[-] OpenGL: driver does not expose DrvSwapBuffers; capture-ordered rendering unavailable.\n");return false;}
    struct Entry{void* address;void* hook;void** original;};
    Entry entries[]={{swap,reinterpret_cast<void*>(&DriverSwap),reinterpret_cast<void**>(&originalDriverSwap)},
                     {layer,reinterpret_cast<void*>(&DriverLayerSwap),reinterpret_cast<void**>(&originalDriverLayerSwap)}};
    for(int i=0;i<2;++i){
        if(!entries[i].address || (i && entries[i].address==entries[0].address))continue;
        auto status=MH_CreateHook(entries[i].address,entries[i].hook,entries[i].original);
        if(status!=MH_OK){
            LOG("[-] OpenGL: driver hook creation failed: %s\n",MH_StatusToString(status));
            for(auto& target:driverTargets)if(target){MH_RemoveHook(target);target=nullptr;}
            return false;
        }
        driverTargets[i]=entries[i].address;
    }
    bool queued=true;for(auto address:driverTargets)if(address)queued=MH_QueueEnableHook(address)==MH_OK&&queued;
    auto status=queued?MH_ApplyQueued():MH_UNKNOWN;
    if(status!=MH_OK){
        for(auto address:driverTargets)if(address){MH_QueueDisableHook(address);MH_DisableHook(address);}
        LOG("[-] OpenGL: driver hook activation failed: %s\n",MH_StatusToString(status));return false;
    }
    driverInstalled=true;
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(driver,path,MAX_PATH);
    LOG("[OpenGL] Native rendering installed at DrvSwapBuffers: %ls\n",path);
    return true;
}
BOOL Present(HDC dc, Swap original) {
    Lifecycle::Callback callback;
    if (insideSwap) return original(dc);
    struct Guard { Guard(){insideSwap=true;} ~Guard(){insideSwap=false;} } guard;
    if (probing) {
        if (wglGetCurrentContext()) probeCalled.store(true, std::memory_order_relaxed);
    } else if(!H::bShuttingDown) {
        // Only discover/install here. OBS must see the clean game image at this level.
        InstallDriverHooks(dc);
    } else if(!driverInstalled && WindowFromDC(dc)==window && Lifecycle::workersStopped) {
        std::lock_guard<std::recursive_mutex> lock(Lifecycle::renderMutex);
        GL::Unhook();Lifecycle::renderStopped=true;
    }
    return original(dc);
}
BOOL WINAPI GdiSwap(HDC dc) { return Present(dc, originalGdi); }
BOOL WINAPI WglSwap(HDC dc) { return Present(dc, originalWgl); }

bool Install(bool probe) {
    probing = probe;
    probeCalled = false;
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll"), gl = GetModuleHandleW(L"opengl32.dll");
    void* addresses[] = {gdi ? reinterpret_cast<void*>(GetProcAddress(gdi, "SwapBuffers")) : nullptr,
                        gl ? reinterpret_cast<void*>(GetProcAddress(gl, "wglSwapBuffers")) : nullptr};
    void* callbacks[] = {reinterpret_cast<void*>(&GdiSwap), reinterpret_cast<void*>(&WglSwap)};
    Swap* originals[] = {&originalGdi, &originalWgl};
    bool success = false;
    for (int i=0; i<2; ++i) {
        if (!addresses[i] || (i && addresses[i]==addresses[0])) continue;
        auto status = MH_CreateHook(addresses[i], callbacks[i], reinterpret_cast<void**>(originals[i]));
        if (status != MH_OK) { LOG("[OpenGL] CreateHook failed: %s\n", MH_StatusToString(status)); continue; }
        status = MH_EnableHook(addresses[i]);
        if (status != MH_OK) { MH_RemoveHook(addresses[i]); LOG("[OpenGL] EnableHook failed: %s\n", MH_StatusToString(status)); continue; }
        targets[i] = addresses[i]; success = true;
    }
    return success;
}
}
namespace GL {
void Hook(HWND hwnd) {
    window = hwnd;
    Menu::InitializeContext(hwnd);
    if (Install(false)) LOG("[OpenGL] Presentation hooks enabled.\n");
    else {
        LOG("[OpenGL] No presentation hook could be installed.\n");
        Unhook();
        Lifecycle::renderStopped = true;
    }
}
void Unhook() {
    if(!ReleaseRenderer()) return;
    if (ImGui::GetCurrentContext()) {
        if (ImGui::GetIO().BackendRendererUserData) ImGui_ImplOpenGL3_Shutdown();
        if (ImGui::GetIO().BackendPlatformUserData) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    context = nullptr;
}
bool InstallProbe() { return Install(true); }
void RemoveProbe() {
    for (auto target : targets) if (target) MH_DisableHook(target);
    // Do not release trampolines while a presentation callback is returning.
    while (Lifecycle::callbacks.load() != 0) Sleep(1);
    for (auto& target : targets) if (target) { MH_RemoveHook(target); target = nullptr; }
}
bool WasCalled() { return probeCalled.load(std::memory_order_relaxed); }
}
#else
namespace GL {
void Hook(HWND) { LOG("[!] OpenGL backend is not enabled!\n"); }
void Unhook() {}
bool InstallProbe() { return false; }
void RemoveProbe() {}
bool WasCalled() { return false; }
}
#endif
