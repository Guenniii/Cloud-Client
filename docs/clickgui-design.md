# PhantomUI Click-GUI – Design-Spezifikation

Figma: https://www.figma.com/design/VcWGCiEWlSXlJh5XNHnRs2
Frames: `1 · Hauptmenü – Midnight Indigo`, `2 · Hauptmenü – Ocean Mint (Preset getauscht)`, `3 · Komponenten`.

Alle Maße in px bei **100 % Menü-Skalierung**. Bei 80–120 % alle Werte mit `scale = Config::menu_size_percent/100` multiplizieren
(Linienstärken 1 px bleiben 1 px). Alles ist mit `ImDrawList` baubar: `AddRectFilled`/`AddRect` mit Rundung, `AddCircleFilled`,
`AddText`, `AddRectFilledMultiColor` (Verläufe) und das vorhandene `DrawGlowRect`/`DrawGlowCircle` (Glow). Kein Blur, keine Masken.

## 1. Farben

Es gibt **keine neuen Farbfelder**: alle Rollen sind die Felder aus `Theme::Palette` (`Theme::Current()`), also funktionieren alle
13 Presets, Custom Accent/Background und Animated Accent automatisch. Einzige abgeleitete Farbe ist `glow` = `accent` mit Alpha 0.45
(entspricht `DrawGlowRect` mit `alpha≈1`).

| Rolle | Verwendung | Midnight Indigo | Ocean Mint |
|---|---|---|---|
| `background` | Fenster, Eingabefelder, Hotkey-Badge, Swatch-Box | `#111321` | `#0D1722` |
| `card` | Modul-Karte, Suchfeld, Popup; Sidebar mit 60 % Alpha | `#191D30` | `#162431` |
| `border` | 1-px-Rahmen, Divider; Tab-Hover mit 55 % Alpha | `#2F354E` | `#2A3E4D` |
| `text` | Titel, Modulnamen, aktive Labels | `#ECEFFF` | `#E7F5F7` |
| `muted` | Untertitel, Meta-Zeile, inaktive Icons, Section-Labels | `#9AA4C1` | `#97B0B9` |
| `label` | Setting-Labels, Tab-Label (normal) | `#BEC6E3` | `#B7D2D8` |
| `accent` | Toggle an, Slider-Füllung/Knopf, Slider-Wert, aktive Icons, Indikatorbalken, Count-Badge aktiv | `#9587FF` | `#4BDAB7` |
| `hover` | (Reserve, z. B. Slider-Knopf aktiv) | `#B0A4FF` | `#71EBCE` |
| `soft` | Icon-Box, aktiver Tab, Favorites-Button, Count-Badge inaktiv, Capture-Badge | `#302C4D` | `#1E4142` |
| `toggle` | Toggle aus, Slider-Schiene | `#434964` | `#3D5160` |
| `onAccent` | Text auf `accent` (Count-Badge aktiv, Logo-Icon) | `#161529` | `#0C2422` |
| `success` | (nicht im Click-GUI verwendet) | `#6DD9AB` | `#5DDC99` |
| `glow` | Glow von Toggle, Slider, Indikatoren, Logo | `accent` @ 45 % | `accent` @ 45 % |

Feste Farben: Toggle-Knopf `#FFFFFF`; Fenster-Schatten Schwarz 50 %, y +16, Radius 48 (optional, ImGui: 3–4 Lagen `AddRectFilled`
mit sinkendem Alpha); Popup-Schatten Schwarz 45 %, y +8, Radius 24.

## 2. Typografie

Schrift: **Poppins** (bereits als `poppin_font` eingebettet), Icons **Font Awesome 5 Free Solid** (`IconsFontAwesome5.h`).
Benötigte Atlas-Größen: 10, 11, 12, 13, 14, 15, 18 (Poppins) und 8, 10, 12, 14, 15 (Icons) – oder eine Größe laden und
`AddText(font, size, …)` nutzen.

| Stil | Größe / Gewicht | Farbe | Verwendung |
|---|---|---|---|
| Seitentitel | 18 SemiBold | text | „Visual modules“ |
| Logo | 15 SemiBold | text | „PhantomUI“ |
| Modulname | 14 SemiBold | text | Kartenkopf |
| Tab-Label | 13 Medium | label / text (Hover, Aktiv) | Sidebar |
| Setting-Label | 13 Regular | label | Zeilen |
| Slider-Wert | 13 Medium | accent | „2500 ms“ |
| Body/Meta | 12 Regular | muted | Untertitel, „7 settings“, Beschreibung |
| Meta aktiv | 12 Medium | accent | „3 active“ |
| Hex-Wert | 12 Medium | text | „#F459D5“ |
| Badge / Button klein | 11 Medium | je nach Zustand | Hotkey, Toggle/Hold, Reset |
| Count-Badge | 11 SemiBold | onAccent / label | Sidebar |
| Section-Label | 10 SemiBold, Großbuchstaben, Laufweite +1.2 | muted | „MODULE“, „CLIENT“ |

Zeilenhöhe ≈ 1.5 × Schriftgröße; Text in Zeilen immer vertikal zentrieren (`y + (h - fontSize)/2`).

## 3. Layout (Hauptmenü 900 × 600)

```
┌────────────┬──────────────────────────────────────────────────────┐
│ Sidebar    │ Content (Padding 20 l/r/o, 0 unten, clip)            │
│ 200 breit  │ Header 36 hoch: Titel+Untertitel | Suche 210×34 | Fav │
│ Pad 16/20  │ Abstand 14                                            │
│ Logo 30    │ Modul-Liste (Abstand 8, scrollbar, 660 innen breit)   │
│ Tabs 36    │   Karte 60 hoch, aufgeklappt = 60 + 1 + Settings      │
│ …          │                                                       │
│ Profil-Chip│                                                       │
└────────────┴──────────────────────────────────────────────────────┘
```

- **Fenster**: 900 × 600, Rundung 16, Füllung `background`, Rahmen 1 px `border`, Fenster-Padding 0 (Inhalt eigene Paddings).
- **Sidebar**: x 0, Breite 200, volle Höhe, Füllung `card` @ 60 %. Padding links/rechts 16, oben 20, unten 16, vertikaler Abstand 4.
  - Logo-Zeile: Mark 30 × 30, Rundung 9, `accent` + Glow, Icon `ICON_FA_GHOST` 14 `onAccent`; daneben (Abstand 10) „PhantomUI“ 15 und
    „Minecraft 26.2“ 11 muted. 14 Abstand nach unten.
  - Section-Label: Padding links 6, oben 8, unten 4.
  - Tabs: 168 × 36 (siehe Komponenten), Reihenfolge Combat, Movement, Utility, Visuals | CLIENT: Settings, Themes, Profiles, Friends,
    SeedCracker (Icons wie heute in `menu.cpp`). Count-Badge nur bei den vier Modul-Kategorien (= Anzahl aktiver Module).
  - Profil-Chip unten: volle Breite, Padding 10/8, Rundung 10, `background` + Rahmen `border`; Icon `ICON_FA_FILE` 12 accent,
    „Profil“ 10 muted / Profilname 12 Medium text, rechts „END“ 10 SemiBold muted (Hinweis Entladen).
- **Content**: x 200, Breite 700, Padding 20 (unten 0), Clip.
  - Header: links Titel 18 + Untertitel 12 („4 of 11 enabled“). Rechts Suchfeld 210 × 34, Rundung 10, `card` + Rahmen `border`,
    Padding 12, Icon `ICON_FA_SEARCH` 12 muted, Abstand 8, Platzhalter 12 muted. Abstand 10. Favorites-Button Höhe 34, Padding 12,
    Rundung 10, `soft`, Stern 12 accent + „Favorites“ 12 Medium text, Abstand 7. Bei aktivem Favoritenfilter: Füllung `accent`,
    Text/Icon `onAccent`.
  - Modul-Liste: Abstand 14 unter Header, Kartenabstand 8, Breite 660. Scrollen per Mausrad (Child-Window, keine sichtbare Scrollbar
    oder 3 px Scrollbar `border`, Rundung 2).

## 4. Komponenten

### Toggle (Checkbox, auch Modul-Schalter)
- 36 × 20, Rundung 10. Aus: `toggle`. An: `accent` + `DrawGlowRect(…, accent, t)`.
- Knopf: Kreis Ø 14, `#FFFFFF`, Mittelpunkt x = `3 + 7 + t·16`, y = Mitte.
- Hover: Schiene +8 % Richtung `text` blenden (`Theme::Blend(col, text, .08)`).
- Klickfläche = ganze Zeile (Label + Toggle), nicht nur der Schalter.

### Checkbox-Zeile
- Breite = Spaltenbreite (296), Höhe 28. Label 13 `label` links, Toggle rechts bündig, beide vertikal zentriert.

### Int-Slider-Zeile
- Breite 296, Höhe 44: Kopf 20 (Label 13 `label` links, Wert `"%d%s"` 13 Medium `accent` rechts), Abstand 6, Spur 12 hoch.
- Schiene: 4 hoch, Rundung 2, `toggle`, mittig in der Spur. Füllung: 0 → `t·w`, `accent` + Glow (Alpha 0.35).
- Knopf: Ø 11 `accent` + `DrawGlowCircle`, Mittelpunkt auf Füllungsende. Hover/Aktiv: Ø 13, Glow 1.0 statt 0.85,
  Knopf-Farbe `hover`.
- Suffix exakt aus `ModuleSetting` (`" ms"`, `" %"`, `"%"`, `" m"`, `" px"`, `" deg"`, `" deg/s"`, `" ticks"`, `" Items"`, `" HP"`,
  `" chunks"`; leer erlaubt).
- Interaktion wie heute (unsichtbares `SliderInt` über der Spur); zusätzlich Strg+Klick = Werteingabe (ImGui-Standard).

### Farbwähler (ColorRGB)
- Zeile 296 × 28: Label links; rechts Box 112 × 26, Rundung 6, `background` + Rahmen `border`, Swatch 18 × 16 bei (7, 5) Rundung 4,
  Hex `#RRGGBB` 12 Medium `text` bei x 33. Hover: Rahmen `accent` @ 60 %. Geöffnet: Rahmen `accent`.
- Popup 232 breit, Padding 12, Abstand 10, Rundung 10, `card`, Rahmen `border`, Schatten. Inhalt:
  1. Titel „Farbe: <Modul>“ 12 SemiBold + Schließen-Icon `ICON_FA_TIMES` 11 muted.
  2. SV-Feld 208 × 120, Rundung 6: `AddRectFilledMultiColor(weiß, hue, hue, weiß)` dann
     `AddRectFilledMultiColor(transparent, transparent, schwarz, schwarz)`; Cursor Ring Ø 12, Weiß, 2 px.
     (MultiColor kennt keine Rundung → Ecken mit 4 kleinen `background`-Viertelkreisen übermalen oder Rundung weglassen.)
  3. Farbton-Leiste 208 × 10, Rundung 5: 6 Segmente `AddRectFilledMultiColor` (Rot→Gelb→Grün→Cyan→Blau→Magenta→Rot);
     Knopf Ø 14 mit aktueller Farbe, Ring 2 px Weiß.
  4. Hex-Feld (Rest-Breite × 26, Rundung 6, `background`, Rahmen `accent` bei Fokus) + Vorschau 26 × 26 Rundung 6.
  5. Preset-Swatches 20 × 20, Rundung 5, Abstand 6: aktueller `accent`, `secondary_rgb`, `friend_rgb`, `enemy_rgb`, `#F0B852`,
     `#3470DF`, `#B6F43E`, `#FFFFFF`.

### Hotkey-Feld (Badge)
- Höhe 20, Padding 7 × 2, Rundung 5, Text 11 Medium. Steht 10 px rechts neben dem Modulnamen.

| Zustand | Füllung | Rahmen | Text |
|---|---|---|---|
| Leer | `background` | – | „Set Key“ `muted` |
| Gesetzt | `background` | 1 px `border` | Tastenname (`GetKeyName`) `text` |
| Hover | `soft` | 1 px `border` | wie oben |
| Capture | `soft` | 1.5 px `accent`, Alpha pulsiert 0.5↔1 (800 ms) | „...“ `accent` |
| Konflikt | `background` | 1.5 px `accent` | Tastenname `text`, daneben „Key conflict“ 11 `accent` + Tooltip |

Klick = Capture starten/abbrechen, Rechtsklick = löschen, Backspace im Capture = leeren, Escape = abbrechen (wie heute).

### Favorit-Stern
- `ICON_FA_STAR` 14, Trefferfläche 28 × 32. Aus: `muted`, Hover: `label`, Favorit: `accent`. Steht 12 px links vom Modul-Toggle.

### Modul-Karte
- Breite = Liste (660), Kopf 60 hoch, Rundung 12, Padding links/rechts 16, horizontaler Abstand 12, alles vertikal zentriert.
- Inhalt v. l. n. r.: Icon-Box 32 × 32 Rundung 9 `soft` mit Modul-Icon 15 (an: `accent`, aus: `label`) | Textblock (füllt):
  Zeile 1 Name 14 SemiBold + Hotkey-Badge; Zeile 2 (Abstand 1) „N settings“ 12 muted + Chevron 8 muted (`ICON_FA_CHEVRON_DOWN`,
  aufgeklappt `ICON_FA_CHEVRON_UP`) + Punkt Ø 4 muted + „N active“ 12 Medium accent (nur wenn > 0) | Stern | Toggle.
- Zustände:

| Zustand | Füllung | Rahmen | Extras |
|---|---|---|---|
| Aus | `card` | 1 px `border` | Icon `label`, Toggle aus |
| An | `card` | 1 px `border` | Indikator links 3 × 28 bei (0, 16), Rundung 2, `accent` + Glow; Icon `accent`; Toggle an |
| Hover | `card` + `soft` @ 35 % | 1 px `accent` @ 45 % | sonst wie aus/an |
| Aufgeklappt | `card` | 1 px `accent` @ 35 % | Divider 1 px `border` unter dem Kopf, dann Settings-Bereich |

- Klickflächen: Kopf (außer Badge/Stern/Toggle) = auf-/zuklappen; Toggle = an/aus; Badge = Hotkey; Stern = Favorit.
- **Settings-Bereich**: Padding 16 (oben 12, unten 14), Abstand 12.
  1. Beschreibung (`Registry::Description`) 12 muted, umbrechend.
  2. Zwei Spalten à 296, Spaltenabstand 24: links Checkbox- und Farbzeilen (Abstand 8), rechts Slider (Abstand 10).
     Reihenfolge innerhalb der Spalten wie in `module_registry.cpp`. Hat ein Modul nur einen Typ, Einträge abwechselnd auf
     beide Spalten verteilen.
  3. Aktionszeile: Segment „Toggle | Hold“ (`HotkeyMode`): Außen Padding 2, Rundung 8, `background` + Rahmen `border`; Segment
     Padding 10 × 3, Rundung 6, aktiv `soft` + Text `accent`, inaktiv Text `muted`, 11 Medium. Daneben „Hotkey-Modus“ 11 muted.
     Rechts „Reset module“ (`Registry::ResetModule`): Padding 10 × 4, Rundung 8, Rahmen `border`, Icon `ICON_FA_UNDO` 10 muted +
     Text 11 Medium `label`; Hover Füllung `soft`.
- Höhe aufgeklappt = 60 + 1 + 12 + Beschreibung + 12 + max(Spaltenhöhe) + 12 + 24 + 14.

### Kategorie-Tab (Sidebar)
- 168 × 36, Rundung 10, Padding links 14 / rechts 10, Abstand 10: Icon 14 (16 breit, zentriert) | Label 13 Medium (füllt) | Count-Badge.
- Count-Badge: Padding 6 × 1, Rundung 9 (Pille, Höhe 18), Text 11 SemiBold.

| Zustand | Füllung | Icon | Label | Badge |
|---|---|---|---|---|
| Normal | – | `muted` | `label` | `soft` / Text `label` |
| Hover | `border` @ 55 % | `label` | `text` | `soft` / `label` |
| Aktiv | `soft` | `accent` | `text` | `accent` / Text `onAccent` |

Aktiv zusätzlich Indikator 3 × 16 bei (0, 10), Rundung 2, `accent` + Glow.

## 5. Animationen

Alle Übergänge als exponentielle Annäherung pro Frame wie im bestehenden Code:
`v += (target - v) * ImSaturate(dt * k)`. Angegebene Dauer ≈ Zeit bis 95 % (`3/k` s).

| Element | Eigenschaft | k | Dauer |
|---|---|---|---|
| Toggle | Knopf-Position, Farbe off→on, Glow | 12 | ~250 ms |
| Hover (Karte, Tab, Buttons, Badge) | Füllung/Rahmen-Blend | 16 | ~190 ms |
| Karte auf/zu | Höhe 60 ↔ voll, Settings-Alpha 0→1 (Alpha erst ab 40 % Höhe) | 10 | ~300 ms |
| Tab-Wechsel | Aktiv-Hintergrund gleitet vertikal zum neuen Tab (Pill wie heute `last_category`) | 14 | ~210 ms |
| Seitenwechsel | Content-Alpha 1→0→1 + y-Versatz 6 px → 0 | 12 | ~250 ms |
| Slider | Knopf Ø 11 → 13 und Glow bei Hover/Drag | 18 | ~170 ms |
| Hotkey-Capture | Rahmen-Alpha `0.75 + 0.25·sin(t·2π/0.8)` | – | 800 ms Periode |
| Menü öffnen | Fenster-Alpha 0→1, Skalierung 0.97→1 | 14 | ~210 ms |
| Popup Farbwähler | Alpha 0→1, y −4 → 0 | 18 | ~170 ms |

Bei „Animated Accent“ (Theme) läuft der Akzent bereits über `Theme::Current()`; keine Extra-Logik nötig.

## 6. Hinweise zur Umsetzung

- Glow immer über die vorhandenen `DrawGlowRect`/`DrawGlowCircle` (respektiert `Theme::MenuGlow()`), nicht neu bauen.
- Alpha-Multiplikation für Ein-/Ausblenden über `ColA(col, alpha)`.
- Modul-Icons und Kategorie-Icons wie in `menu.cpp` (`ModuleIcon`, `categories[]`), keine neuen Icons nötig außer
  `ICON_FA_GHOST` (Logo), `ICON_FA_UNDO` (Reset), `ICON_FA_TIMES` (Popup schließen), `ICON_FA_CHEVRON_UP`.
- Bei schmalem Fenster (< 760 bei Skalierung 80 %): Sidebar auf 64 px nur Icons (Label + Badge ausblenden, Badge als Punkt 6 px
  oben rechts am Icon), Settings einspaltig.
- Texte in Settings bleiben exakt wie in `module_registry.cpp` (ASCII-Umschreibungen wie „Linienstaerke“ beibehalten).
