#include "../base.hpp"
#include <Windows.h>
#include <thread>
#include <dxgi.h>
#pragma comment(lib, "opengl32.lib")
#include <string>
#include "utils.hpp"
#include <chrono>
#include "../hooks/backend/opengl/hook_opengl.hpp"
#include "../hooks/backend/vulkan/hook_vulkan.hpp"
#include "../console/console.hpp"

#define RB2STR(x) case x: return #x

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static RenderingBackend_t g_eRenderingBackend = NONE;

static BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
	const auto isMainWindow = [ handle ]( ) {
		return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
	};

	DWORD pID = 0;
	GetWindowThreadProcessId(handle, &pID);

	if (GetCurrentProcessId( ) != pID || !isMainWindow( ) || handle == GetConsoleWindow( ))
		return TRUE;

	*reinterpret_cast<HWND*>(lParam) = handle;

	return FALSE;
}

namespace Utils {
	void SetRenderingBackend(RenderingBackend_t eRenderingBackground) {
		g_eRenderingBackend = eRenderingBackground;
	}

	RenderingBackend_t GetRenderingBackend( ) {
		return g_eRenderingBackend;
	}

	const char* RenderingBackendToStr( ) {
		RenderingBackend_t eRenderingBackend = GetRenderingBackend( );

		switch (eRenderingBackend) {
			RB2STR(DIRECTX9);
			RB2STR(DIRECTX10);
			RB2STR(DIRECTX11);
			RB2STR(DIRECTX12);

			RB2STR(OPENGL);
			RB2STR(VULKAN);
		}

		return "NONE/UNKNOWN";
	}

	HWND GetProcessWindow( ) {
		HWND hwnd = nullptr;
		EnumWindows(::EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));

	//	while (!hwnd) {
	//		EnumWindows(::EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));
	//		LOG("[!] Waiting for window to appear.\n");
	//		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	//	}

		char name[128];
		GetWindowTextA(hwnd, name, RTL_NUMBER_OF(name));
		LOG("[+] Got window with name: '%s'\n", name);

		return hwnd;
	}

	void UnloadDLL( ) { Base::Unload(); }

	HMODULE GetCurrentImageBase( ) {
		return (HINSTANCE)(&__ImageBase);
	}

	int GetCorrectDXGIFormat(int eCurrentFormat) {
		switch (eCurrentFormat) {
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
		}

		return eCurrentFormat;
	}



	RenderingBackend_t Utils::DetectRenderingBackend( ) {
            LOG("[*] Detecting rendering backend...\n");

            bool hasVulkan = GetModuleHandleA("vulkan-1.dll") != nullptr;
            bool hasOpenGL = GetModuleHandleA("opengl32.dll") != nullptr;

            if (!hasVulkan && !hasOpenGL) {
                LOG("[-] Neither vulkan-1.dll nor opengl32.dll is loaded.\n");
                return NONE;
            }

            // Eindeutiger Fall: nur eine der beiden DLLs geladen -> keine Probe nötig
            if (hasVulkan && !hasOpenGL) {
                LOG("[+] Only vulkan-1.dll loaded -> Vulkan.\n");
                return VULKAN;
            }
            if (hasOpenGL && !hasVulkan) {
                LOG("[+] Only opengl32.dll loaded -> OpenGL.\n");
                return OPENGL;
            }

            // Beide DLLs sind geladen -> wir hooken kurz beide Present-Funktionen
            // und schauen, welche tatsächlich vom Spiel aufgerufen wird.
            LOG("[!] Both modules loaded, probing actual present calls...\n");

            bool glReady = GL::InstallProbe( );
            bool vkReady = VK::InstallProbe( );

            RenderingBackend_t eResult = NONE;

            for (int i = 0; i < 50; ++i) { // max. 5 Sekunden warten
                if (vkReady && VK::WasCalled( )) {
                    LOG("[+] Vulkan present call detected.\n");
                    eResult = VULKAN;
                    break;
                }
                if (glReady && GL::WasCalled( )) {
                    LOG("[+] OpenGL present call detected.\n");
                    eResult = OPENGL;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            if (glReady)
                GL::RemoveProbe( );
            if (vkReady)
                VK::RemoveProbe( );

            if (eResult == NONE)
                LOG("[-] No present calls observed within timeout.\n");
            else
                LOG("[+] Backend confirmed via active present calls: %s\n",
                    eResult == VULKAN ? "VULKAN" : "OPENGL");

            return eResult;
        }
}


