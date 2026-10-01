#pragma once

namespace GL {
	void Hook(HWND hwnd);
	void Unhook( );

	// Leichtgewichtiger Hook nur zur Erkennung – initialisiert KEIN ImGui.
        bool InstallProbe( );
        void RemoveProbe( );
        bool WasCalled( );
}
