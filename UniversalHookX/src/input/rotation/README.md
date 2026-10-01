# Shared rotation helpers

- `targeting.hpp`: finite point-to-angle calculation and nearest-point comparison. Entity discovery remains in the calling module.
- `controller.hpp`: existing smoothing profiles and rotation state, without JNI dependencies.
- `ownership.hpp`: one owner per module update. ModuleManager dispatches HitCrystal before Standalone; the first accepted owner retains the update even after reset. This is ordered dispatch, not a queued numeric priority system.
- `utils/SilentAim.cpp`: serializes calculation and JNI send, restores camera angles after send failures, and scopes temporary Java references.

Call `beginUpdate()` once before module dispatch, `apply()` with a named owner, and `reset(owner)` on release, cancellation or disable. Requests execute immediately on the existing SDK worker; this refactor does not change thread dispatch. The caller remains responsible for a valid player/world and action preconditions.

Standalone SilentAim selects visible living non-spectator players while Right Shift is held. Enable its Combat card and set range (eye-to-eye distance, default 4 blocks) and full-cone FOV (default 60 degrees). The selected target is retained until invalid; otherwise the smallest view angle wins. Range/FOV and the module hotkey are saved by the existing profile registry. Game screens, the client menu, focus loss and key release cancel the action. HitCrystal retains its separate crystal target selection.

`player_selection.hpp` contains pure scoring/retention; `player_targeting.hpp` reads a bounded player-list snapshot through JNI. No entity references survive a scan. No automatic attacks or team/friend filtering are implemented. Existing `priority` and `fixMovement` request fields are reserved and have no effect.

Host tests in the delivered rotation package cover angle math, reset, ownership, local frames, missing JNI lookups and camera restoration after Java exceptions. Gameplay still requires testing in Minecraft.
