# Ideen: neue Module und Verbesserungen

Stand 2026-11-12. Nichts geändert; Vorschläge auf Basis von docs/audit.md und gezieltem Code-Lesen. Kein Build, nichts im Spiel getestet.
Pfade: `src/` = `UniversalHookX/src/`, `java/` = `SeedcrackerX-master/src/main/java/phantomui/`.

**Pflichtstellen für jedes neue Modul (geprüft)**
- **C++-Karte:** Flags in `src/modules/settings.hpp`; Karte in `src/menu/module_registry.cpp:17-254` und `MODULE_COUNT` (`:13`, fest 34) mitziehen. Zu klein ergibt einen Compile-Fehler, zu groß einen Null-Eintrag und Crash.
- **C++-Listen:** Beschreibung (`module_registry.cpp:287-327`), Icon (`src/menu/menu.cpp:79ff`), Labels (`src/utils/config/settings_text.hpp`), Status-HUD (`src/menu/status_hud.hpp:15-24`), Diagnose (`Report` in `src/menu/diagnostics.hpp`).
- **UtilitySuite-Option:** in `src/input/assist_options.hpp` und `java/assist/AssistOptions.java` anhängen (COUNT mitziehen, Clamp in `snapshot`); genau eine Zuweisung in `src/input/utility_suite.hpp:66-91` (prüft `check_protocol`).
- **UtilitySuite-Instanz:** eine `AssistModule`-Instanz anlegen (`src/modules/utility/assist_modules.cpp:13-17`, `runtime_modules.hpp`, `ModuleManager.cpp:25-29`). Ohne sie ruft niemand `Publish`, wenn nur das neue Modul aktiv ist.
- **Neuer Hotbar-Owner:** enum (`java/movement/HotbarCoordinator.java:15`), `name()`-switch (exhaustiv, Compile-Fehler wenn vergessen), `mayPreempt`, Cancel-Liste in `UtilitySuiteBridge.clear()` (`:48-53`).
- **Bündeln:** Die UtilitySuite-Vorschläge 1, 2, 6, 7 und 10 in einem Bridge-Schritt umsetzen (COUNT 26 → n). Das ergibt einen gemeinsamen Build, ein Commit-Paar und einen Minecraft-Neustart.

## 1. Neue Module

### Combat

**1. Trigger Bot**
- **Für den Spieler:** greift automatisch an, sobald das Fadenkreuz auf einem gültigen Spieler liegt und der Angriffs-Cooldown geladen ist.
- **Baut auf:** Zielprüfung wie `ShieldBreakerAssist.targetReady` (`java/assist/ShieldBreakerAssist.java`: Fadenkreuz-Treffer, AntiBot, Friends, Sichtlinie, Reichweite); Cooldown- und Clip-Prüfung wie `java/combat/BreachSwapBridge.java`; Angriff über `HotbarAction.attack` (public, enthält den Friends-Check); Takt über `UtilitySuiteBridge.tick()` (`:33-42`).
- **Neu:** `assist/TriggerAssist.java` mit Screen-Guard, Owner `TRIGGER`, drei Optionen (an, Reaktionszeit in ms, nur mit Waffe).
- **Bridge:** ja, AssistOptions +3 und ein Owner.
- **Aufwand/Nutzen:** klein–mittel / hoch.
- **Prüfen:** kein Angriff auf Freunde oder Bots, bei offenem Screen oder Menü, bei Cooldown < 0,9; kein Doppelschlag zusammen mit Auto Mace oder Breach Swap.

**2. Cooldown Guard**
- **Für den Spieler:** verschluckt Linksklicks auf Spieler, solange der Angriff nicht geladen ist, damit Spam-Klicks keine schwachen Treffer erzeugen.
- **Baut auf:** Klick-Kette in `src/hooks/hooks.cpp:87-92` (`TryShieldClick || AutoMace::TryClick || BreachSwap::TryClick`); Muster `UtilitySuite::TryShieldClick` (`utility_suite.hpp:128-134`) und `UtilitySuiteBridge.shieldClick()` (`:44-47`).
- **Neu:** `guardClick()` in Java; `TryGuardClick` als letztes Glied der Kette; das Flag in die äußere Bedingung (`hooks.cpp:90`) aufnehmen; zwei Optionen (an, Schwelle in %).
- **Bridge:** ja, eine Methode und AssistOptions +2.
- **Aufwand/Nutzen:** klein / mittel.
- **Risiko:** `UtilitySuite::Load` verlangt alle Method-IDs (`:46`). Mit einer alten JAR fällt die ganze Suite bis zum Neustart aus.
- **Prüfen:** Abbauen und Klicks auf Mobs bleiben unberührt.

**3. Auto Surround**
- **Für den Spieler:** setzt auf Tastendruck Obsidian um die eigenen Füße als Schutz gegen Kristalle.
- **Baut auf:** Platzier-Helfer in `java/combat/SafeAnchorBridge.java` (`free` :138, `visibleHit` :142, `placement` :152, `send` :166 → `HotbarAction.interactBlock`, serverautoritativ); Sequenz mit Delays und Lease (`:215`); C++-Muster `src/input/combat/safe_anchor.hpp` und `safe_anchor_module.cpp`.
- **Neu:** Helfer in eine package-private Klasse ziehen; `combat/SurroundBridge` (`publish`/`clear`); Zielpositionen, Reihenfolge, Abbruch bei Bewegung; Owner `SURROUND`.
- **Bridge:** ja, neue Klasse.
- **Aufwand/Nutzen:** groß / hoch.
- **Risiko:** `SafeAnchorBridge` nur in Signaturen gelesen; jede Platzierung braucht eine sichtbare Nachbarfläche.

### Movement

**4. Elytra Swap**
- **Für den Spieler:** tauscht per Aktionstaste Brustplatte und Elytra, ohne das Inventar zu öffnen.
- **Baut auf:** `InventoryTransactions.transfer` (`java/assist/InventoryTransactions.java`; Klick-Klick-Rückklick ist bereits ein Tausch; Rüstungsslot 6); `EquipmentScores.armorScore`; Owner `AUTO_ARMOR`; Aktionstasten-Muster `ShortWindCharge_Request` (`src/input/action_key.hpp`).
- **Neu:** `UtilitySuiteBridge.swapChest()` über `mc.execute` mit `valid()` und `InventoryTransactions.ready`; Request-Flag; dritter Fall im fest verdrahteten Aktionstasten-Zweig (`module_registry.cpp:361-368`).
- **Bridge:** ja, eine Methode, kein Layout.
- **Aufwand/Nutzen:** klein / mittel–hoch.
- **Prüfen:** Auto Armor tauscht nicht zurück (`AutoArmor_KeepElytra` ist per Default an; Auto Armor arbeitet nur bei offenem Inventar); Cursor-Stack ist danach leer.

**5. Fall Clutch**
- **Für den Spieler:** wirft bei gefährlichem Fall kurz vor dem Aufprall automatisch eine Wind Charge nach unten.
- **Baut auf:** Wurf mit Pitch 90 aus `java/movement/ShortWindChargeBridge.java` (`HotbarAction.find/ready/usable/use`, im selben Paket nutzbar); Fall-Erkennung wie `java/combat/AutoMaceBridge.java`; Publish-Takt-Muster `AutoToolBridge.publish`; Owner `WIND_CHARGE`.
- **Neu:** `movement/FallClutchBridge` (`publish(minFall, spoof)`/`clear`) mit Bodenabstand per `level.clip`; C++-Modul nach `src/modules/movement/safe_walk_module.cpp`.
- **Bridge:** ja, neue Klasse.
- **Aufwand/Nutzen:** mittel / mittel.
- **Risiko:** Timing hängt vom Ping ab; darf nicht während eines Auto-Mace-Falls feuern (per Setting oder Coordinator ausschließen).

### Utility

**6. Totem-Slot nachlegen (Refill-Erweiterung)**
- **Für den Spieler:** legt nach einem Pop wieder ein Totem in den Hotbar-Slot, damit Predict Double Hand ein Ersatz-Totem findet.
- **Baut auf:** `java/assist/RefillAssist.java:23` überspringt heute alles Unstapelbare (`expected.getMaxStackSize()<=1`); `PredictDoubleHandAssist` sucht nur in Slots 0–8; Vorrang der Offhand ist schon geregelt (`InventoryAssist.java:17-21`).
- **Neu:** die Bedingung für Totems bei leerem Zielslot lockern; Option `REFILL_TOTEMS` plus Checkbox auf der Refill-Karte.
- **Bridge:** ja, AssistOptions +1.
- **Aufwand/Nutzen:** klein / hoch.
- **Prüfen:** Punkt „Predict Double Hand: Ersatz-Totem“ aus TEST-CHECKLIST mit zwei Pops hintereinander.

**7. Auto XP (Rüstung reparieren)**
- **Für den Spieler:** wirft XP-Flaschen nach unten, solange ein Mending-Rüstungsteil unter X % Haltbarkeit liegt.
- **Baut auf:** Haltbarkeitsrechnung in `EquipmentScores.armorScore`; `HotbarAction.find(Items.EXPERIENCE_BOTTLE)` und `use(…, 90f, spoof, lease)`.
- **Neu:** `AutoXpAssist`, Owner `AUTO_XP`, drei Optionen (an, Schwelle in %, Delay).
- **Achtung:** `find/ready/usable/use` sind package-private in `phantomui.movement`. Entweder public machen oder die Klasse dort ablegen.
- **Bridge:** ja, AssistOptions +3.
- **Aufwand/Nutzen:** klein–mittel / hoch.
- **Variante:** Heil-Splash mit gleichem Code; braucht einen Finder über `POTION_CONTENTS`.

### Visual/ESP

Vorab (klein, sonst wächst das Risiko mit jedem Modul): Die 29 Visual-Optionen sind Magic Numbers (`c[3]`, `c[12]`, `c[17]`, `c[23]` in `java/visual/VisualModulesBridge.java:16-27`; Positionsliste in `src/input/visual_modules.hpp:60-64`), die Primitive `kind` 0–8 ebenso. Benannte Konstanten nach dem Muster `AssistOptions` einführen, dann greift der vorhandene Regex in `tools/client_build.py:25-36`. Das ergänzt Audit-Punkt 17, der nur die Länge prüft.

**8. Hole ESP**
- **Für den Spieler:** markiert sichere 1×1-Löcher (Bedrock/Obsidian ringsum) in der Nähe.
- **Baut auf:** begrenzter Block-Scan wie `DamagePrediction.reason` (Würfel um die Füße); 100-ms-Cache wie `java/visual/ExplosionPreview.java`; Box-Zeichnung aus dem `kind==3`-Zweig in `visual_modules.hpp:162ff`.
- **Neu:** `visual/HoleEsp.java`; `kind` 9 mit Kategorie Bedrock/Obsidian; vier Optionen (an, Reichweite, zwei Typen); Flag im `enabled`-Gate (`visual_modules.hpp:46`).
- **Bridge:** ja, `frame(int[])` 29 → 33 auf beiden Seiten.
- **Aufwand/Nutzen:** mittel / hoch.
- **Prüfen:** entladene Chunks, Y-Grenzen, kein Frame-Einbruch bei Reichweite 16.

**9. Crystal Spots**
- **Für den Spieler:** hebt die besten Kristall-Platzierblöcke hervor und zeigt geschätzten Ziel- und Eigenschaden.
- **Baut auf:** `ExplosionPreview.damage(mc, p, Source)` und die Zielwahl in `append` (derzeit `private`); Label-`kind` 7 wird schon generisch gezeichnet.
- **Neu:** Kandidatensuche in Reichweite (Obsidian/Bedrock mit freiem Block darüber), Top 3 alle 100 ms; ein eigener `kind` für den gefüllten Block, weil `kind==4` am Flag `ProjectilePrediction_HighlightBlock` hängt.
- **Bridge:** ja, Visual-Optionen +3.
- **Aufwand/Nutzen:** mittel–groß / hoch.
- **Risiko:** Die Kosten von `getSeenPercent` pro Kandidat müssen gedeckelt werden.

### HUD/Menü

**10. Combat-HUD**
- **Für den Spieler:** zeigt Totems, Kristalle, Obsidian, Perlen und XP im Inventar, die Haltbarkeit der vier Rüstungsteile und Cooldowns (Perle, Wind Charge).
- **Baut auf:** Snapshot-Muster `coordinationStatus()` (`UtilitySuiteBridge.java:54`, Abruf in `utility_suite.hpp:98-103`); Mutex-Muster `src/input/action_status.hpp`; Fenster und Position des Status-HUD (`status_hud.hpp`, bereits im Profil gespeichert).
- **Neu:** `hudStatus()` liefert ein `int[]`, das in `tick()` etwa alle 250 ms neu gebaut wird; eine Option; ein Abschnitt am Ende von `StatusHud::Draw`.
- **Bridge:** ja, eine Methode und AssistOptions +1.
- **Aufwand/Nutzen:** klein–mittel / hoch.

**11. Pop-Zähler**
- **Für den Spieler:** zählt Totem-Pops pro Gegner und zeigt die Zahl über dem Spieler.
- **Baut auf:** Sound-Listener in `java/visual/SoundEsp.java` und `VisualRules.soundKind`; Label über `VisualFrame.label(7, …)`, auf der C++-Seite ist kein neuer Zeichenpfad nötig.
- **Neu:** Der Listener muss auch ohne Sound ESP laufen; Zuordnung des Pop-Sounds zum nächsten Spieler; Reset bei Tod und Weltwechsel; ein bis zwei Optionen plus Flag im `enabled`-Gate.
- **Bridge:** ja, Visual-Optionen +1–2.
- **Optional später:** Anzeige im Target HUD (`TargetHudBridge.frame` 8 → 9 Werte, Prüfung `count!=8` in `src/menu/target_hud.hpp`).
- **Aufwand/Nutzen:** klein–mittel / mittel–hoch.
- **Prüfen:** Sound-ID im Spiel bestätigen.

## 2. Menü & Bedienung

Schon vorhanden und deshalb nicht vorgeschlagen: Suche über Module und Settings, Favoriten, „Only active“, Collapse, Undo (32 Schritte), Reset pro Modul, Konflikt-Badge auf der Karte, Toggle/Hold, Import-Vorschau.

**M1. Hotkey-Übersicht und Diagnose-Export wieder erreichbar – klein / hoch.**
- Im Arbeitsstand fehlen auf der Settings-Seite die Hotkey-Liste, die Performance-Anzeige und „Export report“ (uncommitteter Diff von `src/menu/pages/settings_page.cpp`).
- `Diagnostics::Export`, `Performance::Reset` und `ActionStatus::Read` haben dadurch keinen Aufrufer mehr.
- `config_page.cpp:129` verweist weiter auf „in Settings“; TEST-CHECKLIST erwartet „Settings / Action coordination“.
- Vorschlag: eine kompakte Tabelle nur der belegten Tasten mit Konfliktmarke, eine Zeile für den letzten Koordinationskonflikt und ein Export-Button. Der alte Code steht im Diff.
- Falls die Entfernung Absicht war: den toten Code und den Verweistext löschen.

**M2. „Alle aus“-Button – klein / mittel–hoch.**
- Schleife über `Registry::GetModules()`, die `enabled` auf false setzt, als Button in der Toolbar neben „Undo“ (`menu.cpp:501-520`).
- Undo greift automatisch, weil `history.hpp` die `enabled`-Werte erfasst.
- Friends ausnehmen, denn dessen `enabled` ist der Schutzschalter.
- Die End-Taste entlädt die ganze DLL (`hooks.cpp:35`) und ist dafür kein Ersatz.

**M3. Hilfetext pro Setting – klein / mittel.**
- Tooltips entstehen heute per Teilstring-Suche im Label (`menu.cpp:773-781`) und sind entsprechend generisch.
- Ein optionales Feld `const char* help = nullptr` am Ende von `ModuleSetting` (`src/utils/config/config.hpp:18-29`) lässt alle bestehenden Initialisierer gültig.
- Zuerst die erklärungsbedürftigen Settings befüllen: Hotbar spoofing, Predict-Reserve, Fake-Lag-Burst.

**M4. Defaults entschärfen – klein / mittel.**
- `Reach_range=6` ist das Slider-Maximum (3..6); `SafeAnchor_Logs=true` schreibt per Default in die Konsole (`src/modules/settings.hpp`).
- Vorschlag: 3 bzw. false.
- Nebenwirkung: „Reset“ und Profile ohne diesen Schlüssel fallen auf die neuen Werte, weil `factory_values` daraus entsteht (`module_registry.cpp:257-266`).

**M5. Profil-Schnellwechsel per Taste – mittel / mittel.**
- Baut auf dem Load-Pfad aus `config_page.cpp` (`ResetModule`-Schleife plus `Config::Load`) und `ProcessHotkeys` auf.
- Die Bindung muss außerhalb der Profile liegen. Am besten erst zusammen mit der Datei aus Audit-Punkt 18 umsetzen.

## 3. Stabilität & Wartung

**S1. Hängende Java-Exception pro Modul isolieren – klein / hoch.**
- `ModuleManager::UpdateModules` (`src/modules/ModuleManager.cpp:37-41`) ruft alle Module nacheinander auf demselben Worker-`JNIEnv` auf.
- Jedes Bridge-Modul steigt bei `env->ExceptionCheck()` still aus (z. B. `utility_suite.hpp:62`, `safe_walk_module.cpp:40`, `player_targeting.hpp:18`), ohne die Exception zu löschen.
- Eine einzige liegengebliebene Exception legt damit alle Worker-Module lahm, ohne dass der Verursacher sichtbar wird.
- Fix: nach jedem `module->Update()` prüfen, den Modulnamen gedrosselt loggen, `ExceptionDescribe` und `ExceptionClear` aufrufen (~6 Zeilen).
- Prüfen: Mit einer alten JAR arbeiten die übrigen Module weiter, und das Log nennt das Modul.

**S2. Modulstatus sichtbar machen – mittel / hoch.**
- Ein fehlgeschlagenes `Load` (JAR alt, Methode fehlt) zeigt sich nur als `printf` mit 5-s-Retry; die Karte bleibt „an“ und tut nichts.
- Nur Player ESP zeichnet einen Fehlertext (`src/utils/esp/player_esp.hpp:28-31`).
- Vorschlag: eine kleine Statusablage (Modulname → Text, Muster `action_status.hpp`), gesetzt in den `Load`-Fehlerpfaden, angezeigt auf der Karte und im Diagnose-Report.
- Am billigsten zusammen mit dem `BridgeClass`-Helfer aus Audit-Punkt 3.

**S3. Tests für die JNI-freien Bausteine – klein–mittel / mittel.**
- Es gibt nur `tools/test_client_build.py` und die manuelle Checkliste.
- Reine Logik liegt bereits getrennt: `input/rotation/player_selection.hpp`, `targeting.hpp`, `ownership.hpp`, `input/hotkey_state.hpp`, `action_key.hpp`, `loot_beacon_rules.hpp`, `menu/navigation_layout.hpp`, `utils/config/share_code.hpp`; in Java `AimAssistMath`, `ExplosionMath`, `VisualRules`, `ProjectilePhysics`.
- Vorschlag: eine `tools/native_tests.cpp` mit `assert`, im Build-Schritt per `cl` vor MSBuild kompiliert und ausgeführt (`find_vcvars` ist vorhanden, `client_build.py:111`).

**S4. Java-Logs einheitlich – klein–mittel / mittel.**
- In `java/` stehen 36 `System.out.println`. Nur `AssistLog` und `SafeAnchorBridge.drainLogs` gehen gezielt in die DLL-Konsole.
- Vorschlag: `AssistLog` in ein gemeinsames Paket legen, die übrigen Bridges darüber loggen und über den bestehenden `drainLogs`-Abruf leeren (`utility_suite.hpp:118-126`).
- Keine Signaturänderung, aber ein gemeinsamer Build.

**S5. Arbeitsstand sichern – klein / hoch.**
- Das C++-Repo hat 90 Einträge in `git status --short`, darunter ungetrackte Kern-Dateien wie `input/friends.hpp`, `input/visual_modules.hpp`, `menu/target_hud.hpp`. Das Java-Repo hat 24.
- `CHECKPOINT.md` steht auf 2026-10-01.
- Die umgesetzten Audit-Punkte 5–7 und 11–15 sind damit nicht als getesteter Stand festgehalten.
- Vorschlag (nur auf Auftrag): TEST-CHECKLIST durchgehen, beide Repos als Paar committen, CHECKPOINT aktualisieren.
- Auffällig: `src/dependencies/images/bytearray.h` ist als geändert markiert (laut CLAUDE.md „nicht anfassen“; nicht geöffnet).

## Top 5 zum Umsetzen

| # | Name | Aufwand | Nutzen | Bridge |
|---|---|---|---|---|
| 1 | S1 Exception-Isolierung im Worker | klein | hoch | nein |
| 2 | 6 Totem-Slot nachlegen | klein | hoch | ja (1 Option) |
| 3 | 10 Combat-HUD | klein–mittel | hoch | ja (1 Methode, 1 Option) |
| 4 | 1 Trigger Bot | klein–mittel | hoch | ja (3 Optionen, 1 Owner) |
| 5 | 8 Hole ESP | mittel | hoch | ja (Visual 29 → 33) |

Reihenfolge und Prüfung:
1. **S1 allein** (nur C++). Build; mit absichtlich alter JAR starten; die übrigen Module laufen weiter.
2. **Punkte 6, 10 und 1 als ein Bridge-Schritt** (AssistOptions 26 → 31). `Build-Client.ps1 -Action build`; `check_protocol` meldet 31 Felder; Minecraft neu starten; TEST-CHECKLIST zu Refill, Predict Double Hand, Modulkonflikten und Status-HUD.
3. **Visual-Optionen benennen, dann Hole ESP.** Build; TEST-CHECKLIST „Neue visuelle Module“ (OpenGL und Vulkan, Deaktivieren, Unload).

## Nicht geprüft

- Kein Build, kein Spieltest. Annahmen zu Minecraft 26.2 (Sound-ID des Totem-Pops, Fallschaden-Regel der Wind Charge, Platzierregeln für Kristalle, Cooldown-API) stammen aus dem Code bzw. allgemeiner Kenntnis und sind nicht verifiziert.
- Ob die Entfernung von Hotkey-Liste und Diagnose aus `settings_page.cpp` Absicht ist (M1).
- Ob heute tatsächlich eine Exception hängen bleibt (S1 ist ein Schutznetz), und ob `System.out` der Bridges in der DLL-Konsole ankommt (S4).
- C++ nicht geöffnet: `menu/widgets.cpp`, `login.cpp`, `theme_page.cpp`, `friends_page.cpp`, `seedcracker_page.cpp`, `target_hud_draw.hpp`, `key_mapping.cpp`, `rotation/controller.hpp`, `silent_aim_module.cpp`, `Reach.cpp`, `CwCrystal.cpp`, `HitCrystal.cpp`, `AutoTotem.hpp`, die Hook-Backends. `menu.cpp` nur per grep und die Zeilen 76-84, 250-265, 760-790.
- Java nur teilweise: `SafeAnchorBridge` (Signaturen), `FakeLagBridge` (Kopf). Nicht geöffnet: `AimAssistBridge`, `HitEffects`, `ProjectilePrediction`, `EchoTrailBridge`, `PlayerSkinBridge`, `oresim/*`, `seedcracker/*`.
- Zeilennummern in `diagnostics.hpp`, `status_hud.hpp` (Ende von `Draw`) und `HotbarAction.java` sind nur über Funktionsnamen angegeben.
- Serverseitige Erkennung der Aktionsmodule (Trigger Bot, Surround, Fall Clutch) nicht bewertet; graphify nicht benutzt.
