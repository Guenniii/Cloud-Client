# Projektuebersicht

## Einstieg und Lebenszyklus

`dllmain.cpp` startet den Client-Worker. Dieser richtet Konsole, MinHook, JNI und Grafik-Hooks ein und ruft `Base::Init()` auf. `Base.cpp` steuert die Update-Schleife. `Base::Unload()` fordert das Entladen an; der Worker uebernimmt die Bereinigung.

Beim Unload werden zuerst die verwalteten Worker beendet. Die Grafik-Hooks bereinigen das Menue im Render-Thread, dann werden Hooks deaktiviert und auslaufende Callbacks abgewartet. Anschliessend folgen Module, Bridge-Referenzen, SDK/JNI, Konsole und zuletzt die DLL. Die Diagnoseschritte stehen in `bin/unload-diagnostic.log` (bezogen auf den Projektordner).

## Bereiche

| Ordner | Aufgabe |
| --- | --- |
| `menu` | Layout, Seiten, Widgets, Ressourcen, Login und UI-Modulregister; Details in [menu/README.md](menu/README.md) |
| `input` | Windows-Tasten in ImGui-Tasten umwandeln |
| `modules` | Laufzeitmodule, gemeinsame Einstellungen und Modulmanager |
| `modules/combat` | CW-Koordination und Combat-Module; Details in [README.md](modules/combat/README.md) |
| `hooks` | Fenster-Hook und Grafik-Hooks fuer OpenGL/Vulkan |
| `utils/sdk` | JNI-Zugriff und Minecraft/Fabric-Wrapper; Details in [README.md](utils/sdk/README.md) |
| `utils/seedcracker` | SeedCracker-Bridge und Clientthread-Auftraege |
| `utils/oresim`, `utils/esp` | OreSim- und PlayerESP-Anbindung/Rendering |
| `utils/config`, `utils/theme` | Konfigurationsdateien und Farbpaletten |
| `utils/lifecycle` | Unload-Anforderung, Worker und Callback-Verfolgung |
| `auth`, `console` | Anmeldung und Debug-Konsole |
| `dependencies` | Eingebundene Bibliotheken und Assets; ImGui enthaelt projektspezifische Anpassungen |

## Zwei Modulregister

`modules/ModuleManager.cpp` registriert die Laufzeitmodule. Neun unabhaengige Laufzeitmodule uebernehmen FastPlace, CW, Reach, AutoTotem, NoJumpDelay, AutoSprint, HitCrystal, SilentAim und SeedCracker. `CW` steuert nur noch seinen Crystal-Worker. Das getrennte Register in `menu/module_registry.cpp` beschreibt die sichtbaren Karten, Hotkeys und Einstellungen und wird auch fuer Configs verwendet. Ein neuer Menueeintrag allein registriert keine Update-Logik.

## Build

Die Visual-Studio-Dateien liegen eine Ebene ueber `src`. Der gepruefte Build ist **Release / x64**, mit MSVC-Toolset **v145** und `stdcpplatest`. Der Vulkan-Build benoetigt das Vulkan SDK; lokal wurde `VULKAN_SDK=C:\VulkanSDK\1.4.341.1` verwendet.

Aus einer passenden Visual-Studio-Developer-Konsole im Projektordner:

```bat
msbuild UniversalHookX.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Ausgabe: `bin/fusion-plus.dll`. Neue Quelldateien immer in `UniversalHookX.vcxproj` und `UniversalHookX.vcxproj.filters` eintragen. Die Java-Bridge ist ein separates Build-Artefakt; ein nativer Build baut deren JAR nicht neu.

## Aenderungsregeln

- Header enthalten Schnittstellen und Member; umfangreiche Methoden kommen in passende `.cpp`-Dateien.
- Dateinamen und Include-Schreibweise muessen uebereinstimmen: `ModuleBase.hpp`, `ModuleManager.cpp/.hpp`, `SilentAim.cpp/.hpp`.
- `JNIEnv*` ist threadgebunden. JNI-Referenzen erst freigeben, nachdem ihre Nutzer beendet sind.
- Eingebettete Font-Arrays gehoeren der DLL. Die vorhandenen ImGui-Einstellungen zur Speicherzuordnung beibehalten.
- GPU-Ressourcen im vorgesehenen Render-Kontext freigeben; Unload nur anfordern, nicht aus einem Hook heraus die DLL freigeben.

## Kurzer Laufzeittest nach Umbauten

Initialisierung und Menue pruefen, Menue-/Modul-Hotkeys testen, eine Config speichern/laden, betroffene Module aktivieren und anschliessend entladen. Minecraft soll geoeffnet bleiben, die Debug-Konsole soll verschwinden. Ein erfolgreicher Build ersetzt diesen Spieltest nicht.

## Weitere organisatorische Arbeit

Modul-Karten und OreSim-Seedeingabe koennen noch aus `menu.cpp` ausgegliedert werden. Eine spaetere Neuordnung von `utils` sollte SDK, Grafik und Feature-Bridges einzeln verschieben, jeweils mit aktualisierten Includes und anschliessendem Build. Die JNI- und Worker-Lebenszyklen dabei separat von Verhaltensaenderungen behandeln.
