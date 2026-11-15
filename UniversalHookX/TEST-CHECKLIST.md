# Test-Checkliste – Minecraft 26.2

Stand: 2026-10-01. Der Nutzer hat den zuletzt gemeinsam gebauten Stand im Spiel
als funktionierend bestätigt. Die folgenden Punkte dienen als wiederholbare
Prüfung für zukünftige Änderungen; sie wurden nicht erneut durch Codex im Spiel getestet.

- [ ] Gemeinsamer Release/x64-Build erfolgreich; JNI-Abgleich ohne Fehler.
- [ ] Installation mit `-DryRun` prüfen, dann bei geschlossenem Minecraft installieren.
- [ ] Minecraft starten: DLL und Bridge initialisieren ohne Fehler.
- [ ] OpenGL: Menü, Hotkeys, Themes, ESP und OBS-Aufnahmeverhalten prüfen.
- [ ] Vulkan: dieselben Darstellungs- und OBS-Prüfungen wiederholen.
- [ ] SeedCracker: Wracks erfassen, Fortschritt und verifizierten Seed prüfen.
- [ ] OreSim: manuellen Seed übernehmen und Erzmarkierungen prüfen.
- [ ] AntiBot: aktivierte Filter bei ESP und Zielmodulen prüfen.
- [ ] Silent Aim, Aim Assist, HitCrystal, Auto Mace und Breach Swap prüfen.
- [ ] Safe Anchor und Shield Breaker: Aktionen, Delays und Slot-Rückwechsel prüfen.
- [ ] Auto Armor im Inventar und Refill bei geschlossenem Inventar prüfen.
- [ ] Predict Double Hand: Kristall auf/unter Fußhöhe, Ersatz-Totem,
      manuellen Slotwechsel und Rückwechsel bei sicherer Situation prüfen.
- [ ] Hit Effect: schnelle Folgeangriffe, Größe, Ausbreitung und Glow-Regler prüfen.
- [ ] Movement-Module, AutoTool und Fake Lag prüfen.
- [ ] Module deaktivieren: keine fortlaufenden Aktionen oder veralteten Panel-Werte.
- [ ] DLL entladen: Minecraft bleibt offen und die Konsole schließt sich.

## Gesicherter Stand

Das C++- und Java-Projekt bleiben getrennt. Ihre lokalen Git-Commits werden in
`CHECKPOINT.md` zusammen dokumentiert. Ein Git-Checkout stellt Quelldateien wieder
her, ersetzt jedoch keine geladenen DLLs oder JARs. Zum Verwenden eines alten
Stands beide Projekte passend auschecken und danach neu bauen/installieren.

Der Build benötigt weiterhin den lokalen Gradle/Loom-Cache und eine vorhandene
Bridge-JAR als Basis für Bibliotheken und Ressourcen; siehe `tools/README.md`.
Die zu diesem Stand gehörenden Binärdateien werden zusätzlich lokal mit
Prüfsummen gesichert. Git-Commits werden nicht automatisch veröffentlicht.

## Modulkonflikte nach dem Checkpoint

- [ ] Safe Anchor oder Pearl Catch aktiv ausfuehren; Auto Mace/Shield Breaker
      duerfen keine fremden Slots oder Folgeangriffe einschieben.
- [ ] Auto Mace bei blockierter Hotbar: kein visuelles Kopfzielen; nach Freigabe
      darf ein weiterhin gueltiger Fall erneut geprueft werden.
- [ ] Predict Double Hand waehrend einer Combo: Totem uebernimmt, alte
      Folgeaktionen bleiben abgebrochen, manueller Slotwechsel wird respektiert.
- [ ] Auto Tool gibt den Slot fuer eine explizite Itemaktion frei; Auto Armor
      und Refill unterbrechen Auto Tool nicht.
- [ ] Settings / Action coordination zeigt den letzten Konflikt. Beim offenen
      Menue sind Module pausiert; die Anzeige beschreibt keine wartende Attacke.

## Neue visuelle Module

- [ ] Projectile Prediction: Perle in Haupt-/Nebenhand, Bogen bei voller/teilweiser
      Ladung und Wind Charge testen; Marker an Wand/Boden und an einem Ziel pruefen.
- [ ] Fliegende Pfeile/Perlen zeigen einen Verlauf, eingebettete Pfeile nicht.
- [ ] Sound ESP: entfernte Schritte, Wuerfe und Explosionen pruefen; Filter,
      Reichweite, Ausblenden und Richtungsmarker hinter der Kamera testen.
- [ ] Storage ESP: Truhe, Fass, Shulker und Endertruhe durch Waende pruefen;
      Typfilter, abgebaute Container und entladene Chunks testen.
- [ ] Alle drei: Hotkeys, Profilwerte, Theme-Farben, OpenGL/Vulkan und OBS pruefen.
- [ ] Deaktivieren, Menue/Fokuswechsel und DLL-Unload: keine alten Marker oder
      weiterlaufenden Sound-Listener; Minecraft bleibt offen.

- [ ] Storage-Farbpicker: jede Containerart anders faerben; Box, Glow und Label
      vergleichen. Profil speichern/laden, Schwarz/Weiss sowie Theme-Override testen.
- [ ] Prediction: Boden-, Wand- und Blockkantentreffer testen; der getroffene
      Block wird umrandet und gefuellt, bei Entitaetentreffern bleibt die Blockbox aus.
- [ ] Aufprallmarker und Aufprallblock jeweils unabhaengig aus-/einschalten.


## Item ESP, Friends and Explosion Preview

- Both Vulkan and OpenGL: enable Item ESP; drop a totem, weapon, and block. Check each filter, count/name/distance, glow, range and custom colors. Disable and verify instant removal.
- Friends: add an account name, save, restart; verify persistence in bin/friends.txt. Name matching ignores case. Edit/remove and verify immediate update. Profile save/load preserves the global protection toggle and ESP color.
- With AntiBot off and on: Silent Aim, Aim Assist, Auto Mace, Breach Swap and Shield Breaker must ignore friends. Manual vanilla attacks remain available. Friends stay visible in box and skin ESP with the selected color.
- Explosion Preview: nearby live crystals show estimated self damage and health-based LETHAL indication; aim near a non-friend player for target damage. A charged anchor is shown when directly targeted outside the Nether. Empty/Nether anchors are omitted.
- Check armor, protection/blast protection, resistance, peaceful difficulty, creative invulnerability and solid cover reduce predicted damage. Values are estimates: vanilla crystal cover sampling / 27 anchor samples, no custom server damage, no future movement or special anchor water shielding.
- Disable, disconnect/rejoin and unload: no stale markers/listener references or bridge errors. Restart Minecraft after the DLL/JAR pair changes.

## Middle-click Friends / dedicated tab

- Enable Friends and the middle-click option. Aim at a live player and click once: one saved friend, immediate targeting exclusion and ESP color. Holding or clicking again must not remove or duplicate the friend.
- Disable the option or Friends itself: no additions. Click on blocks/items, while a game screen is open, while the client menu is open, or with another app focused: no additions and no delayed replay.
- Friends icon opens a page with manual add (Enter or button), per-name removal and scrolling without a visible scrollbar. Existing comma/semicolon/multiline friends.txt files remain readable.
- Invalid or overly long names are rejected. Removing one account preserves others. Restart to verify saved list, and profile reload to verify the middle-click toggle.


## Click-GUI (ClickGui::Draw, Settings > Menu style = Click GUI)

- Select "Click GUI" in Settings > Menu style (default after injection) and open the menu in OpenGL and Vulkan. Switch to "Classic": the classic menu must look and behave as before (search, favorites, Only active, Collapse, Undo, hotkey badge, reset, OreSim seed).
- Sidebar: tabs with count badge (= enabled modules), gliding active pill, hover states; CLIENT tabs open Settings, Themes, Profiles, Friends, SeedCracker; profile chip shows the active profile. Page change cross-fades.
- Menu size 80 / 100 / 120 %: layout scales; below 760 px width the sidebar shows icons only (dot badge, label tooltip) and settings use one column. Check on a small screen/window too.
- Header: title, "N of M enabled", search (all categories, case-insensitive, also finds setting names), Favorites filter. Cards scroll with the mouse wheel; off-screen cards cost no frame time.
- Card: toggle, star (favorites sort first), expand/collapse animation (header click, but not on badge/star/toggle), description, two columns (checkbox/color left, sliders right), slider drag and Ctrl+click typing (visible while active), color box opens the existing popup, tooltips on settings.
- Hotkey badge: Set Key / set / hover / capture (pulsing border; Backspace clears, Escape cancels) / conflict ("Key conflict" + tooltip); right click clears.
- Toggle|Hold segment changes `hotkey_mode` (saved in profile); hidden for Short Wind Charge and Auto Pearl Catch. "Reset module" restores defaults (OreSim also clears the seed field).
- Switch themes/presets, custom accent, animated accent and gradient: all colors follow; menu glow slider affects glows.

## Orbit (Orbit::Draw, Settings > Menu style)

- Menu style: the row "Menu style" (Classic / Click GUI / Orbit) on the Settings page exists in all three menus; clicking switches on the next frame without flicker or stuck windows (window IDs are separate). Default after injection: Click GUI.
- Profile: save a profile with Orbit active, load it in another style: Orbit returns. Load a profile without `ui.menu_style` (old build) or with an invalid value: Click GUI. `profile_version` stays 2. Undo after a profile load restores the previous menu style. Share-code export/import is unchanged; the `before-import-...` backup profile contains `ui.menu_style`.
- Window: 900 x 600 scaled by menu size 80 / 100 / 120 % (and fitted to small displays), draggable by empty areas (gaps, centre, corners), fade-in on open, ring turns -20 deg to 0, particles and border like the Click GUI. OpenGL and Vulkan.
- Themes: light and dark presets, custom accent, animated accent, gradient, menu glow 0 / 100 / 200 %: all colours follow, glow reacts.
- Navigation: ring segments (hover, selected, click on segment and label), counters "0N ON", tick ring (one tick per module, lit when enabled), centre disc "online / total", dock buttons with tooltip pill, profile bar shows the active profile, dashed divider. All five client pages open in the right zone at 80 / 100 / 120 % without horizontal scroll or overlapping elements (Settings, Themes, Profiles, Friends, SeedCracker).
- Grid: tile states (off / on / hover / selected / favorite), left click selects, double click toggles, right click favorite, middle click selects and starts hotkey capture; names clip with an ellipsis and tooltip; wheel scroll with indicator (search "e" shows many tiles); search "glow" finds modules by setting name; favorites filter; empty result message; selection falls back to the first visible tile when the filter hides it.
- Inspector: power button, name and description with ellipsis/tooltip, keycap (click = capture, Backspace clears, Escape cancels, right click clears, conflict "!" with tooltip), TOG/HOLD hidden for Short Wind Charge and Auto Pearl Catch, star, reset (OreSim also clears the seed field). Capture ends when the selection or page changes.
- Settings widgets: diamond toggles (also by clicking the label), 20-cell sliders (drag, Ctrl+click typing is clamped to the range, value and unit shown), colour rows with hexagon swatch; the colour picker (hue ring, SV square, hex field, quick colours, outside click / Escape closes) writes the same 0xRRGGBB values as the old popup. Values behave exactly as in the old menus. Storage ESP, OreSim (seed field row), Hit Effect, Aim Assist, Target HUD, Loot Beacon, Item ESP, Safe Anchor and Auto Crystal scroll in the inspector.
- Animations: tiles fade in staggered after a category change (not on every search keystroke); the connection line grows from the selected tile on every selection change (also tile to tile), a light runs down the line and the end dot flashes, repeating subtly while the tile stays selected; the line is hidden when the tile is scrolled out and grows again when it returns; click flash on the tile; slider cells follow value changes smoothly. No visible frame-time regression against the Click GUI (Performance::Client).
- Text: 10 px readability floor holds at 80 %; Orbit fonts load (no fallback to Poppins); "OFF" stays inside the toggle at 80 %.
