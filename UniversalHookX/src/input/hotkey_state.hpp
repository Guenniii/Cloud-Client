#pragma once

namespace Input {
enum class HotkeyMode { Toggle, Hold };

// Render-thread state. A key held through blocking or rebinding must be
// released before it can activate anything again.
struct HotkeyState {
    int key = -1;
    HotkeyMode mode = HotkeyMode::Toggle;
    bool previousDown = false;
    bool armed = false;
    bool holding = false;
    bool restore = false;

    bool Update(int nextKey, HotkeyMode nextMode, bool down, bool blocked, bool enabled) {
        const bool changed = key != nextKey || mode != nextMode;
        if (holding && (changed || blocked || !down)) {
            enabled = restore;
            holding = false;
        }
        if (changed) {
            key = nextKey;
            mode = nextMode;
            armed = false;
        }
        if (blocked || key == 0) armed = false;
        else if (!down) armed = true;
        if (!blocked && key != 0 && armed && down && !previousDown) {
            if (mode == HotkeyMode::Toggle) enabled = !enabled;
            else { restore = enabled; holding = true; enabled = true; }
        }
        previousDown = down;
        return enabled;
    }
};
}
