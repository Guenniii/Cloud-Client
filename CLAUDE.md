# UniversalHookX / PhantomUI – Hinweise für Claude

## Projekt
- C++-DLL (`fusion-plus.dll`, MinHook + ImGui) wird in **Minecraft 26.2** injiziert (Stand CHECKPOINT.md, 2026-10-01).
  Hookt OpenGL (`wglSwapBuffers`) und Vulkan (`vkQueuePresentKHR`), zeigt das PhantomUI-Menü, ESP, Kampf-/Movement-Module.
- Java-Gegenstück: Bridge-JAR `SeedCrackerBridge.jar` aus `../SeedcrackerX-master` (Paket `phantomui`).
  Die DLL lädt die JAR per JNI über `phantomui.bootstrap.BridgeClassLoader` und ruft statische Bridge-Methoden.
- Geprüfter Stand (Commits, SHA256 von DLL/JAR): `UniversalHookX/CHECKPOINT.md`; Tests: `UniversalHookX/TEST-CHECKLIST.md`.

## Build (aus `UniversalHookX/`)
```powershell
.\tools\Build-Client.ps1 -Action build            # = python -X utf8 tools\client_build.py build
.\tools\Build-Client.ps1 -Action install -DryRun  # Hashes/Zugriff prüfen
.\tools\Build-Client.ps1 -Action install          # nur bei geschlossenem Minecraft
python .\tools\test_client_build.py               # Tests des Build-Skripts
```
- Ablauf: `check_protocol` (JNI-Optionslayout) → javac aller Java-Quellen → JAR (Basis: vorhandene `bin/SeedCrackerBridge.jar`)
  → MSBuild `Release|x64` mit `OutDir`/`IntDir` auf das Build-Verzeichnis → `manifest.json`.
- Artefakte landen in `obj/client-build/<zeitstempel>/` (DLL, JAR, Logs, `manifest.json`); `obj/client-build/latest.json` zeigt auf den letzten Erfolg.
- `build` installiert danach automatisch nach `bin/` (bei laufendem Minecraft nur Hinweis `NOT INSTALLED`; `-NoInstall` = nur bauen).
- `install` kopiert beide nach `bin/` (`bin/fusion-plus.dll`, `bin/SeedCrackerBridge.jar`), mit Backup, Rollback und `installed.json`.
- Direkter VS-Build Release|x64 schreibt `bin\fusion-plus.dll` (vcxproj `OutDir .\bin\`).
- Voraussetzungen: Python 3.9+, JDK 25, VS-Toolset v145, Vulkan SDK, lokaler Gradle/Loom-Cache. Nach JAR-Wechsel Minecraft neu starten.

## Ordnerstruktur (`UniversalHookX/src/`)
- `hooks/` – Render-Hooks: `backend/opengl`, `backend/vulkan`, `hooks.cpp`, `present_sync.hpp`.
- `menu/` – ImGui-Menü: `pages/` (Config, Settings, SeedCracker), Widgets, Login, Target-HUD, Diagnose-Export, Modul-Registry.
- `modules/` – Module nach Kategorie (`combat/`, `movement/`, `utility/`), `ModuleManager`, `ModuleBase`, `RuntimeModule`, `settings.hpp`.
- `input/` – Hotkeys, Freunde, Rotation/Targeting/AntiBot, Kampf-Assists, `utility_suite.hpp`, `assist_options.hpp` (JNI-Optionslayout).
- `utils/sdk/` – JNI-/Minecraft-Wrapper: `CMinecraft`, `CFabric`, `java.*`, `jni_refs`, `jni_safety.hpp`, `game_runtime.hpp`.
- `utils/config/` – `ModuleData`/`ModuleSetting`, Profile speichern/laden, Config-Share-Codes (`share_code.hpp`, `sharing.hpp`).
- `utils/seedcracker/` – Laden der Bridge-JAR (`seedcracker_bridge.cpp`, `LoadExtensionClass`), SeedCracker-Status, Shipwreck-ESP.
- Weitere: `utils/esp`, `utils/oresim`, `utils/theme`, `auth/`, `console/`, `lifecycle/`; `dependencies/` = vendored.
- Java `phantomui.*`: `bootstrap` (ClassLoader), `assist` (Utility-Suite, `AssistOptions`), `aim` (manueller Angriff),
  `combat` (Aim Assist, Auto Mace, Breach Swap, Safe Anchor), `compat` (`NativeActions`), `esp` (Player/Skin/Echo Trail),
  `hud` (Target HUD), `movement` (Auto Tool, Pearl Catch, Wind Charge, Safe Walk), `network` (Fake Lag),
  `oresim`, `player` (Friends, AntiBot), `seedcracker`, `visual` (Projectile/Explosion/Sound/Storage/Item).

## JNI: C++-Datei → Java-Klasse (`phantomui.`…)
| C++ (`src/…`) | Java-Klasse |
|---|---|
| `utils/seedcracker/seedcracker_bridge.cpp`, `utils/seedcracker/shipwreck_esp.hpp` | `seedcracker.SeedCrackerBridge`, `seedcracker.NativeClientTask` |
| `utils/sdk/CMinecraft.cpp` | `compat.NativeActions` |
| `utils/esp/player_esp.hpp` / `utils/esp/echo_trail.hpp` | `esp.PlayerEspBridge`, `esp.PlayerSkinBridge` / `esp.EchoTrailBridge` |
| `utils/oresim/oresim.hpp` | `oresim.OreSimBridge` |
| `input/utility_suite.hpp` / `input/visual_modules.hpp` | `assist.UtilitySuiteBridge` / `visual.VisualModulesBridge` |
| `input/auto_tool.hpp` / `input/friends.hpp` / `input/fake_lag.hpp` | `movement.AutoToolBridge` / `player.FriendsBridge` / `network.FakeLagBridge` |
| `input/rotation/antibot.hpp` / `input/rotation/manual_attack.hpp` | `player.AntiBotBridge` / `aim.ManualAttackBridge` |
| `input/combat/{safe_anchor,breach_swap,auto_mace,aim_assist}.hpp` | `combat.{SafeAnchor,BreachSwap,AutoMace,AimAssist}Bridge` |
| `modules/movement/{short_wind_charge,safe_walk,auto_pearl_catch}_module.cpp` | `movement.{ShortWindCharge,SafeWalk,AutoPearlCatch}Bridge` |
| `modules/movement/key_pearl_module.cpp` | `movement.AutoPearlCatchBridge.pearl(boolean)` |
| `menu/target_hud.hpp` | `hud.TargetHudBridge` |
Bridge-Klassen über `SeedCracker::LoadExtensionClass(env, "phantomui.…")` laden, nicht über `env->FindClass`
(der `BridgeClassLoader` definiert `phantomui.*`, `com.seedfinding.*` und `kaptainwutax.seedcrackerX.*` selbst).

## Konventionen
- **JNI-Safety** (`utils/sdk/jni_safety.hpp`): `JniSafety::LocalFrame` (Push/PopLocalFrame, RAII, auf `operator bool` prüfen),
  `JniSafety::Lookup` (Lookups mit `ExceptionCheck` + Clear, liefert bei Fehler `T{}`), `ScopeExit`.
  Nach jedem rohen `env->Call…` `ExceptionCheck()` prüfen; keinen neuen JNI-Lookup starten, solange eine Exception anhängt.
  Globale Refs beim Entladen freigeben (Unload-Reihenfolge Worker → Menü → SDK beachten).
- **Optionslayouts** müssen auf beiden Seiten gleich bleiben: `input/assist_options.hpp` ↔ `phantomui/assist/AssistOptions.java`
  (COUNT 30, Indizes 0..COUNT-1, nur anhängen; jede Option genau einmal in `input/utility_suite.hpp` gesetzt – prüft `check_protocol`).
  `VisualModulesBridge.frame(int[])` erwartet genau 31 Werte (wird nicht automatisch geprüft).
- **Profile**: `PhantomUI/configs/<name>.cfg` (relativ zum Arbeitsverzeichnis), Text `key=value`, erste Zeile `profile_version=2`,
  pro Modul `module.<name>.enabled|favorite|hotkey|hotkey_mode|<setting>=<int>`. Namen über `ValidName` (1–64 Zeichen).
  Menüwahl `ui.menu_style=0|1|2` (Klassisch / Click GUI / Orbit, Standard 2, kein Autoload; `menu.cpp` `kMenus[]`).
  Freunde: `bin/friends.txt` neben der DLL (max. 128).
- **Share-Codes**: Config `CC-CONFIG1.<base64url ohne Padding>.<FNV-1a 8 hex>`, Payload tab-getrennt (`client-config\t1`, `theme`, `module`).
  Theme `CC-THEME1|preset|…|CHECKSUM`. Import immer `Inspect` (nur prüfen) → `Apply` (legt Backup-Profil `before-import-…` an). Dateien `*.ccfg`.
- **Atomares Schreiben**: in `<ziel>.tmp` schreiben, dann `MoveFileExW(tmp, ziel, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`,
  bei Fehler `.tmp` löschen. Kein gemeinsamer Helper – Muster wie in `utils/config/config.cpp` oder `input/friends.hpp` übernehmen.

## Nicht anfassen
- `src/dependencies/` (ImGui, MinHook, stb, JNI-Header), `bytearray.h`, Font-Header – vendored, nicht ändern und nicht lesen.
- Binärdateien in `bin/` und `obj/client-build/` nicht von Hand ersetzen; nur über `install`. `bin/SeedCrackerBridge.jar` ist die Basis-JAR für den Build.
- `CHECKPOINT.md` beschreibt einen getesteten Stand – nur nach erneutem Test aktualisieren.
- **Bridge-Änderungen** (Methodennamen, JNI-Signaturen, Optionslayouts, Array-Größen) immer in **beiden Repos zusammen** ändern,
  gemeinsam bauen und beide Commits zusammen dokumentieren. Kein Commit/Push ohne Auftrag.

## Arbeitsweise
- Neue Features und größere Umbauten: erst `planner` (Plan), dann `implementer` (Umsetzung), dann `reviewer` (Review vor jedem Commit).
- Für Suche im Code den `code-searcher`-Agent nutzen. graphify-Graph liegt in `graphify-out/` (`GRAPH_REPORT.md`).
- Vor Abschluss: gemeinsamer Build mit `Build-Client.ps1 -Action build` und relevante Punkte aus `TEST-CHECKLIST.md` nennen.
