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

The color popup is drawn by `custom_color_picker.cpp`, using the same live theme
palette and glow helpers as other menu controls. It has rounded SV gradients,
a capsule hue track, eased selection handles, fade/position entrance animation,
strict six-digit RGB hex entry (optional #), six presets and original-color
restore. RGB changes apply immediately; easing affects only visual handles.
Hue and saturation survive black/gray edits. Each setting keeps independent HSV
state and re-synchronizes when profile loading changes its packed RGB value.

The Friends page lives in pages/friends_page.cpp and opens via the header Friends icon. Account editing, per-name removal and middle-click options share the same Friends state and profile flags. Friend list writes replace friends.txt atomically.


- `pages/theme_page.cpp`: dedicated palette-icon page with live mini previews. Seven themes are appended in `utils/theme/theme.hpp`; existing profile indexes 0-2 stay stable, and configuration bounds use `Theme::Count`. Theme selections are persisted when a profile is saved.

## Three menu variants

- `menu.cpp` (`Menu::DrawPhantomMenu`): classic menu (style 0). `clickgui.hpp/.cpp` (`ClickGui::Draw`): Click-GUI per `docs/clickgui-design.md` (style 1, default, window ID `###client_menu_v2`). `orbit.hpp/.cpp` (`Orbit::Draw`): Click-GUI v2 "Orbit" per `docs/clickgui-design-v2.md` (style 2, window ID `###client_menu_orbit`).
- All three are always compiled. `Config::menu_style` (0/1/2) selects the menu; `Menu::Render` reads it once per frame, so a change made on the Settings page ("Menu style" row, shared by all menus) takes effect on the next frame.
- The style is saved in the profile as `ui.menu_style=<0..2>` (profile_version stays 2). Profiles without the key, or with an out-of-range value, load as 1 (Click GUI). It is part of the undo snapshot (`history.hpp`). There is no autoload: after every injection the Click GUI starts.
- Orbit uses its own fonts: Space Grotesk (Medium, Bold) and JetBrains Mono (Regular, Bold), embedded in `fonts/orbit_fonts.h` (included only by `resources.cpp`; several sizes per face, Latin-1 up to 16 px, ASCII above). Licenses: `fonts/OFL-SpaceGrotesk.txt`, `fonts/OFL-JetBrainsMono.txt` (SIL OFL 1.1). The fonts are exposed as `orbit_text`, `orbit_text_bold`, `orbit_mono`, `orbit_mono_bold` (`OrbitFace`, nearest size is picked at draw time).
- `module_view.hpp`: logic shared by all variants (categories, icons, search/filter, counters, favorites sorting, conflict tooltip, module reset, setting help, OreSim seed field, client pages).
