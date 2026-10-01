# Menu structure

- `menu.hpp`: public entry points used by the render hooks.
- `menu.cpp`: main layout, module cards, animations and renderer lifecycle.
- `widgets.hpp/.cpp`: shared toggles, buttons, sliders, alpha and glow helpers. Animation state belongs to the widget implementation.
- `palette.hpp`: live access to the active theme; no cached color copies.
- `key_names.hpp/.cpp`: Windows virtual-key display names.
- `pages/config_page.cpp`: config UI; receives the module array and count from the main menu.
- `pages/settings_page.cpp`: menu hotkey, particles, theme selection and unload action.
- `pages/seedcracker_page.cpp`: cracking progress, candidates and reset action.
- `pages.hpp`: page rendering interfaces.

Config file I/O stays in `utils/config`; Java interaction stays in the existing bridge APIs. Page code runs on the same render thread as before. `Menu::Shutdown` remains in the main menu and is called by the graphics backend before destroying ImGui.

Keep embedded font ownership settings intact: static byte arrays must not be freed by ImGui. New `.cpp` files must be registered in both Visual Studio project files.

- `resources.hpp/.cpp`: font/style setup and embedded texture loading, access and release. The graphics hook still calls `Menu::Images`; cleanup still follows Java shutdown and precedes ImGui destruction. Embedded arrays are defined in this translation unit only. Global font symbols are retained for the customized ImGui dependency.
- `module_registry.hpp/.cpp`: stable module definitions, settings bindings and module hotkey edge detection. `GetModules()` returns a non-owning span over the same entries used by rendering and config.

- `login.hpp/.cpp`: login form, private login state, license/user accessors and login-window animation. Particles are passed as a render callback so the login page does not own the main menu.
- `../input/key_mapping.hpp/.cpp`: Windows virtual-key to ImGui-key conversion, shared by menu and module key capture. `modules/settings.hpp` now contains only module setting values.

GPU handles, JNI thread ownership, authentication defaults and request behavior are unchanged by this structural refactor.
