# Orbit-Login – Design-Spezifikation und Umsetzungsplan

Vorschau aller Zustände: https://claude.ai/artifact/CAAvx4Fab77TvTmhdSESrM
Designsprache: [clickgui-design-v2.md](clickgui-design-v2.md) (Orbit). Status: **umgesetzt** (Schritte 1–6, siehe Abschnitt 5); noch nicht im Spiel getestet.
**Umfang:** dieses Dokument beschreibt nur den Orbit-Login (Pfade relativ zum Repo-Stamm: `UniversalHookX/src/…`). Ältere, unabhängige Änderungen im Arbeitsbaum siehe Ende von Abschnitt 5.

## 0. Befunde aus dem Code und Entscheidungen

- **`ui.menu_style` ist beim Login nicht aus einem Profil geladen.** Profile laden nicht automatisch, der Login läuft vor jedem Profil.
  `Config::menu_style` hat dort den Standardwert aus `UniversalHookX/src/utils/config/config.hpp` (Zeile 62, vom Nutzer auf **2** gesetzt),
  Theme steht auf Preset 0 *Pearl Violet* (hell), `menu_size` auf 100 %. Der Login-Schalter hängt **nicht** daran (siehe Entscheidung A); gelesen wird
  `menu_style` nur für den Übergang: bei 2 (Standard) ist der **Morph ins Orbit-Menü der Normalweg**, nur Ausblenden gibt es nur, wenn dieser Standard
  wieder auf 0/1 geändert wird. `config.cpp` fällt bei Profilen ohne oder mit ungültigem Schlüssel `ui.menu_style` weiterhin auf 1 zurück (von dieser Arbeit nicht geändert).
- **Server nicht erreichbar** kommt heute als `"Empty response"` (leere Antwort) oder `"Login request failed"` (Exception) an.
  Das lässt sich rein in der Anzeige zuordnen, ohne Request oder Parser zu ändern. Auch eine fehlerhafte `OK|…`-Antwort (`stoi` wirft) endet als
  „Login request failed“ und wird deshalb als „Server not reachable“ angezeigt. Enter sendet nur aus dem Passwortfeld ab, nicht aus dem Benutzerfeld.
- Erfolgsablauf heute: `LOGIN_SUCCESS_HOLD` 0.7 s + `SUCCESS_EXIT_DURATION` 1.1 s. Absenden ist gesperrt bis `intro_done` (1.6 s).
  Der Entwurf nutzt **genau diese Zeiten**, die Konstanten bleiben unverändert.

### Entscheidung A – Schalter: Compile-Time-Konstante
`constexpr int LOGIN_VIEW` am Anfang des anonymen Namespace in `UniversalHookX/src/menu/login.cpp`: `1` = klassischer Login, `2` = Orbit-Login
(Standard 2). Ändern und neu bauen; es gibt keine Datei auf der Platte (`login.cfg` wurde verworfen). Beide Ansichten bleiben
immer mitkompiliert, ein Wechsel kann den Build nicht brechen.

### Entscheidung B – „Login merken“ entfällt
Vom Nutzer gestrichen: es wird nichts gespeichert (weder Benutzername noch Passwort).

## 1. Fenster

- 420 × 540 px bei 100 %, Rundung 20, Füllung `background`, Rand 1 px `border`, Punktraster wie im Orbit-Menü (18 px, muted @ 14 %).
- Skalierung genau wie `Orbit::Draw`: `s = min(size/100, fit)`, alle Maße × s, Text `Fz()` (Mindestgröße 10 px).
  Beim Login ist `menu_size` immer 100 %, es wirkt nur das Verkleinern auf kleine Fenster (`fit`, Display minus 32 px).
- Mittig auf `io.DisplaySize` (nicht `SM_CXSCREEN`), nicht verschiebbar. Fenster-ID eigen (`###login_orbit`).
- Farben: nur `Theme::Current()`-Felder; Fehlerfarbe = die bestehende `IM_COL32(215,85,105)` = `#D75569`.

| Rolle | Pearl Violet (Start) | Neon Orchid |
|---|---|---|
| background / card / border | `#F8F9FC` / `#FFFFFF` / `#E6E6F2` | `#170D23` / `#261635` / `#4F2C62` |
| text / muted / label | `#19192B` / `#74768B` / `#696989` | `#FFF0FF` / `#BF9CC9` / `#D8B3ED` |
| accent / hover / soft / toggle | `#6F42FF` / `#855EFF` / `#EBE6FF` / `#DADBE7` | `#F459D5` / `#FF89E5` / `#4D2055` / `#5A3868` |
| onAccent / success | `#FFFFFF` / `#23AC6B` | `#230C2A` / `#5CEDB8` |
| Fehler | `#D75569` | `#D75569` |

## 2. Aufbau (y-Werte bei 100 %)

| Element | Position / Maß | Details |
|---|---|---|
| Halo | Mitte (210, 132), Radius 150 | `DrawGlowCircle` accent @ 10 % (Fehler: Fehlerfarbe, Erfolg 18 %) |
| Marke | (24, 22) | „phantom/ui“ Mono 12/700, „/“ accent; Chip „MC 26.2“ Mono 10, Rand border, Rundung 5 |
| Status rechts oben | rechts 24, y 25 | Mono 10 muted: `AUTH` / `AUTH · 0.8 s` (Laden, echte Laufzeit) / `AUTH · OK` |
| Ring | Mitte (210, 132), r 60, Breite 10 | 4 Segmente à 82°, Lücke 8°, Start −86°, Halo 18 px @ 22 % (`ALPHA_SEGMENT_HALO`) |
| Tick-Ring | r 44–50, 35 Striche, 2.5 px | Laden: 14 leuchten (laufen mit), Erfolg: alle |
| Mittelscheibe | r 34 `card` + Rand | Icon 24: Ghost (accent), bei Fehler Schloss / Schlüssel / WLAN-aus in Fehlerfarbe |
| Titel | Mitte, Oberkante y ≈ 201 (Textmitte 214) | Space Grotesk 26/700, Laufweite −0.8: „Sign in“ / „Verifying“ / Benutzername |
| Unterzeile | Mitte, y 234, Höhe 26 | Mono 10, Laufweite 1.5, muted – oder Fehler-Pille (unten) |
| Feld-Label | x 40, y 282 / 352 (Felder y 300 / 370) | Mono 9, Laufweite 1.5, muted (Fehler: Fehlerfarbe) |
| Eingabefeld | x 40, 340 × 44, Rundung 8 | `card`, Rand 1 border; Icon 14 links (Padding 12), Text 14 (Passwort: Mono 14/700, Laufweite 2) |
| Auge-Button | rechts im Passwortfeld, 32 × 32, Rundung 6 | Icon Auge / Auge-durchgestrichen 16 (Icon wechselt sofort, nur der `soft`-Hintergrund und die Icon-Farbe animieren); Hover, gedrückt und eingeschaltet: `soft` + Icon accent; nach dem Klick bleibt der Fokus im Passwortfeld |
| Hinweiszeile | y 422, Höhe 22 | nur rechts „CAPS ON“ Mono 9/700 accent, wenn Feststelltaste an; links nichts |
| Button | x 40, y 456, 340 × 46, Rundung 10 | accent, Text 14/700 onAccent + Enter-Icon 16, Glow 18 @ 35 %; Hover: `hover` + Glow 1.0 |
| Fortschritt | ersetzt den Button | `card` + Rand, Padding 9/14: oben Mono 9/700 (links Status, rechts „WAIT“ bzw. beim Erfolg die Zeit; die Laufzeit beim Laden steht im Header „AUTH · x s“), unten 20 Zellen × 8 px, Abstand 3, Rundung 2 |
| Fußzeile | y 516 | Mono 9 muted: „ENTER = SUBMIT“ links (kein ↵-Glyph im Font-Atlas), „END = UNLOAD“ rechts |

Fokus: Rand 1.5 px accent + äußerer Ring 3 px accent @ 14 %, Icon accent, Cursor accent.
Fehler-Pille: Höhe 26, Padding 10, Rundung 8, Fehler @ 10 % + Rand Fehler @ 40 %; Icon 12 + Text 12/600 Fehler + Code Mono 9 muted.

## 3. Zustände

| Zustand | Ring | Titel / Unterzeile | Felder | Unten |
|---|---|---|---|---|
| Eingabe | Segmente `toggle`, oben rechts accent (Segment 0, −86°…−4°) | „Sign in“ / `AUTHENTICATE TO OPEN ORBIT` | normal (kein Anfangsfokus) | Button „Sign in“ |
| Laden | alle accent (Alpha 1 / .5 / .65 / .8), dreht 360°/1.2 s, Ticks laufen mit | „Verifying“ / `CHECKING LICENSE · HWID` | Alpha 0.5, gesperrt | Fortschritt „VERIFYING“, 5-Zellen-Fenster läuft (60 ms/Zelle) |
| Fehler Passwort | Fehlerfarbe | „Sign in“ / Pille „Invalid credentials“ + Code „FAIL“ (muted, mit Abstand, ohne „·“) | Passwortfeld Fehler-Rand | Button „Sign in“ |
| Fehler Lizenz | Fehlerfarbe, Schlüssel-Icon | Pille „No active license · LICENSE_INVALID“ | normal | Button „Sign in“ |
| Fehler Server | Fehlerfarbe, WLAN-aus-Icon | Pille „Server not reachable“ + Code „NO RESPONSE“ | normal | Button „Retry“ (Pfeile-Icon) |
| Erfolg | 4 Segmente schließen zum Vollkreis, alle Ticks an | Benutzername / `WELCOME BACK` | ersetzt durch Lizenzkarte | Fortschritt „OPENING ORBIT“ in `success` |

**Lizenzkarte** (x 40, y 282, 340 × 156, Rundung 12, Padding 16, Abstand 10): Schlüssel-Icon + „LICENSE“ Mono 9 + Chip „ACTIVE“
(success @ 14 %); Zahl Mono 30/700 accent + „days left“ 15/600 + rechts „EXPIRES <expiryDate>“ Mono 10;
20 Zellen (Füllung = `min(daysLeft,30)/30`); unten Mono 9 „HWID BOUND“ / „AVATAR SYNCED“.
`daysLeft == -1`: „PERMANENT“ (ohne ∞), alle Zellen an, kein Ablaufdatum.
**Avatar**: Kreis Ø 60 in der Ringmitte, `UserTexture()`, Ring 3 px `card` + 2 px accent + Glow. Solange der Download läuft:
Kreis mit Initiale (26/700 onAccent); umgesetzt als einfacher accent-Kreis (kein Verlauf), danach Überblendung 200 ms.

**Fehler-Zuordnung (nur Anzeige, `responseText` unverändert):**

| Server-Text | Anzeige | Markiertes Feld / Icon |
|---|---|---|
| `FAIL` | Invalid credentials | Passwort / Schloss |
| `User not Found` | User not found | Benutzername / Person |
| `LICENSE_INVALID`, `NO_LICENSE` | No active license | – / Schlüssel |
| `License expired` | License expired | – / Schlüssel |
| `HWID_MISMATCH` | HWID mismatch | – / Schloss |
| `This User is banned` | Account banned | – / Schloss |
| `Empty response`, `Login request failed` | Server not reachable (+ Retry) | – / WLAN aus |
| alles andere | Text wie geliefert | – / Warnung |

Tippen nach einem Fehler setzt den Fehlerstil zurück (Pille bleibt, bis erneut abgeschickt wird).

**Umgesetzte Abweichungen (Anzeige):**
- „WLAN aus“: FA5 Free hat kein Glyph, es ist das WLAN-Icon mit diagonaler Fehlerlinie. Anzeigetexte kommen aus `MapLoginError` in `orbit.cpp`
  (`StatusText` der klassischen Ansicht bleibt unverändert; z. B. „License expired“ statt „Your license has expired“, „Account banned“).
  Unbekannte Serverantworten: nur erste Zeile, höchstens 60 Zeichen.
- Retry: jede neue Absendung setzt die Fehler-Flanken zurück, auch ein sofort wieder fehlschlagender Versuch schüttelt und färbt neu.
- Avatar: bis `Resources::HasUserAvatar()` wahr ist (heruntergeladenes Profilbild hochgeladen; `UserTexture()` liefert sonst das Standardlogo)
  zeigt die Mitte einen Accent-Kreis mit Initiale (nur bei ASCII-Anfangsbuchstaben), danach Überblendung. „AVATAR SYNCED“ erst dann.
- Fortschritt „OPENING ORBIT“ läuft über die ganzen 1.8 s (Halten + Exit); bei Menüstil 0/1 steht „OPENING MENU“.
- Eingabe: Passwort ohne Laufweite 2, Titel ohne Laufweite −0.8, kein Accent-Cursor (ImGui hat keine Cursorfarbe).

## 4. Animationen (ms)

| Was | Dauer | Kurve |
|---|---|---|
| Intro: Segmente zeichnen sich (0 → 82°), versetzt | 0–800, 60 je Segment | ease-out cubic |
| Intro: Titel + Unterzeile (Alpha, y +6 → 0) | 600–1000 | ease-out |
| Intro: Felder, dann Button | 800–1300 / 1100–1600 | ease-out (Ende = `INTRO_DURATION` 1.6 s, Absenden frei) |
| Fokus-Rand / Hover | 120 | exponentiell k = 25 |
| Auge umschalten | 120 | Hintergrund/Farbe (Icon wechselt sofort) |
| Laden: Ring-Drehung | 1200 je Umdrehung | linear |
| Laden: Zellen-Fenster | 60 je Zelle | linear, Endlosschleife |
| Fehler: Karte schütteln | 300 (±6 px, 3 Ausschläge) | gedämpfter Sinus |
| Fehler: Ring → Fehlerfarbe | 200 | linear |
| Erfolg: Segmente schließen | 250 | ease-out |
| Erfolg: Avatar ein (Skalierung 0.6 → 1) | 300 | ease-out back |
| Erfolg sichtbar (Halten + erste 0.4 s Exit) | 0–1100 | – |
| Morph: Inhalt aus | 1100–1250 | linear |
| Morph: Fenster 420 × 540 → 900 × 600, Ring (210,132,r60) → (165,238,r96) | 1100–1800 | ease-in-out |
| Danach | ab 1800 | `logged_in = true`; `Orbit::Draw` übernimmt (Waben versetzt, 18 je Kachel) |

Ist der Menüstil nicht Orbit (0/1), läuft der Morph nicht: nach 1.1 s nur Ausblenden (Alpha 1 → 0, 700 ms).

**Übergabe an `Orbit::Draw`:** Fenster, Ring (Mitte, Radius, Breite, Spanne) und Ticks wandern zum Menü-Fenster (gleiche Skalierung, Zentrierung
und Klemmung auf die vorige Menüposition wie `Orbit::Draw`); das Login-Raster blendet aus, das Raster der rechten Zone ein. Während des Morphs setzt
`DrawLogin` eine Frame-Nummer (`g_login_handoff_frame`); `Orbit::Draw` startet innerhalb von 3 Frames danach voll geöffnet (`open_t = 1`, gewählter Ring-Abschnitt
ohne Überblendung, Kacheln staffeln weiter). `menu.cpp` prüft `IsLoggedIn()` nach `Login::Render` erneut, damit der Frame des Wechsels schon das Menü zeichnet
(`open_alpha = 1` über `Orbit::TakesLoginHandoff()`). Nicht übernommen: Fenster-Verlauf und Schatten des Menüs im Morph, die Tick-Anzahl (35 → Modulanzahl) springt.

## 5. Umsetzung (Status)

Alle Login-Zustände, Request, HWID, Hash und Antwort-Parser sind unverändert. Keine Java-/Bridge-Änderung; diese Arbeit hat `src/auth/` nicht angefasst.
Einzige Verhaltensänderung der klassischen Ansicht: Absenden ist jetzt auch während des Success-Exit (`success_exit`) gesperrt
(vorher ca. 0.25 s Lücke zwischen Halten und `logged_in`).

| # | Schritt | Dateien (tatsächlich) | Stand |
|---|---|---|---|
| 1 | `login.cfg` | – | verworfen (siehe Entscheidung A/B) |
| 2 | Weiche, `View`, geteilter Submit | `menu/login.cpp`/`.hpp`: `View`, `SubmitLogin()` mit vollem Guard (`!loginInProgress`, `!login_success_pending`, `!success_exit`, `intro_done`), `StatusText()`, gemeinsamer Intro-Fortschritt in `Render`, `LOGIN_VIEW`; `menu/orbit.hpp`: `DrawLogin` | umgesetzt |
| 3 | Eingabe-Zustand | `menu/orbit.cpp`: `DrawLogin`, `DotGrid` (auch von `Orbit::Draw` genutzt) | umgesetzt |
| 4 | Laden, Fehler, Erfolg | `menu/orbit.cpp`: `MapLoginError`, Lizenzkarte, Avatar; `menu/resources.cpp`/`.hpp`: `HasUserAvatar()`; `View`: `success_exit_duration` | umgesetzt |
| 5 | Intro, Morph, Übergabe | `menu/orbit.cpp`: Intro-Phasen, Morph, `g_login_handoff_frame`; `menu/orbit.hpp`: `TakesLoginHandoff()`; `menu/menu.cpp`: Menü im selben Frame wie der Wechsel; `View`: `success_timer`, `success_hold` | umgesetzt |
| 6 | Doku + Testliste | diese Datei, `TEST-CHECKLIST.md` | umgesetzt |

Noch offen: Build und Test im Spiel (`.\tools\Build-Client.ps1 -Action build`), Punkte siehe `TEST-CHECKLIST.md`, Abschnitt „Login“.

### Nicht Teil dieser Arbeit (ältere, unabhängige Änderungen im Arbeitsbaum)

- `src/auth/*`: Beta-Feld in der Antwort und `LicenseInfo::beta`; `g_beta` / `Login::HasBeta()` in `login.cpp`.
- BETA-Badge im klassischen Footer (`menu.cpp`) und Benutzerleiste des Orbit-Menüs (Avatar, Name, ONLINE, BETA-Pille).
- `logged_in` startet in `login.cpp` wieder mit `false` (der letzte Commit hatte `true`), der Login ist also standardmäßig aktiv.
- `config.hpp`: Standard `menu_style = 2`.
