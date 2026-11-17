# Plan: Info HUD als eigenes Modul (Orbit-Stil)

Stand 2026-11-17. Pfade relativ zu `UniversalHookX/`. Zeilenangaben am 2026-11-17 im Code nachgesehen. Nur Plan, noch kein Code.

## 1. Was wirklich gebraucht wird

**Muss:**
- Neue Modulkarte „Info HUD“ (Flag `InfoHUD_Enabled`). Über die Karte bekommt das HUD Hotkey, Toast, Undo, Profil und Share-Code automatisch.
- Die Liste wird aus `Registry::GetModules()` gebaut. Das Info HUD selbst wird per Zeigervergleich ausgeschlossen: `m.enabled == &InfoHUD_Enabled`.
- Eigene Optik im Orbit-Stil, Position per Ziehen bei offenem Menü.
- Das alte Status-HUD wird **ersetzt**, nicht daneben weitergeführt:
  - `src/menu/status_hud.hpp` wird neu geschrieben. Datei und Namespace bleiben, damit sich `menu.cpp:8` und `:774` nicht ändern.
  - Die feste Liste `:15-24` entfällt. Damit fällt auch die Pflichtstelle „Status-HUD“ aus `docs/ideas.md:8` weg, und ideas-2 Punkt 3.4 ist erledigt.

**Weggelassen (nice-to-have):**
- Koordinaten, Blickrichtung, Ping, Server und TPS. Dafür gibt es heute keinen Getter; in `utils/sdk` gibt es kein `getX`, kein Yaw und kein Ping.
- Ausblenden bei F1 oder offenem Screen (dafür gibt es im ganzen Projekt keine Erkennung).
- Glow-Option, Farben pro Kategorie, eigene Sortieroptionen, rechtsbündiger Anker, Reset-Button.

Gründe:
- Die Position wird bei jedem Frame auf den Bildschirm geklemmt (wie heute `status_hud.hpp:31-34`). Das HUD kann also nicht verloren gehen, ein Reset ist unnötig.
- F1 und Screen-Erkennung brauchen beide JNI bzw. die Bridge.

**Migration:**

| Altes Element | Neu |
|---|---|
| `Config::hud_enabled` (`config.hpp:52`) | entfällt, ersetzt durch `InfoHUD_Enabled` |
| Profil-Schlüssel `hud.enabled` (`config.cpp:70`) | wird nicht mehr geschrieben. Beim Laden wird er weiter gelesen und auf `InfoHUD_Enabled` abgebildet (`config.cpp:203-206`), genauso wie der Altschlüssel `silent_aim` (`:217`). |
| `hud.x` / `hud.y` (`config.cpp:71-72`, `:207-211`), `Config::hud_x/y` | bleiben unverändert (Pixel, gleiche Bedeutung). Keine Migration nötig. |
| Settings-Zeile „Info HUD“ (`settings_page.cpp:115-124`, Reset und Toggle) | entfällt. Die Watermark-Zeile rückt nach oben (`:125`: `watermarkY = unloadY + 70`). Ein zweiter Schalter für dasselbe Flag würde nur verwirren. |

Warum die Migration korrekt ist:
- `config_page.cpp:63-64` setzt vor `Config::Load` alle Module zurück. Dadurch ist `InfoHUD_Enabled` beim Laden zunächst false.
- `hud.enabled` steht in der Datei vor den `module.*`-Zeilen. Ein späteres `module.Info HUD.enabled` überschreibt also den Altwert.
- Ergebnis: Alte Profile mit `hud.enabled=1` zeigen das HUD weiterhin, neue Profile haben Vorrang.

## 2. Inhalt

**Modulliste:**
- Schleife über `GetModules()` mit `*m.enabled` und ohne das Info HUD selbst.
- Sortiert nach Textbreite absteigend, bei Gleichstand nach Name. Das ergibt die übliche Treppenform einer „Arraylist“.
- Ohne Kategoriefarben: Die Palette kennt keine (`palette.hpp`), nur Accent.
- Pro Zeile ein Icon über `ModuleView::ModuleIcon(m)` (`module_view.hpp:73`). Das gibt es schon, und es fällt pro Kategorie auf ein Standard-Icon zurück.
- Hinweis: `Friends_Enabled` ist per Default `true` (`settings.hpp:138`). „Friends“ steht deshalb fast immer in der Liste. Das entspricht der Anforderung „alle anderen aktiven Module“. Wenn das stört, später gezielt ausnehmen; im Plan ist das nicht vorgesehen.

**Infozeilen (nur was nichts kostet):**

| Info | Quelle | Kosten |
|---|---|---|
| FPS | `ImGui::GetIO().Framerate` | frei (Render-Hook = Spiel-FPS) |
| Uhrzeit | `GetLocalTime` (Windows) | frei |
| Renderer (OpenGL/Vulkan) | `U::RenderingBackendToStr()` (heute `status_hud.hpp:52`) | frei. Im HUD weglassen, es steht im Diagnose-Report. |
| Koordinaten, Richtung | neuer JNI-Getter auf dem Render-Thread oder neue Bridge-Methode | nicht in v1 |
| Ping, Server | Bridge-Methode nötig (vgl. ideas-2 Punkt 3.5 `serverAddress()`) | nicht in v1 |
| TPS | Schätzung aus Java-Paketen, Bridge | nicht in v1 |

**Karteneinstellungen (minimal, 2 Stück):**
- `{"FPS und Uhrzeit", Checkbox, &InfoHUD_Clock}`, Default an.
- `{"Groesse", SliderInt, &InfoHUD_Scale, 75, 150, " %"}`, Default 100. Das Label „Groesse“ gibt es schon (→ „Size“, `settings_text.hpp:13`). Gleiches Muster wie Target HUD (`module_registry.cpp:25`).

## 3. Optik im Orbit-Stil (px bei 100 %, alles × `InfoHUD_Scale/100`)

```
┌────────────────────────────────┐  Rundung 10, Card mit 88 % Alpha, 1 px Border
│ ● FUSION           144 FPS 14:32│  Kopf 28 px
├──── (Accent-Linie 2 px, 40 px) ─┤  sonst 1 px Border
│▌ ⚔  Predict Double Hand         │  Zeile 22 px
│▌ ☠  Auto Crystal                │
│▌ 👁  Player ESP                  │
└────────────────────────────────┘  Innenabstand 10 px links/rechts, 8 px unten
```

**Kopf:**
- Accent-Punkt mit r = 3.
- „FUSION“ in `orbit_text_bold` 13 px, Farbe `Text`.
- Rechts `orbit_mono` 11 px in `Muted`: „144 FPS · 14:32“, nur wenn die Checkbox an ist.

**Zeile:**
- Links ein Accent-Balken, 2 × 14 px.
- Icon über den globalen Font `icons` (wie `orbit.cpp:165-168`), 11 px, Accent.
- Name in `orbit_text` 13 px, Farbe `Text`.

**Breite:**
- Maximum aus Kopfbreite und der breitesten Zeile, mindestens 150 px.
- Die Breite folgt dem Zielwert weich (Lerp).

**Leer:** Ist außer dem HUD kein Modul aktiv, erscheint eine Zeile „No active modules“ in `Muted`, 11 px.

**Animation:**
- Pro Modul ein Wert a ∈ [0, 1], der weich dem Zielwert folgt: `a += (ziel - a) * saturate(dt * 12)`. Das sind etwa 150 ms; dt wird auf 0.1 gekappt wie in `orbit.cpp:66`.
- Zeilenhöhe × a, Alpha × a, Einschub von links `(1 - a) * 12 px`.
- Ausgeschaltete Module bleiben sichtbar, bis a < 0.01. Dadurch gleiten sie sauber heraus.
- Der Zustand liegt in einem `static std::vector<float>` mit der Größe von `GetModules().size()`.

**Fonts:**
- Die Orbit-Helfer (`Pick`/`TextFont`) liegen im anonymen Namespace von `orbit.cpp:70-84` und sind nicht wiederverwendbar.
- `hotkey_toasts.hpp:34-38` enthält dieselbe Suche „nächste geladene Größe“ ein zweites Mal.
- Vorschlag: einen Helfer `inline ImFont* NearestOrbitFont(const OrbitFace&, float px, float* outSize)` in `resources.hpp` anlegen und auch in `hotkey_toasts.hpp` verwenden. Unterm Strich wird es weniger Code.
- Fehlen die Orbit-Fonts, wird auf `poppins` bzw. den ImGui-Standardfont zurückgefallen (Muster `hotkey_toasts.hpp:38`).

**Farben:**
- Nur `Theme::Current()`: `card`, `border`, `accent`, `text`, `muted`.
- `Fade` aus `hotkey_toasts.hpp:26` oder `ColA` aus `widgets.hpp:8`.
- Keine neuen Palettenfelder.

**Verhalten:**
- Es bleibt ein ImGui-Fenster mit `NoBackground`, damit es bei offenem Menü Eingaben bekommt. Gezeichnet wird über `GetWindowDrawList()`.
- Bei offenem Menü ist das ganze Panel per `InvisibleButton` ziehbar, nicht nur der Kopf. Bei geschlossenem Menü gilt `NoInputs` (wie `status_hud.hpp:43-50`).
- Clamp auf `WorkSize` wie bisher.
- Sichtbar, solange der Login durch ist (Aufruf in `menu.cpp:774`). Kein Ausblenden bei F1 oder offenem Screen (siehe oben).
- `menu_size_percent` und `font_size_percent` gelten **nicht**, die gehören zum Menü. Die Skalierung kommt nur aus dem eigenen Größen-Slider, wie beim Target HUD.

## 4. Kategorie, Profil, History, Share-Code

**Kategorie:**
- 3 = Visuals, dort liegt auch Target HUD (`module_registry.cpp:19`).
- Den Karteneintrag **ans Ende** des Arrays nach `Totem Warner` setzen (`:263-265`). So verschieben sich keine Indizes.
- Das Orbit-Raster scrollt (`orbit.cpp:966-969`), dort ist also Platz für 13 Visual-Kacheln.

**Profil-Schlüssel (`profile_version` bleibt 2):**
- `module.Info HUD.enabled|favorite|hotkey|hotkey_mode`
- `module.Info HUD.FPS und Uhrzeit`
- `module.Info HUD.Groesse`
- `hud.x`, `hud.y` bleiben.
- `hud.enabled` wird nur noch gelesen (Altprofile).
- Alles generisch über `config.cpp:100-113` und `:285-330`.

**History:**
- Modulwerte werden generisch erfasst (`history.hpp:22-27`).
- Slot `ui[9]` (`:31`, `:52`) hat `hud_enabled` gehalten. Kleinster sicherer Eingriff: Der Slot bleibt als `0` mit einem Einzeilen-Kommentar stehen, und die Zuweisung in `Restore` fällt weg.
- Ein Umnummerieren von `ui[10..26]` würde der Compiler bei einem Off-by-one nicht bemerken; deshalb nicht.

**Share-Code:**
- Die Karte wandert automatisch mit, weil `sharing.hpp:17` über alle Module geht.
- Die Position (`hud.x/y`) war nie Teil des Share-Codes und bleibt draußen.
- Hinweis: Ein Apply schaltet alle Module ab (`sharing.hpp:76`, `:84`) und schaltet sie nur mit der Option „enabled“ wieder ein. Ohne diese Option ist das Info HUD danach aus, genauso wie jedes andere Modul.

## 5. Dateien, Threading, Performance

| Datei | Änderung |
|---|---|
| `src/modules/settings.hpp:154-155` | neue Zeile: `inline std::atomic<bool> InfoHUD_Enabled=false, InfoHUD_Clock=true; inline std::atomic<int> InfoHUD_Scale=100;` (Default aus wie das alte `hud_enabled`) |
| `src/menu/module_registry.cpp:14` | `MODULE_COUNT` 37 → 38 |
| `src/menu/module_registry.cpp:265` | neue Karte `{"Info HUD",0,0,&InfoHUD_Enabled,3,{…2 Settings…}}` |
| `src/menu/module_registry.cpp:338` | Beschreibung: „Lists all other active modules, plus FPS and clock. Drag it while the menu is open; the position is saved in profiles.“ |
| `src/menu/module_view.hpp:112` | Icon `{"Info HUD", ICON_FA_LIST}`. Die Konstante per grep in `IconsFontAwesome5.h` bestätigen (nicht geöffnet). Fehlt sie, greift der Fallback auf `ICON_FA_EYE`. |
| `src/utils/config/settings_text.hpp:122` | `{"FPS und Uhrzeit","FPS and clock"}` |
| `src/menu/status_hud.hpp` | `Draw()` neu schreiben. `Watermark::Draw()` am Anfang (`:12`) bleibt. Gate ist `InfoHUD_Enabled`. |
| `src/menu/resources.hpp` (+ `hotkey_toasts.hpp:34-38`) | Font-Helfer (optional, spart eine dritte Kopie) |
| `src/utils/config/config.hpp:52` | `hud_enabled` löschen |
| `src/utils/config/config.cpp:70` | `hud.enabled` nicht mehr schreiben |
| `src/utils/config/config.cpp:156` | `hud_enabled=false;` entfernen |
| `src/utils/config/config.cpp:203-206` | `hud.enabled` → `InfoHUD_Enabled` (`settings.hpp` ist schon eingebunden, `:6`) |
| `src/menu/history.hpp:31`, `:52` | Slot 9 wird `0`, Restore-Zuweisung weg |
| `src/menu/diagnostics.hpp:27` | `"Info HUD: " << InfoHUD_Enabled.load() << " at " << hud_x << ',' << hud_y` |
| `src/menu/pages/settings_page.cpp:115-125` | Info-HUD-Zeile entfernen, `watermarkY = unloadY + 70` |
| `docs/ideas.md:8` | „Status-HUD“ aus den Pflichtstellen streichen (nur Doku) |

**Was nicht nötig ist:**
- `ModuleManager` und Runtime-Modul: Das Info HUD ist reine UI auf dem Render-Thread.
- Shutdown-Flags in `menu.cpp:740-743`.
- Java-Bridge, `check_protocol`, `VisualModulesBridge`.
- Kein Bridge-Paar, kein JAR-Wechsel.

**Threading:** Alles läuft auf dem Render-Thread (`Menu::Render`):
- `hud_x/y` sind wie bisher nicht atomar und werden nur dort benutzt.
- Die Flags sind atomar.
- Auch Hotkeys (`ProcessHotkeys`) sowie Load/Save (Config-Seite) laufen auf dem Render-Thread.

**Performance:** Pro Frame etwa 38 atomare Loads, ≤ 37 `CalcTextSizeA` und ein Sort über ≤ 37 Einträge. Das ist vernachlässigbar. Kein JNI, kein Cache nötig.

## 6. Schritte (jeder einzeln baubar)

1. **Modul anlegen:** Flags, Karte, `MODULE_COUNT`, Beschreibung, Icon, Label.
   - Prüfen: Build. Die Karte erscheint in Visuals (Orbit, Click GUI, Klassisch) und lässt sich schalten. Das alte HUD funktioniert noch unverändert über Settings.
2. **HUD neu zeichnen:** `status_hud.hpp` neu, Gate `InfoHUD_Enabled`, optional der Font-Helfer.
   - Prüfen: Build. Im Spiel erscheinen die Liste, das Ausblenden des eigenen Eintrags, die Animation, das Ziehen und die Skalierung.
3. **Altes HUD entfernen:** `hud_enabled` löschen sowie Config, History, Diagnose und Settings-Zeile anpassen.
   - Prüfen: Build. Der Compiler findet jeden übersehenen Verweis. Danach die Profiltests unten.
4. **Doku:** `docs/ideas.md:8` anpassen. `CHECKPOINT.md` erst nach dem Test.

Gemeinsamer Build: `.\tools\Build-Client.ps1 -Action build`. Die JAR bleibt inhaltlich unverändert; zum Testen die DLL neu injizieren.

## Testpunkte (für TEST-CHECKLIST)

**Karte:**
- Info HUD in Visuals.
- Mit Hotkey (Toggle): Toast „Info HUD on/off“, das HUD erscheint bzw. verschwindet.
- Hold-Modus funktioniert.

**Liste:**
- Das Info HUD steht nie in der Liste.
- Jedes andere aktive Modul erscheint, darunter Friends, die Aktionsmodule (Key Pearl …) und Alias-Karten (Reach, FastPlace).
- Sortierung nach Breite.
- Leerzustand „No active modules“.

**Animation:**
- Beim Ein- und Ausschalten gleitet und blendet die Zeile.
- Nach einem Ruckler springt nichts.
- Die Breite folgt weich.

**Bedienung:**
- Bei offenem Menü ziehbar, bei geschlossenem Menü gehen Klicks ins Spiel.
- Klemmt am Bildschirmrand, auch nach einem Auflösungswechsel bzw. Fenster-Resize.

**Optik:**
- Größe 75 / 100 / 150 %.
- FPS/Uhr an und aus.
- Helle und dunkle Themes, Custom Accent.
- Fallback ohne Orbit-Fonts.
- OpenGL und Vulkan.
- Unload (End) ohne Absturz.

**Profil:**
- Speichern enthält `module.Info HUD.*` und `hud.x/y`, aber kein `hud.enabled`.
- Altprofil mit `hud.enabled=1` → HUD an; mit `0` oder ohne Schlüssel → aus.
- Laden setzt die Position.

**Undo:**
- Schalten der Karte macht genau einen Schritt rückgängig.
- Ziehen erzeugt genau einen Undo-Schritt nach dem Loslassen.

**Share-Code:** Export und Import enthalten die Karte. Ohne die Option „enabled“ ist das HUD danach aus.

**Settings-Seite:**
- Die Zeile „Info HUD“ ist weg.
- Watermark-, Runtime- und Unload-Zeilen überlappen nicht (Orbit 80 / 100 / 120 %).

**Diagnose:** Der Report zeigt „Info HUD: …“.

## Nicht geprüft

- Build, Spieltest, ob `ICON_FA_LIST` existiert (Header nicht geöffnet), ob die Settings-Seite eine feste Inhaltshöhe hat.
- Für Sortierung und Ausschluss ist kein automatischer Test geplant (triviale Logik, manuelle Prüfung oben).
- Risiko: Das Mapping `hud.enabled` → `InfoHUD_Enabled` greift nur, weil die Config-Seite vor `Config::Load` alle Module zurücksetzt (`config_page.cpp:63-64`). Ein künftiger Ladepfad ohne diesen Reset (z. B. Profil-Schnellwechsel M5) würde den alten Zustand behalten.
