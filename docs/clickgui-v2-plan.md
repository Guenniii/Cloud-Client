# Click-GUI v2 „Orbit" – Umsetzungsplan

Stand 2026-11-14. Verbindlich: `docs/clickgui-design-v2.md`. Entwurf (Referenz): https://claude.ai/artifact/MY62iTmCevmJbbd9YFJheK.
Realistische ImGui-Vorschau mit echten Modulen: https://claude.ai/artifact/UuxpLd2NScr4AMZ4NcoBdY.
Pfade relativ zu `UniversalHookX/`. Nur Plan, noch kein Code.

## 1. Dateien

| Datei | Änderung |
|---|---|
| `src/menu/orbit.hpp`, `orbit.cpp` (neu) | `Orbit::Draw()`, Fenster `###client_menu_orbit` (`###client_menu_v2` gehört schon v1, clickgui.cpp:30) |
| `UniversalHookX.vcxproj` :207/:258, `.filters` :28/:89 | je zwei Einträge |
| `src/menu/menu.cpp` :20, :777-778 | Include, Auswahl |
| `src/utils/config/config.hpp` :50, `config.cpp` :69/:163/:182 | `menu_v2`, Key `ui.menu_v2` |
| `src/menu/history.hpp` :13/:28-33/:49-57 | `ui` 25 → 26 |
| `src/menu/pages/settings_page.cpp` nach :63 | Schalterzeile |
| `src/menu/resources.cpp` :84, `resources.hpp` :8 | nur bei Schrift-Variante B/C |
| `src/menu/README.md` :40-41, `TEST-CHECKLIST.md` nach :96 | Doku, neuer Abschnitt |

Unverändert bleiben clickgui.cpp, `DrawPhantomMenu`, widgets.*, module_view.hpp, navigation_layout.hpp, module_registry.*, custom_color_picker.cpp und **alle `src/utils/theme/*`**.

Die abgeleiteten Farben sind vier Alpha-Konstanten in orbit.cpp über `ColA(P::Accent(), …)`: Glow .55, Power-Ring .18, Segment-Halo .22, Punkte .14. Es gibt keine neuen Palette-Felder; alle 13 Presets sowie Custom und Animated Accent funktionieren (theme.hpp:74-76).

### Wiederverwendung

- **widgets.cpp**
  - `ColA`/`ColV` (:29/:35)
  - `DrawGlowCircle` (:42) für Zonen-Halo, Power und Profil-LED
  - `DrawGlowRect` (:60) für Linie und Keycap-Puls
  - `DrawCustomColorPopup` (Farb-Variante A)
  - Nicht wiederverwendet: `CustomToggle` und `DrawSliderRow` (ohne AlwaysClamp, :203).
- **module_view.hpp**
  - `categories` (:42): Ring = 0–3, Dock = 4–8 in Spec-Reihenfolge
  - `PAGE_*`, `ModuleIcon` (:73)
  - `Filter`, `CountScope`, `CountEnabledPerCategory`, `VisibleSorted` (:117-167)
  - `DrawConflictTooltip` (:170), `ResetModule` (:182), `SettingHelp` (:188)
  - `DrawOreSimSeedInput` (:204), `DrawClientPage` (:58)
- **Muster aus clickgui.cpp** (kopieren, v1 nicht anfassen)
  - Helfer `Sc/Fz/A/Mix/Anim/TextC/IconC/SpacedText` (:32-71)
  - unsichtbares `SliderInt` mit `AlwaysClamp` und `TempInputIsActive` (:147-168)
  - Hotkey-Logik (:236-285), `mode_used` (:346)
  - Fensterposition und Fade (:411-428), Verlauf (:439-444), Suchfeld (:613-634)
  - Abschluss mit `Particles` und `History::EndFrame` (:672-673)
- **navigation_layout.hpp:** nichts. Die Datei ordnet nur die Tab-Leiste des klassischen Menüs.
- **Registry:** `GetModules`, `Description`, `HasConflict`, `GetKeyName`. `CaptureHotkey` läuft bereits in `Menu::Render` (:763).

## 2. Umschalter und Profil-Key

- **Variable:** `inline bool menu_v2=false;` in config.hpp, wie `particles_enabled`; nur der Render-Thread nutzt sie.
- **Auswahl:** menu.cpp:778 wird zu `(Config::menu_v2 ? Orbit::Draw : kDrawMenu)();`. `kDrawMenu` bleibt die Wahl des „alten" Menüs im Code und steht derzeit auf `ClickGui::Draw` (v1).
- **Speichern und Laden:**
  - Speichern: eine Zeile nach config.cpp:69.
  - Laden: bei :163 `menu_v2=false` zurücksetzen, dann parsen wie `particles` (:267), nur `0|1`.
  - `profile_version` bleibt 2; alte Builds überspringen den Key (:272).
- **Settings-Seite:** Zeile „Orbit menu" mit `CustomToggle(bool*)`. Die Seite wird von allen Menüs geteilt, deshalb erscheint die Zeile auch in den alten Menüs; das ist dort die einzige sichtbare Änderung. Aus Orbit führt dieselbe Seite über das Dock zurück.

### Folgen

- Ein Profil ohne Key schaltet beim Laden aus Orbit zurück ins alte Menü.
- Es gibt keinen Autoload; `Config::Load` wird nur in config_page.cpp:64 aufgerufen. Nach jeder Injektion startet daher das alte Menü.
- **History:** `menu_v2` kommt in den Snapshot, damit „Profil laden → Undo" konsistent bleibt. `Orbit::Draw` muss `History::EndFrame()` aufrufen, sonst entsteht beim Zurückschalten ein einziger riesiger Undo-Schritt.
- **Share-Codes:** unberührt, weil die Payload nur Modul-Records und Theme enthält (sharing.hpp:14-27). Das Backup-Profil `before-import-…` (:75) bekommt den Key automatisch.

## 3. Was in ImGui nicht 1:1 geht

| Entwurf | Ersatz |
|---|---|
| box-shadow / drop-shadow | Große Flächen: `DrawGlowCircle/Rect`. Kleine Elemente (LED, Raute, Kopfzelle, Hexagon): lokaler Helfer mit 3–4 Lagen × `Theme::MenuGlow()`, weil `DrawGlowCircle` mindestens 12 Lagen à 48 Segmente zeichnet (widgets.cpp:46) |
| clip-path-Hexagon mit Rand | 6 Punkte mit `AddConvexPolyFilled` + `AddPolyline` (1/2 px); Glow als skalierte Polygon-Lagen. Treffertest Punkt-in-Hexagon über einen InvisibleButton für das ganze Raster, weil sich die Bounding-Boxen überlappen |
| Ring | `PathArcTo` + `PathStroke` 14 px; Halo als zweiter Strich 24 px @ 22 %; Treffertest über Winkel und Radius |
| radial-gradient-Halo | `DrawGlowCircle` (r ≈ 90, Auslauf ≈ 170) |
| Punktraster | ca. 1050 × `AddRectFilled` 2×2 statt Kreise |
| gestrichelt (Linie, Kreis, Keycap) | eigener `DashLine`/`DashArc` als AddLine-Schleife |
| Masken / runder Overflow | rechteckiger Clip; Inhalte halten Abstand zu den Ecken |
| Space Grotesk, JetBrains Mono, Gewichte | siehe Schrift-Varianten; Faux-Bold (zweimal mit 0.6 px Versatz) nur für Titel, Name und Zähler |
| Laufweite | zeichenweise wie `SpacedText` (clickgui.cpp:65) |
| 8.5-/7.5-px-Text | `AddText(font, size)` skaliert den 16er-Atlas; Mindestgröße 8 px |
| ›, ▸, ▾ | nicht in den Atlas-Bereichen (resources.cpp:55-68), daher FA-Chevron/Caret |
| Stroke-Icons | Font Awesome solid laut Spec; Stern „aus" = muted statt Umriss |
| Block-Cursor | statischer Block 7×14 hinter dem Text, solange das Feld inaktiv ist; aktiv der ImGui-Cursor (die Caret-Position gäbe es nur über die interne API) |
| Ring-Drehung | geht 1:1 als Winkel-Offset; Beschriftung und Trefferflächen bleiben fest |
| gestaffeltes Einblenden | Zeitstempel beim Wechsel; Kachel i startet nach i·18 ms |

### Schrift-Varianten

- **A)** Nur Poppins 16/25. Keine Änderung nötig, aber kein Mono-Look. Die Vorschau zeigt das mit dem Schalter „Poppins (vorhanden)".
- **B) Empfehlung:** Poppins plus Consolas Bold aus `C:\Windows\Fonts\consolab.ttf`, geladen wie arialbd (resources.cpp:84), ASCII-Bereich, Fallback Poppins. Kein Download, keine Lizenzfrage.
- **C)** Space Grotesk (3 Schnitte) und JetBrains Mono (2) einbetten.
  - Beide unter SIL OFL 1.1; der Lizenztext muss beiliegen.
  - Braucht einen Download (nur mit OK).
  - Statische TTFs, grob 0,8 MB mehr DLL (nicht geprüft).
  - Jede Größe ist ein eigener Atlas-Eintrag.

Der Zugriff läuft über `TextFont()`/`MonoFont()` in orbit.cpp, damit ein späterer Wechsel auf C nur eine Zeile kostet.

## 4. Settings-Typen und Maß-Prüfung

- **Checkbox:** Rauten-Schalter (`AddQuadFilled`); ein Klick aufs Label schaltet mit.
- **SliderInt:**
  - Die 20 Zellen sind nur Anzeige (`n = round(t·20)`).
  - Bedient wird stufenlos über das unsichtbare SliderInt mit Strg+Klick und AlwaysClamp.
  - Das Suffix steht getrennt in muted.
  - `nullptr` bzw. leere Suffixe abfangen (Reach, OreSim, Hit Effect).
- **ColorRGB:** Zeile mit Hexagon-Swatch und Hexwert. Für das Popup gibt es drei Varianten:
  - **A)** `DrawCustomColorPopup`: kein neuer Code, Hex-Eingabe vorhanden, aber kein Ring.
  - **B)** `ColorPicker3(PickerHueWheel|NoSidePreview|NoInputs)`: ca. 20 Zeilen und entspricht dem Spec-Wortlaut. Aber: SV-**Dreieck**, Ringbreite fest 8 % = 12.8 px, keine Hex-Eingabe.
  - **C)** Eigener Ring mit SV-Quadrat: ca. 100 Zeilen, exakt nach Spec.
  - Empfehlung B; die Entscheidung ist offen.
- **Hotkey:** Keycap wie clickgui.cpp:266-285.
  - Klick = Capture, Rechtsklick = löschen.
  - Backspace/Escape über `CaptureHotkey` (module_registry.cpp:387/:395).
  - Konflikt über `HasConflict` + `DrawConflictTooltip`.
  - Mittelklick auf eine Kachel = auswählen + Capture.
- **TOG/HOLD:** ausgeblendet für Short Wind Charge und Auto Pearl Catch (module_registry.cpp:367).
- **Favorit:** Stern, Rechtsklick auf die Kachel, Filterbutton.
- **Suche:** `Filter` über alle Kategorien und Setting-Namen.
- **Reset:** `MV::ResetModule`.
- **Beschreibung:** eine Zeile mit Ellipse, Volltext als Tooltip.
- **OreSim-Seed:** `DrawOreSimSeedInput` als eigene Zeile über beiden Spalten, unskaliert wie in v1.
- **Tick-Ring und „/35":** `GetModules().size()` in Registry-Reihenfolge; `MODULE_COUNT` ist privat (:13).

### Prüfung gegen die Registry

- **Raster:**
  - Pro Kategorie: Combat 10, Movement 6, Utility 8, Visuals 11. Das sind höchstens 2 Zeilen, und 12 Kacheln passen (y 110–268, Reiter ab 276). Eine dritte Zeile kollidiert mit dem Reiter.
  - Suche und Favoriten zeigen bis zu 35 Kacheln.
  - Lösung: Raster als Child mit Clip und Mausrad (Muster clickgui.cpp:643). Die Linie wird ausgeblendet, wenn die gewählte Kachel nicht sichtbar ist.
- **Inspector:**
  - Unter dem Kopf bleiben ca. 177 px. Das reicht für 5 Schalter- bzw. Farbzeilen und 3 Slider.
  - Zu groß sind 9 von 35 Modulen (Schalter+Farben / Slider): Target HUD 4/4, Loot Beacon 6/4, Item ESP 8/1, Storage ESP 11/2, Hit Effect 2/5, Aim Assist 3/5, Safe Anchor 5/4, Auto Crystal 1/4, OreSim 11/1 plus Seed-Feld 108 px.
  - AutoTotem, Echo Trail und Projectile Prediction passen genau.
  - Lösung: Settings-Bereich als Child mit Mausrad-Scroll und schmalem Indikator, kein Paging.
- **Breiten** (geschätzt):
  - Label-Spalte ca. 182 px; rund 12 Labels sind zu lang. Farb-Label ca. 106 px.
  - Inspector-Name ca. 220 px; „Projectile Prediction" ist zu breit.
  - Hex-Name 60 px in 2 Zeilen reicht nicht für „Predict Double Hand", „Auto Shield Breaker" und „NoJumpDelay".
  - Lösung überall: Clip + Ellipse + Tooltip.
- **Titel 44 px:** Bis zum Suchfeld bleiben ca. 286 px; Such- und Favoritenmodus brauchen kurze Titel.
- **Linie:** Bei Kacheln mit Mittelpunkt x < 191 (Zeile 1 Spalte 0–1, Zeile 2 Spalte 0) läuft sie durch den Reiter. Dort endet sie an der Reiter-Oberkante (y 276).

## 5. Schritte

Bis einschließlich Schritt 5 wird getestet, indem man `kDrawMenu = Orbit::Draw` (menu.cpp:777) lokal setzt und nicht committet.
- Build nach jedem Schritt: `.\tools\Build-Client.ps1 -Action build`.
- Test nach jedem Schritt: TEST-CHECKLIST :89 (alte Menüs unverändert) sowie OpenGL und Vulkan (:10).

1. **Gerüst + Tokens**
   - Dateien: orbit.hpp/.cpp, vcxproj/filters, Include; optional Mono-Font in resources.
   - Inhalt:
     - Fenster 900×600 × `min(menu_size/100, Display-Fit)`, Rundung 20, Rand, Verlauf, Partikel, Fade, `History::EndFrame`.
     - Helfer (Text, Anim, Dash, Hex, kleiner Glow) und Token-Konstanten.
   - Test: leeres Fenster, verschiebbar, 80/100/120 %, helles und dunkles Preset, Custom Accent.
2. **Navigation**
   - Inhalt:
     - Halo, Marke, Ring mit Zuständen und Klick, „0N ON", Tick-Ring, Strichkreis, Mittelscheibe.
     - Dock mit Tooltip, Profil-Leiste, Trenner, Punktraster, Kopf.
     - Client-Seiten über `DrawClientPage` in der rechten Zone.
   - Test: Zähler = aktive Module; alle 5 Seiten bedienbar bei 80/100/120 %; Profilname wird angezeigt.
3. **Raster + Verbindung + Inspector-Kopf**
   - Inhalt:
     - Kacheln mit Zuständen; Klick, Doppelklick, Rechts- und Mittelklick.
     - Suche, Favoritenfilter, Raster-Scroll, Linie, Reiter.
     - Inspector-Kopf: Power, Name/Beschreibung, Keycap, TOG/HOLD, Stern, Reset.
   - Test: :94/:95 sinngemäß; Suche „glow" (11 Treffer) und „e" (Scroll); Auswahl bei Kategorie- und Filterwechsel; leerer Zustand.
4. **Settings-Widgets**
   - Inhalt: Rauten-Schalter, Segment-Slider, Farbzeile mit Popup, Seed-Feld, Scroll, `SettingHelp`-Tooltips.
   - Test:
     - Werte sind im alten Menü identisch; Strg+Klick wird geklemmt.
     - Storage ESP, OreSim und Hit Effect scrollen.
     - Seed und Reset (:95); Profil speichern und laden.
5. **Animationen**
   - Inhalt: `k = 3000/ms`, also Hover 25, Schalter 18.75, Zellen 33, Linie 16.7, Kategorie 13.6, Ticks 12, Öffnen 9.4; Puls 0.8 s.
   - Test: Sichtprüfung; Frame-Zeit über `Performance::Client` gegen v1; Vertexzahl bei 12 aktiven Modulen.
6. **Umschalter + Profil-Key**
   - Dateien: alle aus §2, README, neuer Abschnitt in TEST-CHECKLIST.
   - Test:
     - Schalter in allen drei Varianten; Profil mit und ohne Key; Undo.
     - Share-Export und -Import unverändert, Backup enthält den Key.
     - :96 (Themes/Glow); `python .\tools\test_client_build.py`.

## 6. Java-Bridge

Keine Änderung nötig. Orbit ist reines ImGui/C++.
- Unverändert bleiben Modulnamen, Setting-Keys, JNI-Signaturen, `AssistOptions` (COUNT 30) und `VisualModulesBridge.frame` (29); `check_protocol` ist nicht betroffen.
- Target HUD, Status-HUD und Watermark hängen an `Menu_Enabled`, nicht an der Variante (menu.cpp:768/:780).
- Eine Ausnahme gäbe es nur bei neuen Modulen oder Settings, und die sind nicht Teil dieses Plans.

## 7. Widersprüche Spezifikation ↔ Entwurf (Spec gilt)

1. **Slider-Kopfzelle:** Der Entwurf zeigt Glow 8 immer, die Spec nur bei Hover. Im Normalzustand also hover-Farbe ohne Glow.
2. **Leere Keycap:** Im Entwurf Unterkante 1 px und transparent; die Spec nennt 3 px als generelles Maß.
3. **Capture-Glow:** Entwurf 50 %, Spec-Token 55 %.
4. **Zähler:** Im Entwurf ergeben die Kategorien 3+2+4+4 = 13, Mitte und Ticks zeigen aber 12. Im Code kommen beide aus derselben Quelle.
5. **Gewählte Kachel:** Im Entwurf ist „gewählt" immer zugleich „an" (Füllung soft, Icon und LED accent). Laut Spec ist die Auswahl nur ein 2-px-Rand plus Glow, unabhängig von an/aus.
6. **Tooltip-Pille:** Main zeigt sie linksbündig versetzt (x 76 bei Buttonmitte 111) und 30 px Oberkante zu Oberkante. Components zeigt sie zentriert mit 6 px Abstand. Die Spec sagt „30 px darüber"; Empfehlung: zentriert.
7. **Farb-Popup:** Der Entwurf zeigt ein SV-Quadrat 90 und einen Ring 16. Das in der Spec genannte `ColorPicker3`/`PickerHueWheel` liefert ein Dreieck und 12.8 px.
8. **Raute „9×9":** Im Entwurf ein um 45° gedrehtes Quadrat mit Seite 9 (Diagonale 12.7); in der Spec mehrdeutig.
9. **Beschreibung:** Der Entwurf kürzt den Text inhaltlich. Real ist es der Registry-Text mit Ellipse (module_registry.cpp:297).
10. **Labels:** Der Entwurf zeigt deutsche Rohnamen („Eigene Spur", „Dauer"). Die UI zeigt `DisplayName()` auf Englisch.
11. **Icons:** Der Entwurf nutzt Stroke-Icons mit anderen Motiven (Schwert, Schraubenschlüssel, Sprout, Layers, User). Die Spec verlangt Font Awesome; ihr Verweis „Mapping in menu.cpp" ist veraltet, es liegt in module_view.hpp:42/:73.
12. **Marke:** Der Entwurf hat zusätzlich das Badge „MC 26.2" und Fettschrift.
13. **Zonen-Halo:** Im Entwurf 12 % auf 170 px. Die Spec nennt nur „Ring-Halo 22 %", und das ist der Segment-Halo.
14. **Reihenfolge:** Im Entwurf steht der Favorit Player ESP an letzter Stelle. `VisibleSorted` stellt Favoriten nach vorn; damit verschiebt sich auch „VIS·03".
15. **„Zeile 116×28":** In der Spec heißt es Zeile, im Entwurf ist das der Button. Die Zeile selbst ist 30 hoch.
16. **Quick-Farben:** Die Werte im Entwurf weichen vom bestehenden Popup ab (custom_color_picker.cpp:127).

Übereinstimmend sind die Visuals-Zuordnung (11 Module, gleiche Namen und Reihenfolge wie die Registry), die Echo-Trail-Settings und ihre Bereiche, die Ring-Geometrie und die Bewegungswerte.

## 8. Lücken der Spec

- Überlauf im Inspector und im Raster (§4).
- Auswahlregeln: Start, Kategorie-Wechsel, weggefilterte Auswahl, leerer Zustand.
- Undo-Button und „Only active" fehlen.
- **Client-Seiten:**
  - Was „Inspector-Stil" genau heißt und welche Kopftexte gelten.
  - Ob die Suche dort sichtbar ist.
  - Wie der Ring ohne aktives Segment aussieht.
- Titel im Such- und Favoritenmodus.
- Kürzungen und Tooltips.
- **Slider:** stufenlos oder gerastert; was bei einem Wert über dem Minimum, aber 0 gefüllten Zellen gilt.
- Fehlende Zustände: Favoritenfilter aktiv, Stern aktiv, Reset-Hover, Suchfokus, Hover auf aktiver Kachel, Namensfarbe „an". TOG/HOLD fehlt in der Komponententabelle.
- Mittelklick auf eine nicht gewählte Kachel.
- Reiter-Kürzel der anderen Kategorien und die Bedeutung des Index.
- Reihenfolge der Ticks.
- Ob sich die Beschriftung mit dem Ring dreht.
- Strichmuster, Alpha des Zonen-Halos, Fensterschatten.
- Verhalten unter 900×600 und Mindest-Textgröße bei 80 %.
- Linie über dem Reiter.

## 9. Risiken und offene Fragen

Risiken:
- **Vertex-Budget und FPS:** viele Glows plus ca. 1050 Punkte. Nicht geprüft ist, ob das Backend Vertex-Offset bzw. 32-Bit-Indizes unterstützt; das liegt in `dependencies/`.
- **Lesbarkeit:** 8.5-px-Text bei 80 % Menügröße.
- **Client-Seiten:** sind fest in px gebaut und bisher nur ab 616 px Breite getestet; in Orbit stehen 524 px zur Verfügung.
- **In v2 festhängen:** Ein Profil mit `ui.menu_v2=1` bei kaputtem Orbit. Ausweg: Profil ohne Key laden.
- **Arbeitsbaum:** clickgui.cpp, module_view.hpp und history.hpp sind untracked, die vcxproj ist geändert. Vor dem Start den v1-Stand committen, damit sich „altes Menü unverändert" per Diff prüfen lässt.
- **Atlas-Wachstum** bei Schrift-Variante C.

Offene Fragen:
1. Ist das „alte Menü" das klassische oder v1? `kDrawMenu` steht auf v1. Alternative: ein Key mit drei Werten.
2. Schrift A, B oder C? Download nur mit OK.
3. Farb-Popup A, B oder C?
4. Ist Scroll im Inspector und im Raster in Ordnung, oder soll die Suche auf die Kategorie begrenzt werden?
5. Undo-Button in Orbit: ja, und wo?
6. Client-Seiten unverändert einbetten (Empfehlung) oder umgestalten? Umgestalten würde die geteilten Seiten ändern.
7. Reicht Persistenz nur über das Profil (kein Autoload)? Soll `menu_v2` ins Undo?
8. Leere Keycap-Unterkante, Badge „MC 26.2", Rautengröße, Tooltip-Position?
9. Key-Name `ui.menu_v2`, obwohl die v1-Fenster-ID schon „v2" heißt, oder lieber `ui.menu_orbit`?

## 10. Entscheidungen (2026-11-14)

Diese Punkte ersetzen die Empfehlungen in den Abschnitten 2–4 und 9.

1. **Menüwahl:** alle drei Menüs sind wählbar. Key `ui.menu_style` = 0 Klassisch, 1 v1, 2 Orbit. Standard ist 1 (v1, wie `kDrawMenu` heute). Unbekannte Werte fallen auf 1 zurück. In den Settings steht eine Zeile mit drei Optionen. `ui.menu_v2` entfällt.
2. **Schrift C:** Space Grotesk und JetBrains Mono Bold (OFL) werden aus github.com/google/fonts geladen und in einen neuen Header eingebettet. Die bestehenden Font-Header bleiben unberührt. Die OFL-Lizenztexte kommen mit dazu. Atlasgröße und DLL-Größe werden nach dem Build gemessen.
3. **Farbwähler C:** ein eigener Hue-Ring mit SV-Quadrat genau nach Entwurf, gezeichnet mit ImDrawList. Kein `ColorPicker3`, kein `DrawCustomColorPopup`.
4. **Platz:** Raster und Inspector scrollen, mit einer Scrollleiste im Orbit-Stil. Das Fenster bleibt 900×600 (Spec).
5. **Undo:** Orbit bekommt keinen Undo-Button. `ui.menu_style` geht trotzdem in den History-Snapshot, damit Undo in Klassisch und v1 konsistent bleibt.
6. **Client-Seiten:** Sie werden unverändert in einen scrollbaren Bereich eingebettet, notfalls mit horizontalem Scroll.
7. **Persistenz:** nur über das Profil, kein Autoload. Nach jeder Injektion startet v1.
8. **Detail-Widersprüche** (Abschnitt 7 und Frage 8): Es gilt die Spezifikation. Wo die Spezifikation schweigt, gilt der Entwurf.
