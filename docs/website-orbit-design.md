# Website im Orbit-Design – Bestandsaufnahme, Spezifikation, Plan

Projekt: `C:\Users\okeba\source\repos\dashboard-beta` (Express + EJS, MySQL). Status: **Entwurf, am Webseiten-Code ist nichts geändert.**
Vorschau (Startseite Desktop + Handy, Login, Account, Store, neue Module-Seite): https://claude.ai/artifact/2Fk9CzfuhsE55RXco97fcZ
Stylesheet-Entwurf: [website-orbit-draft.css](website-orbit-draft.css) (wird in Schritt 1 zu `public/orbit.css`).
Designsprache: [clickgui-design-v2.md](clickgui-design-v2.md), [login-orbit-design.md](login-orbit-design.md).

## 1. Bestandsaufnahme (Funktionen, nicht Design)

### Seiten (`views/`)
| View | Route(n) | Funktion |
|---|---|---|
| `home.ejs` | `GET /`, `GET /home` | Startseite mit Preisen aus `config/store.json` (über `lib/store-catalog`) |
| `store.ejs` | `GET /store` (`?plan=timed\|lifetime`) | Planauswahl, Kaufanfrage-Dialog, Key-Hinweis |
| `login.ejs` | `GET/POST /login` | Browser-Login (Felder `user`, `pass`, `remember`), merkt `?plan=` für den Store |
| `register.ejs` | `GET/POST /register` | Registrierung (`user`, `pass`, `pass_confirm`) |
| `user.ejs` + Partials `user-*` | `GET /user`, `/user/profile`, `/user/license`, `/user/downloads` | Übersicht, Profil (Avatar, Passwort), Lizenzen + Einlösen, Downloads |
| `account.ejs` (+ `hwid.ejs`) | `GET /account` | Browser-Sitzungen, HWID-Reset anfragen, Lizenz- und Sicherheitsverlauf |
| `support.ejs`, `ticket.ejs` | `GET/POST /support`, `GET /support/:id`, `POST /support/:id/reply`, `POST /support/:id/status` | Tickets (auch Kaufanfragen) |
| `updates.ejs` | `GET /updates` (nur angemeldet) | Release-Liste, Admin veröffentlicht/löscht |
| `admin.ejs` | `GET /admin` | Benutzer (Rolle, Bann, Beta, löschen), Lizenzen erstellen/löschen |
| `activity.ejs` | `GET /admin/activity` | Admin-Protokoll, HWID-Anfragen freigeben/ablehnen |
| `error.ejs` | Fehler-Handler | Fehlerseite |

Partials: `brand`, `nav`, ~~`public-nav`~~, ~~`public-footer`~~ (in Schritt 2 entfernt, ersetzt durch `orbit-nav`/`orbit-foot`), `page-start`, `page-end`, `pricing`, `redeem-dialog`, `hwid`, `user-downloads`, `user-licenses`, `user-profile`, `user-redeem`, `user-security`.

### Weitere Endpunkte (bleiben unverändert)
| Methode + Pfad | Antwort | Wer |
|---|---|---|
| `GET /logout` | Redirect `/` | Browser |
| `POST /redeem` | Text `OK` bzw. Fehlertext (`INVALID` …) | `redeem.js` |
| `GET /download` | Datei oder 403 „Keine Lizenz.“ | Browser |
| `POST /profile/password` | (`account-routes.js`) | `account.js` |
| `POST /profile/avatar` | JSON `{ok:true,url:"/profile/avatar?user=…&v=…"}` | `profile-avatar.js` |
| `DELETE /profile/avatar` | JSON `{ok:true}` | `profile-avatar.js` |
| `GET /profile/avatar?user=` | Bild | **Client** (`DownloadUserAvatar`, `dashboard.cloudclient.vip`) und Website |
| `POST /client/hwid-reset` | Text `OK\|PENDING`, `FAIL`, `INVALID_REASON`, `ALREADY_PENDING`, `COOLDOWN`, `RATE_LIMITED`, `ERROR` | **Client**, ohne Cookie |
| `POST /account/sessions/revoke`, `POST /account/hwid` | Redirect | Account-Seite |
| `POST /store/request` | Redirect 303 auf `/support/:id` | Store |
| `POST /admin/*` (toggle-role/ban/beta, delete-user, create-license, delete-license, releases, hwid) | Redirect | Admin |

Hinweis: Der **Client-Login** geht an `auth.cloudclient.vip/login` (eigener Auth-Server, nicht dieses Repo). `/login` hier ist der
Browser-Login. Beide bleiben, wie sie sind.

### Was das neue Markup erhalten muss
- **Formularfelder und Aktionen** wie oben (Namen, Methoden, Ziel-URLs), versteckte `_csrf`-Felder bzw. Header `X-Profile-Token`
  (Pflicht für `/admin/*`, `/redeem`, `/store/request`).
- **Von Tests geprüft** (`tests/*.js`): `id="passwordForm"`, `id="avatar-form"` (nur bei eigenem Profil), `<dialog id="redeemDialog"`,
  genau ein `id="redeemForm"`, Text „Lizenz aktivieren“, `href="/user"`, `href="/admin"` nur für Admins, `href="/download"` nur mit
  gültiger Lizenz, `/default-avatar.svg`, Admin-Aktions-URLs (`/admin/create-license`, `/admin/toggle-role/<id>`).
- **Von Skripten genutzt** (`public/*.js`): `#avatar-file/-form/-remove/-status/-upload`, `#profile-avatar`, `#passwordForm`,
  `#passwordMessage`, `#redeemDialog/-Form/-Message`, `data-open-redeem`, `data-close-redeem`, `data-choose-plan`, `data-close-plan`,
  `dialog[data-plan]`, `data-purchase-form`, `data-filter`, ~~`data-preview-toggle`~~ (in Schritt 2 entfernt). Die Klassen `.menu-toggle` und `.nav a` werden
  durch `data-menu-toggle` / `data-nav` ersetzt (zwei Selektoren in `dashboard.js`), damit keine alte Klasse übrig bleibt.

## 2. Design

**Idee:** Die Website sieht aus wie das Orbit-Menü: Kategorie-Ring, Waben, Reiter-Karten („Dossier“), Segment-Balken, Rauten-Schalter,
Keycaps, Mono-Metadaten. Alles neu; keine alte Farbe, Klasse oder Seitenaufteilung.

### Tokens (CSS-Variablen in `orbit.css`)
| Token | Wert | Herkunft |
|---|---|---|
| `--bg / --card / --border` | `#170D23 / #261635 / #4F2C62` | Theme-Preset Neon Orchid |
| `--text / --muted / --label` | `#FFF0FF / #BF9CC9 / #D8B3ED` | Neon Orchid |
| `--accent / --hover / --soft / --toggle / --on-accent` | `#F459D5 / #FF89E5 / #4D2055 / #5A3868 / #230C2A` | Neon Orchid |
| `--ok / --err` | `#5CEDB8 / #E5677C` | success; Fehler etwas heller als im Client für Kontrast auf Dunkel |
| `--glow / --ring / --glass` | accent 45 % / accent 16 % / card 62 % | Glow, Fokus-Ring, Glas-Flächen |
| Schrift | Space Grotesk 400–700, JetBrains Mono 400–700 | wie Orbit |
| Radien | 6 / 10 / 18 px; Karten oben links 4 px („Reiter-Schnitt“) | Orbit-Inspector |
| Raster | Punkte 22 px, 13 % muted | Orbit-Punktraster |

### Bausteine (alle mit `o-`-Präfix)
| Baustein | Maße / Verhalten |
|---|---|
| `o-nav` | sticky, 68 px, `backdrop-filter: blur(14px)`, Glas 72 %, gestrichelte Unterkante; Links Mono 12 Caps, Ziel ≥ 44 px |
| `o-btn--accent / --ghost` | Höhe 46, Rundung 10, Akzent mit Glow (Hover stärker), Ghost mit Rand |
| `o-card` (+ `--cut`, `--glass`, `--live`) | Rundung 18 (oben links 4), Padding 22; Glas mit Blur 12; „live“ = Akzent-Rand + Glow (aktiver Plan, aktive Lizenz) |
| `o-tab` | Reiter über einer Karte, Mono 11 Caps, Nummer in Akzent („**02** Lizenz“) |
| `o-seg` | 20 Zellen × 10 px, Abstand 3, Kopfzelle hover + Glow (Lizenz-Resttage, Passwortstärke) |
| `o-rune` | Checkbox als Rauten-Schalter 44 × 24 (Angemeldet bleiben, Bestätigungen) |
| `o-field` | 48 px, Icon links, Fokus: Akzent-Rand + 3 px Ring; Fehler `--err` |
| `o-pill` | Fehlermeldung mit Icon, Text und Code rechts in Mono |
| `o-key`, `o-chip`, `o-led` | Keycap mit 3 px Unterkante; Mono-Chips; Status-LED |
| `o-hex` | Wabe per `clip-path` (Module, Avatar-Rahmen) |
| `o-eyebrow` | Mono 11, Laufweite .16em, Abschnitt „**01** / 04 · Name“ |

### Seiten
- **Startseite:** Hero mit großem Kategorie-Ring (4 Segmente mit Modulzahlen 10/6/8/11, 35 Striche drehen sich langsam, 90 s),
  Abschnitte 01 Module (Waben), 02 Ablauf (3 Knoten), 03 Zugang (2 Pläne aus `store.json`), 04 Changelog-Auszug.
- **Login/Register:** Ring-Illustration + Glas-Karte mit Reiter „AUTH ▸ Login“, Fehler als Pille, „Angemeldet bleiben“ als Rauten-Schalter.
- **Account:** ein Raster mit Reiter-Karten 01 Profil (Waben-Avatar, Beta-Chip), 02 Lizenz (Resttage groß + Segmentbalken, „Lizenz aktivieren“),
  03 HWID, 04 Sitzungen, 05 Sicherheit (`#passwordForm`), 06 Verlauf (Zeitleiste mit Rauten). `/user/*`-Tabs werden zu Sprungzielen dieser Karten.
- **Store:** Ablauf-Leiste 01–03, zwei Plan-Karten mit Ring (Monatlich = 30/365 gefüllt, Lifetime = voll), Kaufanfrage als Glas-Karte.
- **Support/Ticket, Updates, Admin, Aktivität:** gleiche Bausteine (Plan Schritte 5–6), Tabellen in `overflow-x:auto`-Boxen.

### Responsive und Bewegung
- Container-Queries auf `.o-page` (Breakpoints 980 / 760 / 640 px), ab 375 px einspaltig, Navigation als Menü-Button.
- Scroll-Einblendung `o-reveal` per `animation-timeline: view()` (nur wo unterstützt, sonst sofort sichtbar), 24 px nach oben.
- Ring-Drehung 90 s (Startseite), 6 s (Login); Hover 120 ms; Rauten-Schalter 160 ms.
- `prefers-reduced-motion`: alle Animationen aus.

## 3. Neue Funktionen (Vorschläge, max. 6)
| # | Funktion | Nutzen | Aufwand | Neuer Endpunkt / DB |
|---|---|---|---|---|
| 1 | **Module-Seite** `/features` (in der Vorschau) | zeigt, was der Client kann; Namen und Texte wie im Spiel | S | neue GET-Route; Daten als `lib/modules.json`, einmal aus `module_registry.cpp` übernommen. Keine DB |
| 2 | **Öffentlicher Changelog** `/changelog` | Updates ohne Login sichtbar, Startseiten-Auszug | S | neue GET-Route, liest bestehende `dashboard_releases`. Keine neue Tabelle |
| 3 | **Status-Seite** `/status` | „Geht der Login gerade?“ ohne Ticket | M | GET `/status` (+ `/status.json`): Website ok, DB-Ping, Erreichbarkeit des Auth-Servers (serverseitig, 60 s Cache), letzte Version. Keine Tabelle |
| 4 | **FAQ** `/faq` | weniger Support-Tickets | S | nur EJS, keine DB |
| 5 | **Lizenz-Erinnerung** im Account | Ablauf unter 7 Tagen sichtbar, Knopf „Verlängern“ → `/store?plan=timed` | S | nichts Neues (nutzt `license_expiry`) |
| 6 | **Theme-Lab** `/themes` | die 13 Client-Presets ansehen, Theme-Code erzeugen | M | nur Frontend; Code-Format muss exakt `share_code.hpp` folgen (vorher prüfen) |

Die Module-Texte bleiben englisch wie im Client (Entscheidung unten).

## 4. Umsetzungsplan (erst nach OK)

Bei Express + EJS bleiben. Jeder Schritt einzeln testbar; `npm test` muss nach jedem Schritt grün sein. Login-, Lizenz- und HWID-Logik,
DB-Schema und Antwortformate bleiben unverändert. Keine Zugangsdaten aus `server.js`/`config/` in Doku oder Vorschau.

| # | Schritt | Dateien | Prüfung |
|---|---|---|---|
| 0 | `npm ci` (erledigt 2026-11-16). Ausgangslage danach: `account`, `avatar`, `store` grün; **`features` und `client-hwid` rot**, weil ihre Test-Datenbank die Beta-Migration `ALTER TABLE users ADD COLUMN beta …` nicht kennt (Fehler „Unhandled test query“). Fix: beide Test-Fakes nehmen diese Abfrage an (2 Zeilen, nur `tests/`). Außerdem ist `dashboard-beta` **kein Git-Repo**: vorher `git init` + erster Commit oder eine Sicherungskopie, damit jeder Schritt zurückgerollt werden kann | `tests/features.test.js`, `tests/client-hwid.test.js` | alle 5 Testdateien grün |
| 1 | `public/orbit.css` + neue Partials `orbit-head`, `orbit-nav`, `orbit-foot`; `error.ejs` als erste Seite | `public/`, `views/partials/`, `views/error.ejs` | `npm test`; Fehlerseite 375 / 1280 px |
| 2 | Öffentliche Seiten: `home`, `store` (+ `pricing`), Menü-Skript auf `data-*` | `views/home.ejs`, `store.ejs`, `partials/pricing.ejs`, `public/dashboard.js`, `storefront.js` | `npm test`; Planwahl, Kaufanfrage, `?plan=` |
| 3 | `login`, `register` | `views/login.ejs`, `register.ejs` | `npm test`; Fehler, „Angemeldet bleiben“, Weiterleitung mit Plan |
| 4 | Account-Bereich: `user` + `user-*`-Partials, `account`, `hwid`, `redeem-dialog` | `views/user.ejs`, `account.ejs`, Partials | `npm test` (prüft IDs); Avatar hoch/runter, Passwort, Einlösen, Sitzung beenden, HWID |
| 5 | `support`, `ticket`, `updates` | Views | `npm test`; Ticket anlegen/antworten, Release |
| 6 | `admin`, `activity` | Views | `npm test`; jede Admin-Aktion einmal |
| 7 | Freigegebene neue Seiten aus Abschnitt 3, je eigene Route + ein Render-Test in `tests/features.test.js` | `server.js` (nur neue Routen), Views, `lib/` | `npm test` mit neuem Test |
| 8 | Aufräumen: `style.css`, `design.css`, `modern.css`, `dashboard.css` und alte Partials löschen; Test, dass keine Seite sie mehr lädt; `REDESIGN-SETUP.md` aktualisieren | `public/`, `views/partials/`, `tests/` | `npm test`; `grep` auf alte Dateinamen leer |

### Entscheidungen (2026-11-16)
- Neue Funktionen: **1 Module-Seite, 2 Changelog, 4 FAQ, 5 Lizenz-Erinnerung, 6 Theme-Lab**. Die Status-Seite (3) entfällt.
- Modul-Texte bleiben **englisch** wie im Client.
- Nur das **dunkle** Design (Neon Orchid), keine helle Variante.
- `npm ci` für Schritt 0 ist freigegeben.
