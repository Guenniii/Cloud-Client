# Ideen Runde 2: neue Module und Verbesserungen

Stand 2026-11-16. Nichts geändert, kein Build, nichts im Spiel getestet. Grundlage: `docs/audit.md`, `docs/ideas.md`, `git log`, gezieltes Lesen der genannten Stellen.
Pfade: `src/` = `UniversalHookX/src/`, `java/` = `../SeedcrackerX-master/src/main/java/phantomui/`.

## Stand der alten Punkte

Seit den beiden Dokumenten gab es drei C++-Commits (`3b5b5bf`, `3fc1982`, `2473def`). Das Java-Repo steht weiter auf `70d9789`.

### audit.md

| # | Punkt | Status | Beleg |
|---|---|---|---|
| 1 | JAR-Pfad 20-mal hart kodiert | offen | weiter in 20 Dateien, z. B. `src/input/utility_suite.hpp:36`, `src/input/rotation/antibot.hpp:24`; kein `BridgeJarPath` |
| 2 | Modul-Metadaten in Listen | offen | `MODULE_COUNT = 35` fest (`src/menu/module_registry.cpp:13`); feste HUD-Liste (`src/menu/status_hud.hpp:15-24`) |
| 3 | Bridge-Boilerplate | offen | kein `BridgeClass`, `GameplayActive`, `CameraHeader` |
| 4 | Altlasten, toter Code, Encoding | offen | `LoginRequest` (`src/auth/Authclient.cpp:11`), No-op-Block (`src/hooks/hooks.cpp:135-153`), `AutoTotem.hpp` weiter ISO-8859 |
| 5 | Zielsuche/AntiBot-Cache | teilweise | IDs gecacht (`player_targeting.hpp:8-47`), AntiBot als GlobalRef (`antibot.hpp:11-41`); 50-ms-Drossel fehlt (`aim_assist_module.cpp:28`) |
| 6 | `Publish`-Drossel | teilweise | einmal statt fünfmal pro Durchlauf (`utility_suite.hpp:107-110`), aktiv aber weiter alle 4 ms |
| 7 | `Sleep` auf dem Worker | teilweise | HitCrystal hat eigenen Thread (`HitCrystal.cpp:22-26, 41-64`); AutoTotem schläft weiter (`AutoTotem.hpp:62, 66`) |
| 8 | VisualFrame-Allokation | offen | `java/visual/VisualFrame.java:10, 29`; `src/input/visual_modules.hpp:65, 71-80` |
| 9 | Skin-ESP-Allokationen | offen | `skin_geometry.hpp:53` weiter `stable_sort` (nur per grep geprüft) |
| 10 | Namens-Lookups auf heißen Pfaden | offen | `game_screen.hpp:13, 17`; `utils/SilentAim.cpp:46-51`; `game_runtime.hpp:77`. Reach nicht im Detail geprüft |
| 11 | Null-ID-Guards | erledigt | `Sprint.hpp:15-19`; `AutoTotem.hpp:80-90` (`Ready`) |
| 12 | HitCrystal-Guards | erledigt | `HitCrystal.cpp:28-34` (`Allowed`); Entity-Iteration nicht erneut geprüft |
| 13 | Doppelte Auto-Crystal-Karte | erledigt | nur noch `module_registry.cpp:205`; Selbst-Checkboxen `:213, :217, :220, :224, :227` stehen noch |
| 14 | Profil laden / Rückfrage | erledigt | `config_page.cpp:63` (`ResetModule`), `:46, :68` („Sure?“) |
| 15 | Seed-Handoff-Race | erledigt | `Apply` läuft jetzt im Render-Pfad (`utils/oresim/oresim.hpp:73`) |
| 16 | Login über Port 80, HWID | offen | `Authclient.cpp:76, 78`; `HWID.cpp:9, 26` |
| 17 | Bridge-Handshake, `check_protocol` | offen | kein `protocolVersion`; `tools/client_build.py:25-36` unverändert |
| 18 | Letztes Profil automatisch laden | offen | `src/menu/README.md:42`: „There is no autoload“ |
| 19 | Performance pro Feature | offen | `performance.hpp:8` kennt nur Client, OreSim, PlayerESP |
| 20 | Player ESP: Name, Leben, Tracer | offen | `java/esp/PlayerEspBridge.java:32-34` weiter 7 Werte |

### ideas.md

| # | Punkt | Status | Beleg |
|---|---|---|---|
| 1 | Trigger Bot | erledigt | `java/assist/TriggerAssist.java`; `module_registry.cpp:139-142`; Optionen 27–29 |
| 2 | Cooldown Guard | offen | Klick-Kette unverändert (`hooks.cpp:92`) |
| 3 | Auto Surround | offen | – |
| 4 | Elytra Swap | offen | – |
| 5 | Fall Clutch | offen | – |
| 6 | Totem-Slot nachlegen | erledigt | `java/assist/RefillAssist.java:24`; `module_registry.cpp:115`; Option 26 |
| 7 | Auto XP | offen | – |
| – | Visual-Optionen benennen | offen | `java/visual/VisualModulesBridge.java:19-28` weiter `c[3]`, `c[12]` usw. |
| 8 | Hole ESP | offen | – |
| 9 | Crystal Spots | offen | – |
| 10 | Combat-HUD | offen | – |
| 11 | Pop-Zähler | offen | – |
| M1 | Hotkey-Liste, Diagnose-Export | offen | `Diagnostics::Export` (`diagnostics.hpp:65`) hat keinen Aufrufer |
| M2 | „Alle aus“ | offen | – |
| M3 | Hilfetext pro Setting | offen | `utils/config/config.hpp:18-28` ohne `help` |
| M4 | Defaults entschärfen | offen | `settings.hpp`: `Reach_range = 6`, `SafeAnchor_Logs = true` |
| M5 | Profil-Schnellwechsel | offen | – |
| S1 | Exception-Isolierung im Worker | erledigt | `ModuleManager.cpp:42-60` |
| S2 | Modulstatus sichtbar | offen | – |
| S3 | Tests für JNI-freie Bausteine | offen | `tools/` ohne `native_tests.cpp` |
| S4 | Java-Logs einheitlich | offen | 37 `System.out.print` in `java/` |
| S5 | Arbeitsstand sichern | teilweise | C++ committet; Java-Repo hat 27 offene Einträge; `CHECKPOINT.md` weiter 2026-10-01 |

**Zu S5:** Das Java-Repo enthält im Commit `COUNT=26`, im Arbeitsverzeichnis 30. Ganze Pakete sind ungetrackt: `bootstrap/`, `compat/`, `hud/`, `visual/`, dazu `TriggerAssist.java`, `FriendsBridge.java` und `EchoTrailBridge.java`. Aus Git allein lässt sich das DLL/JAR-Paar nicht mehr bauen.

## 1. Neue Module

Acht Vorschläge, je Gruppe nach Nutzen/Aufwand sortiert. Alle brauchen die Pflichtstellen aus `docs/ideas.md` (Karte, `MODULE_COUNT`, Beschreibung, Icon, Labels, Status-HUD).

### Combat

**1. Crystal Optimizer**
- **Für den Spieler:** Ein getroffener Kristall verschwindet sofort clientseitig, statt auf das Server-Paket zu warten. Der nächste Kristall lässt sich ohne Ping-Verzögerung setzen.
- **Baut auf:** Den Observer für ausgehende Angriffe gibt es schon zweimal, je mit eigenem Netty-Handler (`java/hud/TargetHudBridge.java:26-36`, `java/assist/HitEffects.java:86-98`). Auto Crystal greift bei `src/modules/combat/CwCrystal.cpp:324` an.
- **Neu:** Beide Observer zu einem zusammenlegen und einen dritten Abnehmer ergänzen: Angriff auf `EndCrystal` in Vanilla-Reichweite → per `mc.execute` clientseitig entfernen. Dazu Karte und Flag.
- **Bridge:** ja (neue Klasse, kein Layout).
- **Aufwand/Nutzen:** mittel / hoch.
- **Risiko:** Lehnt der Server den Treffer ab, bleibt ein unsichtbarer Kristall. Die Entfernen-API in 26.2 ist nicht verifiziert.
- **Warum:** Kein zusätzliches Paket, wirkt bei jedem Ping, auch bei manuellen Treffern.

**2. Auto Cobweb**
- **Für den Spieler:** Setzt per Aktionstaste ein Spinnennetz in den Fußblock des anvisierten Gegners (Eröffnung für Mace und Schwert).
- **Baut auf:** Zielwahl `Rotation::FindPlayerTarget` (`src/input/rotation/player_targeting.hpp:15`) und das Publish-Muster von Auto Mace (`java/combat/AutoMaceBridge.java:43-48`). Platzier-Helfer in `java/combat/SafeAnchorBridge.java` (`free` :138, `visibleHit` :142, `placement` :152, `send` :166, alle `private`). `HotbarAction.interactBlock` (`java/movement/HotbarAction.java:169`).
- **Neu:** Helfer package-private machen, `combat/CobwebBridge.fire(id, spoof)`, ein Owner, C++-Modul nach `src/modules/movement/short_wind_charge_module.cpp`.
- **Bridge:** ja (neue Klasse).
- **Aufwand/Nutzen:** mittel / mittel–hoch.
- **Warum:** Lohnt nur auf Mace-/Schwert-Servern. Teilt die Helfer mit Auto Surround (ideas.md Nr. 3); nicht beide parallel bauen.

### Movement

**3. Key Pearl**
- **Für den Spieler:** Wirft per Taste eine Perle in Blickrichtung, ohne sichtbaren Slotwechsel.
- **Baut auf:** Der erste Teil von `AutoPearlCatchBridge.fire` ist genau dieser Wurf, nur mit Pitch -90 (`java/movement/AutoPearlCatchBridge.java:17-42`). C++-Vorlage `short_wind_charge_module.cpp:20-44`. Aktionstaste in `module_registry.cpp:367-374`.
- **Neu:** Eine Methode `pearl(boolean spoof)` (~15 Zeilen, aktueller Pitch, kein Folgeschritt), Karte, Request-Flag, dritter Aktionstasten-Fall (siehe Abschnitt 3, Punkt 2).
- **Bridge:** ja (eine Methode).
- **Aufwand/Nutzen:** klein / hoch.
- **Warum:** Eine Checkbox auf der Pearl-Catch-Karte reicht nicht, weil eine Karte nur einen Hotkey hat (`config.hpp:32`).

**4. Inventory Move**
- **Für den Spieler:** Laufen und Springen bei offenem Inventar oder Container (nicht im Chat).
- **Baut auf:** Der UtilitySuite-Tick läuft auch bei offenem Screen (`java/assist/UtilitySuiteBridge.java:34-44`). Auto Armor arbeitet nur bei offenem Inventar (`java/assist/AutoArmorAssist.java:16-18, 26`); heute steht man dabei still.
- **Neu:** `assist/InventoryMoveAssist` (~15 Zeilen): Bei einem Container-Screen die Bewegungs-KeyMappings auf den physischen Tastenzustand setzen. AssistOptions +1, eine `AssistModule`-Instanz, Karte.
- **Bridge:** ja (+1 Option).
- **Aufwand/Nutzen:** klein / mittel–hoch.
- **Risiko:** Inventarklicks in Bewegung sind ein klassischer Anticheat-Check. Die Tastenzustand-API in 26.2 ist nicht verifiziert.
- **Warum:** Macht Auto Armor im Kampf erst brauchbar.

### Utility

**5. Hotbar-Layout (Kit Sort)**
- **Für den Spieler:** Stellt nach Respawn oder Kit die gespeicherte Hotbar-Anordnung wieder her.
- **Baut auf:** Refill merkt sich schon pro Slot eine Vorlage (`java/assist/RefillAssist.java:10, 17-20`). Es verliert sie aber bei jedem Screen (`:15`) und bei Spieler-/Weltwechsel (`java/assist/InventoryAssist.java:13`), also auch beim Tod. Transfer über `InventoryTransactions.transfer` (`java/assist/InventoryTransactions.java:27-43`).
- **Neu:** Aktion „Layout speichern“ friert neun Vorlagen ein, die den Respawn überleben. Sortierschritt: ein Zug pro Refill-Delay. Persistenz erst im zweiten Schritt (Datei neben der JAR, atomar).
- **Bridge:** ja (+1 Option, eine Methode `saveLayout()`).
- **Aufwand/Nutzen:** mittel / hoch auf Kit-Servern, niedrig im SMP.
- **Warum:** Nur bauen, wenn auf Kit-Servern gespielt wird.

**6. Fullbright**
- **Für den Spieler:** Volle Helligkeit ohne Fackeln oder Resource Pack.
- **Baut auf:** UtilitySuite-Tick. Größter Nutzen zusammen mit OreSim (`src/utils/oresim/oresim.hpp`).
- **Neu:** Pro Tick einen clientseitigen Night-Vision-Effekt ohne Partikel setzen und in `clear()` wieder entfernen. Einen echten Trank dabei nicht entfernen (merken, ob der Effekt vorher da war). AssistOptions +1.
- **Bridge:** ja (+1 Option).
- **Aufwand/Nutzen:** klein / mittel.
- **Warum:** Reiner Komfort, aber etwa zehn Zeilen und kein Paket.

### Visuals

**7. Totem-Warner**
- **Für den Spieler:** Label „NO TOTEM“ über Gegnern ohne Totem in Off- oder Mainhand. Das Kill-Fenster wird sichtbar.
- **Baut auf:** Spieler-Schleife und Filter aus `java/esp/PlayerEspBridge.java:25-34`. Label-Primitive `VisualFrame.label(7, …)` (`java/visual/VisualFrame.java:13-16`), das `src/input/visual_modules.hpp:123-129` schon generisch zeichnet.
- **Neu:** `visual/TotemWarner.append` (~12 Zeilen), zwei Optionen (an, Reichweite), Flag im Gate (`visual_modules.hpp:46`), Options-Array `:60-64` und Längenprüfung `VisualModulesBridge.java:16` von 29 auf 31.
- **Bridge:** ja (Visual 29 → 31).
- **Aufwand/Nutzen:** klein / hoch.
- **Warum:** Kein neuer Zeichenpfad in C++. Zusammen mit dem Pop-Zähler (ideas.md Nr. 11) in einem Visual-Schritt bauen.

**8. Death Marker**
- **Für den Spieler:** Label mit Distanz und Restzeit am letzten Todespunkt.
- **Baut auf:** Label `kind` 7. Die Lichtsäule (`visual_modules.hpp:89-104`) hängt an `kind` 5 und den Loot-Beacon-Regeln (`:112-117`).
- **Neu:** Die Todeserkennung muss vor dem Screen-Guard laufen, denn `VisualModulesBridge.java:16` bricht bei offenem Screen ab und der Death-Screen ist einer. Der Zustand muss `reset()` überleben (`:21, :32`); Position und Dimension merken. Kleinste Version nur mit Label, ohne C++-Zeichencode.
- **Bridge:** ja (+1 Visual-Option).
- **Aufwand/Nutzen:** klein / mittel–hoch.
- **Warum:** Koordinaten merkt sich im Kampf niemand, und Items despawnen nach fünf Minuten.

**Verworfen (Bedarf zu schwach):**
- Sprint Reset / W-Tap: Vanilla setzt den Sprint nach dem Treffer selbst zurück.
- Jump Reset: Wirkung in 26.2 unbelegt, und perfektes Timing ist ein Erkennungsmuster.
- Logout Spots, Mob ESP, Zoom, Info-HUD (Ping/TPS): kein klarer Bedarf.
- Freecam: groß ohne Mixins.
- Crash-Handler per VEH: Die JVM nutzt Access Violations intern.

## 2. Bestehende Module verbessern

**1. Auto Crystal: verwaister `Swing` bei gehaltenem Rechtsklick (Bug) – klein / hoch**
- `src/modules/combat/CwCrystal.cpp:354`: `p_mc->Swing(env,player);` ist die einzige nicht auskommentierte Zeile im auskommentierten Block `:334-365`.
- Sie läuft damit in jedem Schleifendurchlauf (5 ms, `:248`), sobald Rechtsklick gehalten wird und irgendwo im Inventar ein Kristall liegt. Das gilt auch beim Essen, Blocken und Bauen.
- Vorschlag: Zeile 354 löschen, am besten den ganzen toten Block `:333-365`.
- Warum: Laut `TriggerAssist.java:33-34` hat Swing-Verhalten schon einmal Vulcan-Flags ausgelöst.

**2. Auto Crystal: keine Guards, Angriff alle 5 ms – klein / hoch**
- `CwCrystal.cpp:274` prüft nur `GetAsyncKeyState(VK_RBUTTON)`. Es fehlt jede Prüfung auf `Menu_Enabled`, Fensterfokus und offenen Screen. Der Rechtsklick zählt also auch im ImGui-Menü oder in einem anderen Fenster.
- Vorschlag: denselben Guard wie `HitCrystal::Allowed` nutzen (`HitCrystal.cpp:28-34`).
- `:321-322`: Bei `CW_Delay = 0` (Default) wird derselbe Kristall in jedem Durchlauf angegriffen, bis der Server ihn entfernt. Das sind bis zu 200 Angriffe pro Sekunde.
- Vorschlag: zuletzt angegriffene Entity-ID merken und mindestens 50 ms sperren.
- `:281-297` scannt 36 Slots per JNI in jedem Durchlauf. Die Haupthand zu prüfen reicht (`m_get_main_hand_item`, `utils/sdk/CMinecraft.h:43`).
- `cw.cpp:15` setzt `SetRightClickDelay(0)` einmalig beim Anlegen; das ist wirkungslos und kann weg.

**3. AutoTotem nach Java verlagern, `AutoTotem.hpp` löschen – mittel / hoch**
- `src/modules/combat/AutoTotem.hpp:60` öffnet ohne Screen- oder Fokus-Prüfung den Inventar-Screen. `:67` ruft `setScreen(null)` (`:202`) und schließt damit auch einen offenen Chat oder eine Kiste.
- `:62, :66` schlafen bis zu zweimal 500 ms auf dem gemeinsamen Worker. Alles läuft vom Worker-Thread.
- Java hat die Bausteine schon: `InventoryAssist.java:16-21` wartet heute auf die „legacy“-Sequenz, `InventoryTransactions.transfer` (`:27-43`) ist cursor-sicher, und Refill nutzt es ohne offenes Inventar (`RefillAssist.java:15, 28`).
- Vorschlag: ~15 Zeilen in `InventoryAssist` (nach Pop-Delay `transfer(…, 45, …)`). Option `AUTO_TOTEM` (Index 20) existiert, dazu +1 für den Pop-Delay. 255 Zeilen C++ entfallen.
- Erledigt nebenbei die Reste von Audit 4, 7 und 11.
- Prüfen: zwei Pops hintereinander; Chat offen während eines Pops; ob `Config::Load` die verwaisten Schlüssel `Inventar-Delay` und `Totem-Suchintervall` ignoriert.

**4. Zielpunkt: nächster Punkt der Hitbox statt Augenhöhe – klein / mittel–hoch**
- `src/input/rotation/player_targeting.hpp:30` bindet `ids.y` an `getEyeY`. `:80-84` misst Reichweite und Winkel damit von Auge zu Auge.
- `java/combat/AimAssistBridge.java:83` zielt ebenfalls auf `getEyeY`; `AutoMaceBridge.java:76-79` misst gleich.
- Folge: Ziele über oder unter dem Spieler fallen am Reichweitenrand heraus, obwohl sie treffbar sind. Aim Assist zieht immer auf Kopfhöhe.
- Vorschlag: Die eigene Augenhöhe auf den Bereich Füße bis Kopf des Ziels klemmen (ein zusätzlicher `getY`-Aufruf pro Spieler), in Java dieselbe Klemmung. Keine Signaturänderung, aber gemeinsamer Build.

**5. Trigger Bot: auf Crit warten, Reaktionszeit streuen – klein / mittel–hoch**
- `java/assist/TriggerAssist.java:27` schlägt zu, sobald der Cooldown 0,9 erreicht. Im Sprungaufstieg verschenkt das den Crit.
- Die Reaktionszeit ist konstant (`:25, :27`).
- Vorschlag: Checkbox „Auf Crit warten“ (nicht am Boden und Aufwärtsbewegung → warten), AssistOptions +1. Streuung ohne Option: bei neuem Ziel `seenAt` um zufällig 0 bis Delay/2 verschieben.
- Die Karte hat nur zwei Settings (`module_registry.cpp:139-142`).

**6. AutoSprint: Sprint-Taste halten statt `setSprinting(true)` – klein / mittel**
- `src/modules/movement/Sprint.hpp:39-40` setzt vom Worker bis zu 200-mal pro Sekunde `setSprinting(true)`. Geprüft werden nur Vorwärtsimpuls und Sneaken, nicht Hunger, Item-Benutzung oder Blindheit.
- Zwischen Vanilla-Check und Paketversand kann so ein Sprint-Start bei Item-Benutzung zum Server gehen. Das ist möglich, aber nicht gemessen.
- Vorschlag: nur `options.keySprint.setDown(true)` setzen; Vanilla prüft dann alle Bedingungen.
- Kritisch: Vanilla hat „Sprinten: Umschalten“. Das Modul ist fast überflüssig; Löschen wäre die kleinste Lösung.

**7. FastPlace: Item-Filter – klein / mittel**
- `src/modules/utility/fast_place.cpp:13-17` setzt den Delay für jedes Item auf 0, auch für Blöcke.
- Vorschlag: Checkbox „Nur Kristalle/XP“ über die Haupthand. Der Item-Vergleich existiert schon in `CwCrystal.cpp:293`.
- Warum: Block-Spam ohne Delay ist das, was Anticheats als FastPlace erkennen; Kristalle und XP sind der eigentliche Zweck.

**8. Silent Aim: Waffenwahl und Logs – klein / mittel**
- `src/modules/combat/silent_aim_module.cpp:325` lässt nur Schwerter zu (`IsHoldingSword`, `:228`). Aim Assist und Trigger Bot kennen Axt und Mace (`TriggerAssist.java:28`).
- `:348, :420, :434` schreiben ohne Schalter in die Konsole. Andere Module haben „Console-Diagnose“.
- Die Karte hat nur Reichweite und FOV (`module_registry.cpp:201-204`).
- Vorschlag: Checkbox „Auch Axt/Mace“ und Checkbox „Console-Diagnose“. Den Kartennamen nicht ändern, er ist der Profilschlüssel.

**Zurückgestellt:**
- Storage ESP unterscheidet Trapped Chests nicht und kennt keine Spawner (`java/visual/StorageEsp.java:16-22`). Das braucht eine Layout-Änderung, erst nach den benannten Visual-Konstanten.
- HitCrystal als eigene Karte mit Hotkey (heute Checkbox, `module_registry.cpp:207`).

## 3. Client allgemein

**1. Rückmeldung beim Hotkey-Toggle (Toasts) – klein / hoch**
- `module_registry.cpp:375-378` schaltet Module still um. Bei geschlossenem Menü und ausgeschaltetem Status-HUD sieht man nicht, ob die Taste gegriffen hat.
- Vorschlag: Ringpuffer der letzten drei Wechsel („Trigger Bot an“), 1,5 s im HUD-Pfad gezeichnet (`menu.cpp:773`), ein Schalter in Settings. Keine Bridge.
- Warum: Ein versehentlich aktives Modul ist auf Servern mit Anticheat das teuerste Bedienproblem.

**2. Aktionstasten datengetrieben – klein / mittel–hoch**
- `module_registry.cpp:367-369` kennt genau zwei Aktionsmodule per Zeigervergleich.
- Key Pearl, Auto Cobweb, Elytra Swap und Surround brauchen jeweils einen weiteren Fall.
- Vorschlag: Feld `std::atomic<bool>* action_request = nullptr` am Ende von `ModuleData` (`config.hpp:30-39`). Bestehende Initialisierer bleiben gültig, der Sonderfall wird zu `if (module.action_request)`.

**3. Risiko-Badge pro Modul – klein / mittel**
- Reach (Default 6), FastPlace, Fake Lag, Silent Aim und Inventory Move sind serverseitig auffällig. Das Menü zeigt das nirgends.
- Vorschlag: Name→Risiko-Tabelle neben `Description()` (`module_registry.cpp:292-333`) und ein Badge in der Orbit-Karte nach dem Muster des BETA-Badges.
- Kritisch: Die Einstufung ist eine Einschätzung und muss gepflegt werden. Nur ein Badge, keine Sperrlogik.

**4. Status-HUD im Orbit-Stil – klein–mittel / mittel–hoch**
- `src/menu/status_hud.hpp:37-55` ist ein Standard-ImGui-Fenster mit dem Titel „FUSION / STATUS“ (`:51`) und fester Breite von 220 px. Orbit ist Default, das In-Game-HUD sieht aber wie der alte Client aus.
- Vorschlag: Liste aus `Registry::GetModules()` (erledigt Audit 2 für das HUD), mit Orbit-Fonts und Palette gezeichnet, nach Textbreite sortiert.

**5. Server-Profile – mittel / mittel–hoch**
- Unterschiedliche Anticheats brauchen unterschiedliche Profile. Heute lädt man von Hand.
- Vorschlag: Datei `PhantomUI/servers.cfg` (`host=profil`, atomar geschrieben). Beim Join eines bekannten Hosts den vorhandenen Ladepfad nutzen (`config_page.cpp:63` plus `Config::Load`) und einen Toast zeigen.
- Braucht eine Bridge-Methode `serverAddress()` und baut auf der Datei aus Audit 18 auf.
- Kritisch: erst nach Punkt 1, sonst wechselt das Profil unbemerkt.

**6. Klassik-Menü und Click-GUI v1 einfrieren, später entfernen – klein / mittel**
- Die Seiten sind schon geteilt (`src/menu/module_view.hpp:61-69`). Die Modulkarten gibt es aber dreimal: `menu.cpp:138ff` (~600 Zeilen), `clickgui.cpp` (679), `orbit.cpp`.
- Jeder neue Setting-Typ oder jedes Badge (Punkt 3) kostet dreifach und drei Abschnitte in der TEST-CHECKLIST.
- Vorschlag: ab jetzt neue UI nur in Orbit. Wenn Orbit sich bewährt, `kMenus[]` (`menu.cpp:785`) auf einen Eintrag kürzen und `ui.menu_style` 0/1 auf Orbit abbilden.
- Entscheidung des Nutzers nötig. Die Menüwahl wurde gerade erst gebaut.

## Top 5

| # | Name | Typ | Aufwand | Nutzen | Bridge |
|---|---|---|---|---|---|
| 1 | Auto Crystal: Swing-Bug, Guards, Angriffsrate (2.1 + 2.2) | Verbesserung | klein | hoch | nein |
| 2 | Hotkey-Toasts (3.1) | Client | klein | hoch | nein |
| 3 | Totem-Warner (1.7) | Modul | klein | hoch | ja (Visual 29 → 31) |
| 4 | Key Pearl (1.3) mit Aktionstasten-Feld (3.2) | Modul | klein | hoch | ja (eine Methode) |
| 5 | AutoTotem nach Java (2.3) | Verbesserung | mittel | hoch | ja (AssistOptions +1) |

Reihenfolge und Prüfung:
1. **Nur C++ (2.1, 2.2, 3.1, 3.2):** Build. Prüfen: Rechtsklick halten mit Kristall im Inventar → kein Dauer-Swing; Rechtsklick im Menü oder in anderem Fenster → kein Angriff; TEST-CHECKLIST „Modulkonflikte nach dem Checkpoint“ und die Hotkey-Punkte.
2. **Ein UtilitySuite-Schritt (2.3, 2.5, 1.4, 1.6, optional 1.5):** AssistOptions 30 → 33–35. `check_protocol` meldet die neue Zahl. Minecraft neu starten. TEST-CHECKLIST zu Refill, Predict Double Hand und Auto Armor.
3. **Ein Visual-Schritt (Konstanten benennen, dann 1.7 und 1.8, optional Pop-Zähler):** TEST-CHECKLIST „Neue visuelle Module“ auf OpenGL und Vulkan.
4. **Vor allem anderen:** Java-Repo committen (S5) und Audit 1 (JAR-Pfad) erledigen. Sonst wächst der ungesicherte, nur auf diesem Rechner lauffähige Stand mit jedem Bridge-Schritt.

## Nicht geprüft

- Kein Build, kein Spieltest. Alle Annahmen zu Minecraft 26.2 und zum Anticheat-Verhalten sind unbestätigt: Entity-Entfernen, Night-Vision-Effekt, Tastenzustand-API, Sprint-Reset in Vanilla, Slot-45-Klick ohne offenes Inventar.
- „Vulcan No-Swing“ existiert nicht als Modul. Gefunden wurde nur der Trigger-Bot-Fix (`TriggerAssist.java:33-36`).
- Nicht geöffnet: `orbit.cpp` und `clickgui.cpp` (nur grep), `login.cpp` und `resources.*` (uncommitteter Arbeitsstand), `Reach.cpp`, `FakeLagBridge` ab Zeile 70, `HitEffects`, `SafeAnchorBridge` (nur Signaturen), `seedcracker/*`, `oresim/*`.
- Ob `Config::Load` unbekannte Schlüssel ignoriert (2.3) und ob `CMinecraft::Swing` ein Paket sendet (2.1; aus dem Namen und der Verwendung in `silent_aim_module.cpp:220` abgeleitet).
- „Login merken“ wurde bewusst nicht vorgeschlagen (`docs/login-orbit-design.md`, Entscheidung B).
