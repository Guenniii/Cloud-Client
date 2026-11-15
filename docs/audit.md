# Audit: UniversalHookX (C++-DLL) + SeedcrackerX-master (Java-Bridge `phantomui`)

Stand 2026-11-11. Nur gelesen, nichts geändert, kein Build, nichts gemessen. Performance-Aussagen sind statische Schätzungen aus dem Code.
Pfade: `src/` = `UniversalHookX/src/`, `java/` = `SeedcrackerX-master/src/main/java/phantomui/`.

**Aufrufpfade (geprüft)**
- Worker-Thread: `src/Base.cpp:18-21` ruft `ModuleManager::UpdateModules()` mit `sleep_for(5ms)` in Schleife (nominell 200 Hz, real ~64–200 Hz). Jedes aktive `RuntimeModule::Tick` läuft dort (`src/modules/RuntimeModule.cpp:5-9`).
- Render-Thread: `Menu::Render()` pro Frame aus `src/hooks/backend/opengl/hook_opengl.cpp:155` bzw. `vulkan/hook_vulkan.cpp:641`; ESP/Visuals in `src/menu/menu.cpp:955-961`.

## 1. Feature-Übersicht

„Registry" = Kartendefinition in `src/menu/module_registry.cpp`. „(n. g.)" = Java-Datei nicht geöffnet, Zuordnung aus Namen/CLAUDE.md.

| Name | Was es tut | C++ | Java |
|---|---|---|---|
| Target HUD | Zeigt zuletzt angegriffenen Spieler mit Live-Werten | Registry :18, `menu/target_hud.hpp`, `target_hud_draw.hpp` | `hud/TargetHudBridge`, `TargetHead` (n. g.) |
| Loot Beacon | Lichtsäule über ausgewählten Drops | Registry :28, `input/visual_modules.hpp:89-104`, `loot_beacon_rules.hpp` | `visual/ItemEsp` |
| Echo Trail | Verblassende Bewegungsspuren hinter Spielern | Registry :40, `utils/esp/echo_trail.hpp` | `esp/EchoTrailBridge` |
| Item ESP | Boxen/Labels für Drops, Farben pro Gruppe | Registry :49, `input/visual_modules.hpp` | `visual/ItemEsp` |
| Friends (+ Mittelklick) | Freunde vom Targeting ausnehmen, eigene ESP-Farbe, `bin/friends.txt` | Registry :60, `input/friends.hpp`, `menu/pages/friends_page.cpp` | `player/FriendsBridge` (n. g.) |
| Explosion Preview | Geschätzter Schaden von Crystals/Anchors | Registry :65, `input/visual_modules.hpp:130-132` | `visual/ExplosionPreview` (n. g.) |
| Projectile Prediction | Flugbahn und Einschlagpunkt | Registry :72, `input/visual_modules.hpp:133-144` | `visual/ProjectilePrediction` (n. g.) |
| Sound ESP | Geräuschmarker mit Richtung | Registry :82, `input/visual_modules.hpp:145-161` | `visual/SoundEsp`, `SoundMarkers` (n. g.) |
| Storage ESP | Kisten, Fässer, Shulker, Enderkisten | Registry :91, `input/visual_modules.hpp:162-185` | `visual/StorageEsp` |
| Auto Armor | Legt bessere Rüstung an | Registry :107, `modules/utility/assist_modules.cpp`, `input/utility_suite.hpp` | `assist/AutoArmorAssist`, `UtilitySuiteBridge` |
| Refill | Füllt Hotbar-Stacks nach | Registry :112, wie oben | `assist/RefillAssist` (n. g.) |
| Hit Effect | Partikel bei Treffern | Registry :116, `input/utility_suite.hpp:126-158` | `assist/HitEffects` (n. g.) |
| Predict Double Hand | Wählt bei Gefahr ein Hotbar-Totem | Registry :125, `input/utility_suite.hpp` | `assist/PredictDoubleHandAssist`, `DamagePrediction` (n. g.) |
| Auto Shield Breaker | Axt gegen blockende Spieler | Registry :134, `input/utility_suite.hpp:119-125`, `hooks/hooks.cpp:88-90` | `assist/ShieldBreakerAssist` (n. g.) |
| Fake Lag | Verzögert ausgehende Bewegungspakete | Registry :139, `input/fake_lag.hpp`, `modules/utility/fake_lag_module.cpp` | `network/FakeLagBridge` (n. g.) |
| Aim Assist | Zieht das sichtbare Aim zum Ziel | Registry :144, `modules/combat/aim_assist_module.cpp`, `input/combat/aim_assist.hpp`, `input/rotation/player_targeting.hpp` | `combat/AimAssistBridge`, `AimAssistMath` (n. g.) |
| Auto Tool | Passendes Werkzeug beim Abbauen | Registry :154, `input/auto_tool.hpp` | `movement/AutoToolBridge` (n. g.) |
| Safe Anchor | Anchor setzen, laden, zünden | Registry :158, `input/combat/safe_anchor.hpp`, `hooks/hooks.cpp:79-84` | `combat/SafeAnchorBridge` (n. g.) |
| Breach Swap | Waffenwechsel nach Schwertschlag | Registry :170, `input/combat/breach_swap.hpp` | `combat/BreachSwapBridge` (n. g.) |
| Auto Mace | Mace-Angriff im Fall | Registry :173, `modules/combat/auto_mace_module.cpp`, `input/combat/auto_mace.hpp` | `combat/AutoMaceBridge` (n. g.) |
| Auto Pearl Catch | Perle + Wind Charge auf Hotkey | Registry :178, `modules/movement/auto_pearl_catch_module.cpp` | `movement/AutoPearlCatchBridge` (n. g.) |
| Short Wind Charge | Wind-Charge-Sprung auf Hotkey | Registry :182, `modules/movement/short_wind_charge_module.cpp` | `movement/ShortWindChargeBridge` (n. g.) |
| Safe Walk | Verhindert Laufen über Kanten | Registry :187, `modules/movement/safe_walk_module.cpp` | `movement/SafeWalkBridge` (n. g.) |
| AntiBot | Globaler Bot-Filter (Tabliste, UUID, ungültige Daten, Schonfrist) | Registry :190, `input/rotation/antibot.hpp` | `player/AntiBotBridge` |
| Silent Aim | Server-Rotation zum Ziel, Angriff bei Linksklick mit Schwert | Registry :196, `modules/combat/silent_aim_module.cpp`, `utils/SilentAim.cpp`, `input/rotation/controller.hpp` | – (reines JNI) |
| Auto Crystal (CW) | Zerstört platzierte Crystals, eigener Thread | Registry :200 **und** :204, `modules/combat/cw.cpp`, `CwCrystal.cpp` | – |
| HitCrystal | Obsidian + Crystal setzen/sprengen bei Rechtsklick mit Schwert | Checkbox unter Auto Crystal, `modules/combat/hit_crystal_module.cpp`, `HitCrystal.cpp` | – |
| Reach | Schreibt Reichweite (Felder, Reflection, Attribute) | Registry :207, `modules/combat/reach_module.cpp`, `Reach.cpp` | – |
| FastPlace | `rightClickDelay = 0` | Registry :211, `modules/utility/fast_place.cpp`, `utils/sdk/CMinecraft.cpp:135-141` | – |
| AutoSprint | `setSprinting(true)` bei Vorwärtsbewegung | Registry :214, `modules/movement/auto_sprint_module.cpp`, `Sprint.hpp` | – |
| NoJumpDelay | Entfernt Sprungverzögerung | Registry :218, `modules/movement/no_jump_delay_module.cpp`, `NoJumpDelay.hpp` | – |
| AutoTotem | Totem per Inventar-Klicks in die Offhand | Registry :221, `modules/combat/auto_totem_module.cpp`, `AutoTotem.hpp` | – (Flag auch an UtilitySuite, `utility_suite.hpp:98`) |
| SeedCracker | Strukturdaten sammeln, Seeds verifizieren, Shipwreck-Boxen | Registry :224, `modules/utility/seedcracker_module.cpp`, `utils/seedcracker/*`, `menu/pages/seedcracker_page.cpp` | `seedcracker/SeedCrackerBridge`, `HashVerifiedDataStorage` |
| OreSim | Erzvorhersage aus Weltseed | Registry :231, `utils/oresim/oresim.hpp`, `seed_settings.hpp` | `oresim/OreSimBridge`, `OreSimulation`, `Ore` |
| Player ESP (Box/Skin/Glow) | Spieler-Boxen oder CPU-gerenderte Skins | Registry :245, `utils/esp/player_esp.hpp`, `skin_geometry.hpp`, `glow.hpp` | `esp/PlayerEspBridge`, `PlayerSkinBridge` |
| Manual Attack / Rotation | Gemeinsame Rotation und Zielauswahl | `input/rotation/*` | `aim/ManualAttackBridge` (n. g.) |
| Hotbar-Spoofing | Koordiniert Slotwechsel mehrerer Module | Checkboxen in Registry (z. B. :180, :185) | `movement/HotbarAction`, `HotbarCoordinator` (n. g.) |
| Hotkeys | Toggle/Hold pro Modul, Aktionstasten | `menu/module_registry.cpp:345-390`, `input/hotkey_state.hpp`, `hooks/hooks.cpp:31-92` | – |
| Profile / Config-Sharing | `.cfg`-Profile, Share-Codes | `utils/config/*`, `menu/pages/config_page.cpp` | – |
| Themes | Farbschemata, Glow | `utils/theme/*`, `menu/pages/theme_page.cpp` | – |
| Login/Auth | Login mit SHA-256 + HWID | `menu/login.cpp`, `auth/Authclient.cpp`, `HWID.cpp`, `Hash.cpp` | – |
| Watermark / Status-HUD / Diagnose / History / Performance | Overlays und Hilfen | `menu/watermark.hpp`, `status_hud.hpp`, `diagnostics.hpp`, `history.hpp`, `performance.hpp` | – |

## 2. Struktur / Wartbarkeit (nach Nutzen sortiert)

**1. Absoluter Entwickler-Pfad zur JAR, 20-mal kopiert.**
- Stellen: `input/rotation/antibot.hpp:17`, `utils/esp/player_esp.hpp:39`, `echo_trail.hpp:29`, `utils/oresim/oresim.hpp:55`, `input/utility_suite.hpp:33`, `visual_modules.hpp:30`, `friends.hpp:58`, `auto_tool.hpp:28`, `fake_lag.hpp:28`, `rotation/manual_attack.hpp:29`, `input/combat/{aim_assist:28,auto_mace:29,safe_anchor:28,breach_swap:29}.hpp`, `modules/movement/{short_wind_charge:24,safe_walk:27,auto_pearl_catch:25}_module.cpp`, `modules/utility/seedcracker_module.cpp:61-62`, `utils/seedcracker/shipwreck_esp.hpp:22`, `menu/target_hud.hpp:24`.
- Problem: zwei Schreibweisen (`seedcrackerbridge.jar` / `C:/…/SeedCrackerBridge.jar`); nur der erste Aufrufer zählt, weil `Init` bei `g_ready` sofort zurückkehrt (`utils/seedcracker/seedcracker_bridge.cpp:98`). Auf jedem anderen Rechner/Installationsort laden alle Bridge-Module nicht.
- Umbau: `SeedCracker::BridgeJarPath()` (DLL-Verzeichnis + `SeedCrackerBridge.jar`, Muster `friends.hpp:17-21`) und Overload `Init(JNIEnv*)`; die 20 Literale ersetzen.
- Prüfen: Build; im Log genau einmal „Lade Bridge-Jar von: <bin>".

**2. Modul-Metadaten in sieben handgepflegten Listen.**
- Stellen: Kartenliste mit festem `MODULE_COUNT = 35` (`menu/module_registry.cpp:13,17-251`); Beschreibungen nach Name (`:286-321`, tote Einträge „HitCrystal" :313, „Speed" :315); Icons nach Name (`menu/menu.cpp:81ff`); Status-HUD-Liste (`menu/status_hud.hpp:15-24`); Label-Übersetzung (`utils/config/settings_text.hpp:6-118`); Runtime-Module (`modules/ModuleManager.cpp:10-33`); Shutdown-Flags (`menu/menu.cpp:911-914`).
- Folgen: Im Status-HUD fehlen 9 Module (Loot Beacon, Echo Trail, Item ESP, Friends, Explosion Preview, Projectile Prediction, Sound ESP, Storage ESP, AntiBot). Deutsche Labels sind zugleich Profil-/Share-Code-Schlüssel (`config.cpp:107,305`) – ein umbenanntes Label verliert still gespeicherte Werte.
- Umbau: `StatusHud::Draw` über `Registry::GetModules()` iterieren (nach `enabled`-Zeiger dedupliziert); `MODULE_COUNT` aus `std::size(modules)`; optional `description`/`icon` als Felder in `ModuleData` (`utils/config/config.hpp:30-39`).
- Prüfen: HUD zeigt jedes aktive Modul; Profil laden+speichern ergibt identische Datei.

**3. Bridge-Boilerplate pro Datei kopiert.**
- C++: dasselbe „Check / Load mit 5-s-Retry / GetStaticMethodID / NewGlobalRef / Clear" in `utility_suite.hpp:24-55`, `visual_modules.hpp:22-44`, `auto_mace.hpp:16-51`, `safe_walk_module.cpp:16-36`, `target_hud.hpp:17-42`, `oresim.hpp:47-66`, `player_esp.hpp:17-48`, `echo_trail.hpp:27-35`, `shipwreck_esp.hpp:20-26` – mal mit `JniSafety::Lookup`, mal roh.
- Java: 19-Werte-Kameramatrix 7-mal kopiert: `visual/VisualFrame.java:18-21`, `esp/PlayerEspBridge.java:19-23`, `esp/EchoTrailBridge.java:25-29`, `esp/PlayerSkinBridge.java:73`, `oresim/OreSimBridge.java:68-74`, `seedcracker/SeedCrackerBridge.java:71-74`, `assist/HitEffects.java:197`.
- Guard „Spiel aktiv?" mehrfach leicht unterschiedlich: `aim_assist_module.cpp:19-22`, `auto_mace_module.cpp:13-14`, `silent_aim_module.cpp:292-316`, `safe_walk_module.cpp:40-42`, `utility_suite.hpp:67-68`; dazu 5× `GetForegroundWindow` pro Frame (`target_hud.hpp:45`, `visual_modules.hpp:47`, `echo_trail.hpp:21`, `shipwreck_esp.hpp:15`, `friends.hpp:72`).
- Umbau: kleines `struct BridgeClass { Load(env, "phantomui.X", {{&id,"name","sig"},…}); Release(env); }` neben `jni_safety.hpp`; Java `CameraHeader.write(Minecraft, double[])`; `Input::GameplayActive()`. Schrittweise beim Anfassen der Datei umstellen.

**4. Altlasten außerhalb der JNI-Konventionen, toter Code, Encodings.**
- Roh-JNI ohne `LocalFrame`/`ExceptionCheck`, verändert Spielzustand vom Worker-Thread: `modules/combat/HitCrystal.cpp:134-196`, `AutoTotem.hpp:70-190`, `modules/movement/Sprint.hpp:14-40` (neuere Bridges gehen über `mc.execute`, `java/assist/UtilitySuiteBridge.java:25`).
- Toter Code: `LoginRequest` mit `127.0.0.1:8080` ohne Aufrufer (`auth/Authclient.cpp:11-68`); No-op-Block `hooks/hooks.cpp:133-151`; unerreichbarer Zweig `AutoTotem.hpp:106-114`.
- Encoding: `AutoTotem.hpp` ist Windows-1252 statt UTF-8; doppelt kodierter Kommentar `menu.cpp:939`.
- Umbau: toten Code löschen, `AutoTotem.hpp` nach UTF-8 konvertieren, Konventionen bei Punkt 11/12 nachziehen.

## 3. Performance pro Frame/Tick (nach Nutzen sortiert)

**5. Zielsuche und AntiBot lösen bei jedem Worker-Tick alles neu auf.**
- Wo: `Rotation::FindPlayerTarget` (`input/rotation/player_targeting.hpp:16-43`, pro Spieler `:57-63`), ungedrosselt pro Tick aus `aim_assist_module.cpp:28`, `auto_mace_module.cpp:17`, `silent_aim_module.cpp:334`.
- Warum: pro Aufruf 2 `GetFieldID`, 10 `GetMethodID`, 4 `GetObjectClass`, pro Spieler 6 `GetMethodID`. Der `PlayerFilter::AntiBot`-Konstruktor (`antibot.hpp:15-31`) macht jedes Mal `SeedCracker::Init` (String-Allokation + Mutex), `LoadExtensionClass` (Mutex, `NewStringUTF`, Java-`loadClass`, `seedcracker_bridge.cpp:294-307`), 2 `GetStaticMethodID` und `configure`. Grob 170 JNI-Aufrufe bei 10 Spielern, bis 200×/s je Modul. AntiBot läuft zusätzlich pro Frame (`player_esp.hpp:49`, `echo_trail.hpp:36`) und Tick (`auto_mace.hpp:41`, `breach_swap.hpp:41`, `utility_suite.hpp:76`); Worker und Render-Thread konkurrieren um `g_init_mutex`.
- Beheben: IDs einmalig in `static`s (gegen Basisklassen `Entity`/`Player` über `GameRuntime::FindClass`); AntiBot-Klasse als GlobalRef + statische Method-IDs, `configure` nur bei geänderten Einstellungen; Zielsuche auf 50 ms drosseln (Muster `safe_walk_module.cpp:44-46`). GlobalRef beim Unload freigeben (`menu.cpp:915-929`).
- Prüfen: Aim Assist, Auto Mace, Silent Aim, AntiBot laut TEST-CHECKLIST; Unload/Reload ohne Crash.

**6. `UtilitySuite::Publish` bis zu 5× pro Worker-Durchlauf, ungedrosselt.**
- Wo: `modules/utility/assist_modules.cpp:7`; die Klasse ist 5-mal instanziiert (`ModuleManager.cpp:25-29`), jede aktive ruft `Publish` (`input/utility_suite.hpp:56-118`).
- Warum: pro Aufruf Status-String kopieren (`:61-66`), AntiBot-Konstruktor (`:76`), `NewIntArray(26)` (`:105-106`), `publish`; Java klont das Array und allokiert einen `Request` (`UtilitySuiteBridge.java:20-23`), obwohl nur ein Client-Tick pro 50 ms ihn verarbeitet. Bis ~1000 Publishes/s.
- Beheben: nach dem Lock `static steady_clock::time_point next; if (now < next) return; next = now + 50ms;` (Request-Ablauf 500 ms bleibt eingehalten).

**7. Blockierende `Sleep`s auf dem gemeinsamen Worker frieren alle Module ein.**
- Wo: `HitCrystal::Run` schläft 100+100+100+20+200 ms (`HitCrystal.cpp:78,83,85,99,114`) + 50 ms (`:378`), direkt aus dem Tick solange Rechtsklick gehalten (`hit_crystal_module.cpp:21-22`); `AutoTotem::Tick` 2×100 ms (`AutoTotem.hpp:56,58`).
- Warum: währenddessen kein anderes `Tick` (Zielsuche, SafeWalk, Publish). ≥570 ms überschreiten das 500-ms-Ablaufdatum der UtilitySuite → Auto Armor/Predict Double Hand pausieren (`UtilitySuiteBridge.java:29,35`). Ohne Schwert druckt `Run` bei gehaltener rechter Maustaste jeden Tick zwei Zeilen (`HitCrystal.cpp:22,49`).
- Beheben: `Run` einmalig über `Lifecycle::Spawn` mit `std::atomic<bool> busy` starten (Muster `CwCrystal.cpp:15-25`, eigener Attach/Detach); Logs drosseln; AutoTotem als Zustandsautomat mit Zeitstempeln.

**8. `VisualModulesBridge.frame` allokiert ~147 KB pro Frame, Labels jedes Frame komplett über JNI.**
- Java: `new double[19+9*2048]` pro Frame (`visual/VisualFrame.java:10`) + `Arrays.copyOf` (`:29`); pro Record `double[9]` + zwei `Vec3` (`:25`); `options.clone()`/`labels.clone()` (`VisualModulesBridge.java:11,19`); `String.format` pro Item-Label (`ItemEsp.java:20`); Storage ESP validiert bis 512 Boxen per `getChunk` pro Frame (`StorageEsp.java:46-53`).
- C++: pro Frame `NewIntArray(29)` (`input/visual_modules.hpp:65-66`), neuer `std::vector<std::string>` und pro Label `GetObjectArrayElement`+`GetStringUTFChars` (`:71-80`), auch wenn keine Labels aktiv sind.
- Warum: bei 144 fps ~21 MB Garbage/s allein aus dem Puffer → GC-Druck im Spiel.
- Beheben: `values` als wiederverwendeter Puffer (nur Render-Thread); `add(...)` direkt in den Puffer; Labels nur holen wenn `options[22] || options[23]`, Vektor wiederverwenden; Storage-Validierung in den 50-ms-Scan-Zweig (`StorageEsp.java:32`).

**9. Skin-ESP: bis ~15 MB frische C++-Allokationen plus Sortierung pro Frame.**
- C++: `std::vector<Quad>`, `std::vector<Polygon>`, `mask` pro Frame neu (`utils/esp/skin_geometry.hpp:35,57,60`), `std::stable_sort` (`:53`), Freundesfarbe pro Quad (`:47-48`); davor `GetDoubleArrayRegion` bis 6,7 MB (`player_esp.hpp:61-65`).
- Java: komplette Neu-Rasterung für bis 32 Spieler, `new Vertex` pro Vertex, Stream-Sort, `new PoseStack`/`Mesh` (`esp/PlayerSkinBridge.java:68-71,84,94,120,131`).
- Beheben (klein): Vektoren wiederverwenden (wie `frameValues`, `player_esp.hpp:16`), Tint vor die Schleife, `std::sort`. Bei Bedarf `MAX_QUADS` (60000, `PlayerSkinBridge.java:20`) senken.

**10. Kleinere namensbasierte Lookups auf heißen Pfaden.**
- `Input::GameScreenOpen`: `GetFieldID`+`GetMethodID` pro Aufruf (`input/game_screen.hpp:13,17`) – pro Frame sobald ein Hotkey gebunden ist (`module_registry.cpp:354`) und pro Tick aus Aim Assist/Auto Mace/Silent Aim.
- `SilentAim::apply`: 3 `GetFieldID` + 1 `GetMethodID` pro Aufruf (`utils/SilentAim.cpp:46-51,68`); `AimHeadOnly`/`GetAttackStrength` ebenso (`silent_aim_module.cpp:72-73,183`).
- Reach: alle 150 ms läuft der volle Reflection- und Attribut-Fallback, auch wenn die direkten Felder schon geschrieben wurden (`Reach.cpp:143,148`, Lookups `:250-265`, `:390-391`).
- `GameRuntime::FindClass` löst `loadClass` bei jedem Aufruf neu auf (`utils/sdk/game_runtime.hpp:77`).
- Beheben: IDs in `static`s; Reach-Fallbacks nur bei `!wrote_any`; `loadClass`-ID einmal cachen.

## 4. Bestehende Features verbessern (nach Nutzen sortiert)

**11. Absturzrisiko durch ungeprüfte Field-/Method-IDs.**
- `CMinecraft::Init` meldet „bereit" schon bei `f_player && f_game_mode && m_get_inventory && m_stack_is_empty` (`utils/sdk/CMinecraft.cpp:120`). AutoSprint nutzt ungeprüft `f_input`, `f_move_vector`, `f_vec2_y`, `m_is_shifting` (`Sprint.hpp:19,21,25,28`; `c_input` hat bereits einen Namens-Fallback, kann fehlen). AutoTotem nutzt ungeprüft `m_get_offhand_item`, `c_inv_screen`, `m_handle_inv_click` (`AutoTotem.hpp:71,126-128,140`) – letzteres loggt `CMinecraft.cpp:121` selbst als möglich fehlend.
- Folge: JNI mit Null-ID → JVM-Crash.
- Fix: am Anfang von `AutoSprint::Run` / `AutoTotem::Tick` alle IDs prüfen, sonst einmalig loggen und zurückkehren; nach rohen `Call…` `ExceptionCheck`+`Clear`.

**12. HitCrystal löst ohne Kontextprüfung aus und iteriert ungeschützt.**
- `GetAsyncKeyState(VK_RBUTTON)` ohne Fokus-/Menü-/Screen-Prüfung (`hit_crystal_module.cpp:21`) – feuert auch bei Rechtsklick im ImGui-Menü, Inventar, Chat oder anderem Fenster.
- `FindNearestCrystalEntity` iteriert vom Worker-Thread über `entitiesForRendering` ohne ein `ExceptionCheck` (`HitCrystal.cpp:134-196`, Schleife `:175-191`); eine `ConcurrentModificationException` bleibt anhängig.
- Fix: Guards wie Silent Aim (`silent_aim_module.cpp:292-316`, bzw. Helper aus Punkt 3); nach `hasNext`/`next` prüfen (`silent_aim_module.cpp:143-153`); Momentaufnahme per `toArray` (`player_targeting.hpp:41-43`).

**13. Doppelte „Auto Crystal"-Karte.**
- `menu/module_registry.cpp:200-203` und `:204-206`, beide an `&CW_Enabled`. Beide schreiben `module.Auto Crystal.hotkey`; beim Laden gewinnt die letzte Zeile für beide (`config.cpp:281-302`); zwei `HotkeyState`s togglen im selben Frame und heben sich auf (`hotkey_state.hpp:29-30`).
- Fix: `:204-206` löschen **und** `MODULE_COUNT` auf 34 (`:13`) bzw. `std::size` – sonst nullinitialisierter Eintrag und Crash bei `module.enabled->load()` (`:366`). Nebenbei redundante Selbst-Checkboxen entfernen (`:208`, `:212`, `:215`, `:219`, `:222`).

**14. Profil laden setzt nichts zurück; Speichern/Löschen ohne Rückfrage.**
- `Config::Load` setzt nur `hotkey_mode` und `favorite` zurück (`utils/config/config.cpp:147-150`); `enabled`, `hotkey` und Settings bleiben für fehlende Schlüssel stehen – ältere Profile lassen neuere Module aktiv.
- „Save" überschreibt gleichnamige Profile sofort (`config_page.cpp:33` → `config.cpp:127-128`), „Delete" löscht sofort (`config_page.cpp:49`).
- Fix: vor `Config::Load` (`config_page.cpp:47`, einziger Aufrufer) `Registry::ResetModule(i)` für alle Module (`module_registry.cpp:269-282`); Zwei-Klick-Bestätigung („Sure?" für 3 s).

**15. Datenrennen bei der Seed-Übergabe SeedCracker → OreSim.**
- Der Worker ruft `SeedHandoff::Apply` (`seedcracker_module.cpp:78-84`) und schreibt über `OreSim::SetCrackerSeed` ungeschützt `manualSeed`, `seedInput[]`, `seedError` (`seed_handoff.hpp:54-55`, `seed_settings.hpp:31-37`), während der Render-Thread sie liest/schreibt (`oresim.hpp:78-81,98`, `menu.cpp:163`; `oresim.hpp:15` sagt selbst „Render-thread state only").
- Fix: Auto-Apply an den Anfang von `OreSim::Render` verschieben (`Candidates()`/`HasApplied()` sind schon Mutex-geschützt). Nebenbei: Java sucht `biomeZoomSeed` alle 50 ms per Reflection neu (`SeedCrackerBridge.java:131-133`, dreifach kopiert `:131,:161,:303`) – `static Field` cachen.

**16. Login und HWID.**
- Login unverschlüsselt über Port 80 (`auth/Authclient.cpp:76,78`, kein `WINHTTP_FLAG_SECURE`); der ungesalzene SHA-256 des Passworts (`login.cpp:317`) ist mitlesbar und wiederverwendbar. Benutzername nicht URL-kodiert (`:80`), keine Timeouts/Handle-Prüfung (`:75-83`) – ein hängender Server blockiert das Entladen. Der Avatar-Abruf nutzt schon HTTPS + Timeouts (`:182-189`).
- HWID: `GetMachineGUID` liest bei Fehler uninitialisiertes `char value[64]` (`HWID.cpp:25-39`); Laufwerk `"C:\\"` fest (`:9`).
- Fix: Port 443 + `WINHTTP_FLAG_SECURE` + `WinHttpSetTimeouts`, `HttpHandle`-RAII (`:181`), Prozent-Kodierung, `char value[64]{}`. **Risiko:** Server muss HTTPS können; HWID-Laufwerksänderung sperrt bestehende Lizenzen – nur mit Serverseite abstimmen.

## 5. Feature-Ideen

**17. Bridge-Handshake + erweitertes `check_protocol` – klein.** `protocolVersion()` in `SeedCrackerBridge`, geprüft in `SeedCracker::Init`, angezeigt in `diagnostics.hpp`. `check_protocol` (`tools/client_build.py:25-36`) um die 29 Visual-Optionen (`visual_modules.hpp:60` ↔ `VisualModulesBridge.java:16`) und Record-Größen (`player_esp.hpp:75` ↔ `PlayerEspBridge.java:18`) erweitern.

**18. Letztes Profil automatisch laden – klein.** `Config::active_profile` atomar in eine Datei neben den Profilen schreiben (Muster `config.cpp:113-120`) und beim Start laden; ein Autoload existiert bisher nicht.

**19. Performance-Anzeige pro Feature – klein.** `Performance::Section` (`menu/performance.hpp:8`) um Visual Modules, Echo Trail, Target HUD, Hit Effect erweitern (Scopes in `menu.cpp:953-961`); Worker-Tick-Dauer als atomares Sample. Macht Punkte 5–10 messbar.

**20. Player ESP: Name, Leben, Distanz, Tracer – mittel.** 7-Werte-Record (`PlayerEspBridge.java:32-34`, `player_esp.hpp:75,84`) um Health und Entity-ID erweitern, Namen über den vorhandenen Label-Mechanismus (`visual_modules.hpp:123-129`). ABI-Änderung in beiden Repos gleichzeitig.

## Top 3 zum Anfangen

1. **Punkte 5 + 6 (ID-/AntiBot-Cache, 50-ms-Throttle):** wenige Zeilen in drei Dateien, entfernt den Großteil der JNI-Last auf Worker und Render-Thread, ohne ABI-Änderung.
2. **Punkte 7 + 12 (HitCrystal aus dem gemeinsamen Worker, mit Guards):** beseitigt das Einfrieren aller Module, Fehlauslösungen im Menü/Inventar und den Konsolen-Spam.
3. **Punkt 13 (doppelte „Auto Crystal"-Karte, `MODULE_COUNT` mitziehen):** Zwei-Zeilen-Fix für einen sichtbaren Menüfehler und den kaputten Hotkey nach Profil-Laden.

Direkt danach: Punkt 11 (Null-ID-Guards) – verhindert einen möglichen JVM-Crash mit ~5 Zeilen.

## Nicht geprüft

- C++ nicht geöffnet: `hook_vulkan.cpp` (nur :641), `widgets.cpp`, `custom_color_picker.cpp`, `theme_page`/`settings_page`/`seedcracker_page`, `resources.cpp`, `imageloader.cpp`, `key_mapping.cpp`, `controller.hpp`, `share_code.hpp`, `diagnostics.hpp`, `history.hpp`, `watermark.hpp`, `lifecycle.hpp`, `NoJumpDelay.hpp`.
- C++ nur teilweise: `menu.cpp` (566-645, 895-964 + grep), `login.cpp` (300-345), `HitCrystal.cpp:198-415`, `CwCrystal.cpp:262-370`, `Reach.cpp` (108-163 + grep), `config.cpp:186-270`.
- Java nicht geöffnet: `HotbarAction`, `HotbarCoordinator`, `SafeAnchorBridge`, `FakeLagBridge`, `OreSimulation`, `HitEffects`, `TargetHudBridge`, `AimAssistBridge`, `AutoMaceBridge`, `NativeActions`; `SeedCrackerBridge` nur teilweise.
- Menü-interne Allokationen bei offenem Menü (`menu.cpp:276-282,574,627,746`; `config_page.cpp:37`) gesehen, wegen geringer Priorität nicht aufgenommen.
- graphify: nur `GRAPH_REPORT.md` gegrept (God Nodes/Knowledge Gaps betreffen fast nur vendored ImGui/JNI); `graphify query` wurde nicht ausgeführt.
- Laufzeitverhalten: ob Minecraft 26.2 die Worker-Thread-Zugriffe (`setScreen`, `attack`, `sendPosition` in AutoTotem/SilentAim/HitCrystal) verträgt und die reale Worker-Tickrate – nur aus dem Code abgeleitet.
