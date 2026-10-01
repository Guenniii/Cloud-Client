#pragma once

namespace VK {
	void Hook(HWND hwnd);
	void Unhook( );

	    bool InstallProbe( );
        void RemoveProbe( );
        bool WasCalled( );
}
