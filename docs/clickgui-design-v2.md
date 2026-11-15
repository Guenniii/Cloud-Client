# PhantomUI Click-GUI v2 „Orbit“ – Design-Spezifikation

Entwurf (Canvas): https://claude.ai/artifact/MY62iTmCevmJbbd9YFJheK
Figma-Datei (angelegt, noch leer – MCP-Limit des Starter-Plans erreicht): https://www.figma.com/design/qySKPgr1rSE4TIjaQVnnYY

Idee: statt Sidebar + Liste ein **Kategorie-Ring** (links), Module als **Waben-Raster** (rechts oben) und ein
**Inspector** für das gewählte Modul (rechts unten), verbunden durch eine leuchtende Linie. Alles mit `ImDrawList` baubar:
`PathArcTo`/`PathStroke` (Ring), `AddConvexPolyFilled` (Hexagone), `AddQuadFilled` (Rauten), `AddRectFilled`, `AddCircleFilled`,
`AddText`, Glow über die vorhandenen `DrawGlowRect`/`DrawGlowCircle`. Maße in px bei 100 %, mit `menu_size_percent/100` skalieren.

## Farben
Nur `Theme::Palette`-Felder, daher funktionieren alle 13 Presets. Entwurf zeigt **Neon Orchid** und **Amber Graphite**.

| Rolle | Neon Orchid | Amber Graphite | Verwendung |
|---|---|---|---|
| background | `#170D23` | `#18191C` | Fenster, Eingaben, Keycap |
| card | `#261635` | `#232428` | Mittelscheibe, Hex aus, Inspector, Dock |
| border | `#4F2C62` | `#3D3E43` | Ränder, gestrichelte Trenner |
| text | `#FFF0FF` | `#F6F3E9` | Titel, Werte |
| muted | `#BF9CC9` | `#ADACA1` | Mono-Metadaten, Hex-Icon aus |
| label | `#D8B3ED` | `#D6CDB6` | Setting-Labels, Hex-Name aus |
| accent | `#F459D5` | `#F0B852` | aktiv, Füllungen, LED, Linie |
| hover | `#FF89E5` | `#FFCF7E` | Kopfzelle Slider |
| soft | `#4D2055` | `#423420` | Hex an, Dock-Hover, Segment aktiv |
| toggle | `#5A3868` | `#4E4D47` | Schalter aus, leere Zellen, Ring idle |
| onAccent | `#230C2A` | `#251D10` | Icon/Text auf accent |

Abgeleitet: Glow = accent @ 55 %, Power-Ring = accent @ 18 %, Ring-Halo = accent @ 22 %, Punktraster = muted @ 14 %.

## Schrift
**Space Grotesk** (500/600/700) für Text, **JetBrains Mono** (400/700) für Zahlen, Codes, Metadaten (Großbuchstaben, Laufweite +1…2).
Größen: Seitentitel 44/700 (−1.5 Laufweite), Inspector-Name 22/700, Kategorie 13/600, Label 13, Wert Mono 13/700,
Meta Mono 9–10, Hex-Name 8.5/700 Caps. Icons: Font Awesome (Mapping wie heute in `menu.cpp`).

## Layout 900 × 600, Rundung 20, Rand 1 px border
- **Links 330 px**: Halo `DrawGlowCircle` um (165, 238). Marke „phantom/ui“ (Mono 12, Schrägstrich accent) bei (24, 22).
  - **Ring**: Mittelpunkt (165, 238), r 96, Strichbreite 14, 4 Segmente à 82° mit 8° Lücke, Start −86° (oben, im Uhrzeigersinn):
    Combat (oben rechts), Movement, Utility, Visuals (oben links). Idle `toggle`, Hover `label` @ 55 %, Aktiv `accent` + Halo 24 px @ 22 %.
  - Beschriftung je Segment: Mittelwinkel, r 150, Box 92 × 36 zentriert: Icon 14 + Name 13/600, darunter Mono 9 „0N ON“.
  - **Tick-Ring** r 70–77: 35 Striche (ein Strich je Modul), Breite 2.5, an `accent`, aus muted @ 30 %. Gestrichelter Kreis r 62.
  - **Mittelscheibe** r 54 `card` + Rand: Ghost-Icon 20 accent, „12/35“ Mono 24/700 (/35 muted 14), „ONLINE“ Mono 8.
  - **Dock** y 444: 5 Kreisbuttons Ø 44, Abstand 10, ab x 35 (Settings, Themes, Profiles, Friends, SeedCracker).
    Idle `card` + Rand border, Icon label; Hover `soft` + Rand accent, Icon accent, Tooltip-Pille 30 px darüber (accent, Mono 9/700).
  - **Profil-Leiste** (24, 532), 282 × 44, Rundung 10: LED Ø 8 accent, „PROFILE“ Mono 9 / Name 13/600, rechts „END = unload“.
- **Trenner** x 330, gestrichelt, y 24–576.
- **Rechts** (ab x 331, Punktraster 18 px, Punkt Ø 2):
  - Kopf (21, 20): Mono 10 „04 / 04 · CATEGORY“, Titel 44, Mono 10 „11 MODULES · 4 ONLINE · 2 FAVORITES“.
  - Suche oben rechts (rechts 24, y 26): 190 × 40, Rundung 8, `background` + Rand; „›“ accent, Eingabe Mono 12, Block-Cursor 7 × 14 accent.
    Daneben Favoritenfilter 40 × 40.
  - **Waben-Raster** ab (40, 110): Hexagon spitze Seite oben, 76 × 88, Spaltenabstand 82, Zeilenabstand 70, jede 2. Zeile +41.
    6 Kacheln pro Zeile. Inhalt: LED Ø 5 (+ Stern 8 bei Favorit) bei y 14, Icon 20, Name 8.5 Caps max. 2 Zeilen, Breite 60.
  - **Verbindung**: 2 px accent + Glow von der unteren Spitze der gewählten Kachel bis zur Inspector-Oberkante, Punkt Ø 10 am Ende.
  - **Reiter** (21, 276), 170 × 27, Rundung oben 10: Mono 10 „VIS·03 ▸ INSPECT“ (Kürzel accent).
  - **Inspector** (21, 302), 524 × 282, Rundung 0/18/18/18 (oben links eckig = am Reiter), Padding 18, Abstand 12.
    - Kopf: Power Ø 44 | Name 22 + Beschreibung 11 muted (eine Zeile, abgeschnitten) | Keycap | TOG/HOLD | Stern Ø 30 | Reset Ø 30.
    - Gestrichelter Trenner.
    - Zwei Spalten à 230, Abstand 28: links Schalter- und Farbzeilen (Höhe 30, Abstand 4), rechts Segment-Slider (Abstand 10).
- Andere Seiten (Settings, Themes, Profiles, Friends, SeedCracker) nutzen nur die rechte Zone mit Kopf + Inspector-Stil.

## Komponenten und Zustände
| Komponente | Maße | aus / normal | hover | an / aktiv |
|---|---|---|---|---|
| Hex-Kachel | 76 × 88, Rand 1 (gewählt 2) | Rand border, `card`, Icon muted, LED toggle | Rand label, soft @ 60 %, Icon text | Rand accent @ 55 %, `soft`, Icon accent, LED accent + Glow; **gewählt**: Rand accent 2 px + Glow 10 |
| Ring-Segment | r 96, 14 breit, 82° | toggle | label @ 55 % | accent + Halo |
| Rauten-Schalter | 40 × 20, Rundung 5, Raute 9 × 9 | `toggle`, Raute label links (x 5), „OFF“ rechts | Rand label, Raute text | accent @ 18 % + Rand accent, Raute accent + Glow rechts (x 25), „ON“ links |
| Segment-Slider | 20 Zellen, Höhe 10, Abstand 3, Rundung 2 | leer `toggle`; gefüllt accent mit Alpha 0.35→1 | Kopfzelle Glow 8 | Drag: Kopfzelle 14 hoch, Glow 12; Wert Mono 13 accent + Einheit 10 muted; Min/Max Mono 9 darunter |
| Keycap | 30 × 30 (breiter bei langen Namen), Rundung 6, Unterkante 3 px | leer: gestrichelt, „+“ muted | Füllung soft | gesetzt: `background`, Text; Capture: soft + Rand accent + Glow-Puls, „···“; Konflikt: Rand accent + „!“ + Tooltip |
| Power | Ø 44 | Ring border 1.5 auf background, Icon muted | Ring label | accent, Icon onAccent, Ring 4 px accent @ 18 %, Glow 18 |
| Farbe | Zeile 116 × 28, Hexagon-Swatch 14 × 16 | Rand border | Rand accent @ 60 % | Popup: Farbring Ø 160 / 16 breit, SV-Feld 90 (ImGui `ColorPicker3` mit `PickerHueWheel`), Hexagon-Swatches |
| Dock-Button | Ø 44 | card + border, Icon label | soft + Rand accent, Icon accent, Tooltip | aktuelle Seite = wie hover ohne Tooltip |

Interaktion: Kachel-Klick = auswählen (Inspector), Doppelklick oder Power = an/aus, Rechtsklick = Favorit, Mittelklick = Hotkey-Capture.
Keycap: Klick = Capture, Rechtsklick = löschen, Escape = abbrechen (wie heute).

## Bewegung (ms, exponentiell `v += (t−v)·sat(dt·k)`, k ≈ 3000/ms)
Kategorie wechseln 220 · Kacheln einblenden versetzt 18 je Kachel · Verbindungslinie wächst 180 · Rauten-Schalter 160 ·
Slider-Zellen 90 · Hover 120 · Tick-Ring 250 · Keycap-Puls 800 Periode · Menü öffnen: Ring dreht −20° → 0° in 320.
