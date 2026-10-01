#pragma once
#include <Windows.h>
#include "../utils/lifecycle/lifecycle.hpp"

namespace Hooks {
	void Init( );
	bool Free( );

	inline std::atomic<bool> bShuttingDown{false};
}

namespace H = Hooks;
