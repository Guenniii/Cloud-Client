# Plan: PhantomAccount – Desktop-Account-App

Stand: 2026-11-13. Reiner Plan, nichts gebaut oder getestet. Grundlage ist der Arbeitsstand von `C:\Users\okeba\source\repos\UniversalHookX` (viele uncommittete Änderungen).

Ziel: eigenständige Windows-EXE mit Login, Account-Seite, Einstellungen, HWID-Reset-Anfrage und der Anzeige „Minecraft läuft / läuft nicht“.
Nicht-Ziel: Injektion, Starten oder Verändern von Minecraft, Code aus `injector.cpp`.

## Entscheidungen (2026-11-13)

- **Produktname in der App:** „Cloud“ (Fenstertitel, Login-Karte). Ordner und EXE heißen weiterhin `PhantomAccount`.
- **Signatur:** Erstmal unsigniert. Ein Signier-Schritt kann später ergänzt werden; SmartScreen-/AV-Warnungen beim ersten Start sind möglich.
- **HWID-Reset:** Manuelle Freigabe. Der Server legt die Anfrage als `OK|PENDING` an, ein Admin gibt sie frei; `OK|DONE` entfällt.

## 0. Befunde aus dem Code

| Thema | Befund | Stelle |
|---|---|---|
| Login-Endpunkt | `POST /login` auf `auth.cloudclient.vip`, Port 80, ohne TLS. Body: `user=…&hash=<SHA-256 hex>&hwid=<SHA-256 hex>` | `UniversalHookX/src/auth/Authclient.cpp:70-100` |
| Antwort | `OK`, `OK|days`, `OK|days|expiry`, `OK|days|expiry|username`; sonst Fehler-Token | `Authclient.cpp:104-176`, `src/auth/README.md` |
| Fehler-Token | `FAIL`, `LICENSE_INVALID`, `HWID_MISMATCH`, `NO_LICENSE`, `License expired`, `User not Found`, `This User is banned` | `src/menu/login.cpp:330-346` |
| Avatar | `GET /profile/avatar?user=<kodiert>` auf `dashboard.cloudclient.vip:443`, HTTPS mit Timeouts. Akzeptiert nur PNG 256×256 RGBA bis 512 KB. | `Authclient.cpp:179-205`, `src/auth/avatar_policy.hpp` |
| HWID | SHA-256 aus Volume-Seriennummer `C:\`, CPUID Leaf 0 und Registry-`MachineGuid` | `src/auth/HWID.cpp` |
| HWID-Reset | **Kein Endpunkt.** Die Suche nach `hwid`/`reset`/Hostnamen in `src/` (ohne `dependencies`) und im Injector-Repo findet nur die zwei Endpunkte oben. | – |
| Toter Code | `LoginRequest()` gegen `127.0.0.1:8080` hat keinen Aufrufer. | `Authclient.cpp:11-68` |
| Login-Bypass | Uncommittet: `logged_in = true` und ein fest eingetragener Testname. | `login.cpp:25,30` |
| Gehärtete Variante | Timeouts, RAII-Handles, URL-Kodierung, Größenlimit, HWID-Puffer initialisiert, Parser mit `from_chars`, Tests | `C:\Users\okeba\source\repos\injector\injector\{AuthClient.cpp,HWID.cpp,AuthResponse.cpp,tests\auth_response_tests.cpp}` |
| Build-Eckdaten Client | Toolset v145, `stdcpplatest`, `CharacterSet=MultiByte`, Define `ImTextureID=ImU64`, kein `/utf-8` | `UniversalHookX/UniversalHookX.vcxproj:32,105-161` |
| ImGui-Backends | Vendored sind `imgui_impl_win32`, `imgui_impl_opengl3` (mit Loader) und `imgui_impl_vulkan`. **Kein DX11-Backend.** | `src/dependencies/imgui/` (nur Dateinamen gelistet) |

## 1. Technik

**Option A (Empfehlung): C++ mit ImGui, Win32 + OpenGL3, vcxproj v145 x64.**
- Auth, HWID, Theme, Widgets und Login-Zeichnung werden wiederverwendet. Die HWID-Berechnung ist dann garantiert bytegleich.
- Beide Backends sind schon vendored, es kommt keine neue Abhängigkeit dazu. Das Vulkan SDK wird nicht gebraucht.
- Nutzer der App haben eine OpenGL-fähige GPU, sonst liefe Minecraft nicht.
- Grenzen:
  - ImGui hat keine Screenreader-Unterstützung. Tastaturbedienung kommt über `ImGuiConfigFlags_NavEnableKeyboard`; Enter zum Absenden ist in `login.cpp:277` schon da.
  - Das Layout nutzt feste Pixelwerte. Die App startet deshalb DPI-unaware, Windows skaliert und das Bild ist auf HiDPI leicht unscharf.
  - In RDP-Sitzungen oder VMs ohne OpenGL 3 scheitert der Start. Dann MessageBox und Ende. DX11 wäre der spätere Ausweg, bräuchte aber `imgui_impl_dx11.*` passend zur vendored ImGui-Version.

**Option B: C#/WPF (.NET 8).**
- Vorteile: native Controls, Barrierefreiheit, DPI, `ProtectedData` und Credential Manager eingebaut.
- Nachteile: Auth, HWID, Theme und Design müssen neu geschrieben werden. .NET-Runtime oder eine EXE über 60 MB.
- Hauptrisiko ist der HWID-Port. `std::to_string` der Seriennummer, vier vorzeichenbehaftete CPUID-Ints und der GUID-String müssen bytegleich sein. Eine Abweichung führt zu `HWID_MISMATCH` oder bindet beim Erst-Login eine falsche HWID.

Die vorhandene GDI+-Oberfläche des Injectors (`injector\injector\CloudInjector\src\main.cpp`) ist keine Option. Sie ist mit Injektionslogik verwoben und nutzt Theme und Widgets des Clients nicht.

## 2. Neuer Ordner `C:\Users\okeba\source\repos\PhantomAccount`

```
PhantomAccount\
  PhantomAccount.sln
  PhantomAccount.vcxproj (+ .filters)
  README.md             Build, Hinweis auf geteilte Client-Dateien
  .gitignore            bin/, obj/, .vs/, *.user
  assets\
    app.rc, app.ico, app.manifest
  src\
    main.cpp            Fenster, WGL-Kontext, ImGui-Init, Frame-Schleife, Ansichtswechsel, --selftest
    app.hpp             Sitzung (Name, LicenseInfo, Hash im Speicher), aktive Ansicht
    resources.cpp/.hpp  Fonts, GL-Textur aus Speicher, Avatar-Future
    strings.hpp         DE/EN-Tabelle
    settings.hpp        settings.cfg laden/speichern, Credential Manager
    minecraft_status.hpp
    hwid_reset.hpp      Platzhalter
    views\
      login_view.cpp    aus login.cpp
      account_view.cpp
      settings_view.cpp
```

- **Build: vcxproj statt CMake.** Der Client nutzt vcxproj, und die Einstellungen müssen übereinstimmen.
- **Einstellungen:**
  - Toolset v145, x64, Debug und Release, `stdcpplatest`, `CharacterSet=MultiByte`, Define `ImTextureID=ImU64`.
  - Zusätzlich `/utf-8` wegen der Umlaute in ImGui-Strings; der Client hat das nicht.
  - `SubSystem=Windows`, Libs `opengl32.lib` und `dwmapi.lib`. `winhttp` und `advapi32` kommen per `#pragma` aus den Auth-Dateien.
  - `OutDir=bin\`, `IntDir=obj\`.
- **Vendored Deps:** kein Kopieren. Eine MSBuild-Property `ClientSrc=$(SolutionDir)..\UniversalHookX\UniversalHookX\src\` zeigt auf den Client.
- **Kompiliert aus `$(ClientSrc)dependencies\imgui\`:** `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imgui_impl_win32.cpp`, `imgui_impl_opengl3.cpp`. `imgui_demo.cpp` nur, falls der Linker es verlangt.
- **Fallback für einen eigenständigen Build:** `dependencies\{imgui,font,images}` nach `PhantomAccount\vendor\` kopieren. Erst nötig, wenn das Repo ohne Client-Checkout bauen soll.

## 3. Übernahme aus dem Client

Empfehlung: **per relativem Pfad teilen, nicht kopieren.** Der Auth-Code existiert schon in mehreren auseinandergelaufenen Kopien (Client, `injector\injector`, `injector\test`, `injector\Backuo`). Die relativen Includes in den Client-Dateien lösen sich im Client-Baum von selbst auf.

Preis dafür: Die App baut nur neben dem Client-Checkout, und Änderungen an geteilten Dateien erfordern den Build beider Projekte. Ein Submodule lohnt nicht, weil es das ganze Client-Repo hereinzöge.

**Unverändert geteilt** (unter `C:\Users\okeba\source\repos\UniversalHookX\UniversalHookX\src\`):

| Datei | Abhängigkeiten | Hinweis |
|---|---|---|
| `auth/Authclient.cpp`, `.h` | WinHTTP, `HWID.h`, `avatar_policy.hpp` | Keine Menu- oder Theme-Globals. `ParseLoginResponse` nutzt `std::stoi` und wirft bei `OK|abc`, der Aufrufer muss fangen. |
| `auth/HWID.cpp`, `.h`, `auth/Hash.cpp`, `.h` | advapi32 | Unverändert, damit die HWID identisch bleibt. |
| `auth/avatar_policy.hpp` | nur std | Ungetrackt im Client. |
| `utils/theme/theme.hpp`, `share_code.hpp`, `sharing.hpp` | `imgui.h`, std | Globals sind `inline`. `ThemeSharing::Export/Import` dient als Theme-Persistenz. Die beiden letzten sind ungetrackt. |
| `menu/palette.hpp`, `menu/widgets.hpp`, `.cpp`, `menu/custom_color_picker.cpp` | Theme, `imgui_internal.h`, `modules/color_rgb.hpp` | Kein Bezug zu JNI, Modulen oder Hooks. |
| `modules/color_rgb.hpp` | nur std | – |
| `dependencies/imgui/*`, `font/*`, `images/bytearray.h`, `images/stb_image.h` | – | Nur kompilieren und einbinden, nicht ändern. |

**Kopieren und anpassen:**

| Quelle | Ziel | Was entkoppelt werden muss |
|---|---|---|
| `menu/login.cpp` | `src/views/login_view.cpp` | Siehe Liste unten. |
| `menu/resources.cpp` | `src/resources.cpp` | Siehe Liste unten. |

Anpassungen an `login_view.cpp`:
- `Lifecycle::Spawn` (`:321`) durch `std::async` ersetzen.
- `Resources::*` auf die App-Version umbiegen.
- `GetSystemMetrics`-Positionierung (`:412-415`) ersetzen: das ImGui-Fenster füllt den Client-Bereich.
- `logged_in` startet mit `false`, der Testname entfällt.
- Feste englische Texte (`:213-370`) kommen aus `strings.hpp`.
- Ein Ergebnis-Callback ersetzt die dateistatischen Getter. Der Zustand wird beim Logout zurückgesetzt.
- Das Passwortfeld nach dem Hashen mit `SecureZeroMemory` leeren.

Anpassungen an `resources.cpp`:
- `menu.hpp`, `<vulkan/vulkan.h>`, `imageloader.hpp`, `gif.h` und `lifecycle.hpp` entfallen.
- `SetupStyleAndFonts` (`:44-95`) und die Avatar-Logik (`:98-115`) bleiben.
- Ein eigener GL-Loader mit rund 15 Zeilen kommt dazu: stb_image plus `glTexImage2D`, Vorbild `utils/imageloader.cpp:24-59`. `STB_IMAGE_IMPLEMENTATION` gehört in genau eine TU der App.
- Die vier Symbole `tab_title`, `font_icon`, `poppins`, `icons` müssen definiert werden. Laut Kommentar in `menu/resources.hpp:5` referenziert das angepasste ImGui sie.

**Nicht übernehmen:** `utils/imageloader.*` (Vulkan-Globals), `utils/lifecycle/lifecycle.hpp` (Unload-Logik der DLL), `menu/pages/theme_page.cpp` (hängt an `pages.hpp`), alles aus dem Injector-Repo außer als Vorlage.

## 4. Schritte

Jeder Schritt wird so gebaut:
`msbuild C:\Users\okeba\source\repos\PhantomAccount\PhantomAccount.sln /p:Configuration=Release /p:Platform=x64`

`PhantomAccount.exe --selftest` prüft reine Funktionen per `assert`-ähnlichen Checks und liefert Exit-Code 0 oder 1. Es gibt keine Konsolenausgabe, weil die EXE im Windows-Subsystem läuft.

**(a) Fenster und Design**
- Bau:
  - vcxproj wie in Abschnitt 2.
  - `main.cpp` nach dem ImGui-Beispiel Win32 + OpenGL3: festes, nicht skalierbares Fenster von etwa 460×600 mit normalem Rahmen.
  - Dunkle Titelleiste per `DwmSetWindowAttribute`, VSync, kein Rendern im minimierten Zustand, `io.IniFilename=nullptr`.
  - Fonts und Logo aus `resources.cpp`, `Theme::ApplyStyle()` pro Frame.
  - Leere Login-Karte mit Logo-Intro aus `login_view.cpp`, noch ohne Request.
- Dateien: `PhantomAccount.sln`, `.vcxproj`, `assets\*`, `src\main.cpp`, `src\resources.*`, `src\views\login_view.cpp`.
- Abnahme:
  - Build ohne Fehler. Das zeigt zugleich, ob das angepasste ImGui weitere Client-Symbole braucht.
  - Fenster zeigt Karte, Logo-Animation und Poppins-Schrift.
  - Nach dem Schließen läuft kein Prozess weiter.
  - Leerlauf-CPU bleibt niedrig, GPU nicht auf 100 %.

**(b) Login**
- Bau:
  - Request wie `login.cpp:315-319`: `LoginRequestWithResponse(user, Sha256(pass), GetHWID())`, dann `ParseLoginResponse`.
  - Läuft in `std::async`. Abfrage pro Frame mit `try/catch` wie `login.cpp:53-75`.
  - Fehler-Mapping aus `login.cpp:330-346`. Eine leere Antwort wird zu „Server nicht erreichbar“.
  - Beim Beenden mit laufendem Request `ExitProcess`, weil der Request bis Phase 0 keine Timeouts hat.
- Dateien: `login_view.cpp`, `app.hpp`, `main.cpp`.
- Abnahme:
  - Gültiger Account wechselt zur (noch leeren) Account-Ansicht.
  - Falsches Passwort zeigt die Meldung, die Eingabe bleibt möglich.
  - Bei getrenntem Netz kommt eine Fehlermeldung, die UI friert nicht ein, und das Fenster lässt sich sofort schließen.
  - Selftest: Parser-Fälle aus `injector\injector\tests\auth_response_tests.cpp`, soweit sie zum Client-Parser passen.

**(c) Account-Seite**
- Bau:
  - Name über `LicenseInfo::DisplayName`.
  - Avatar rund per `AddImageRounded`, Vorbild `menu.cpp:860`. `DownloadUserAvatar` läuft im Worker, der Textur-Upload im UI-Thread, Fallback ist das eingebettete Standardbild.
  - Lizenz: `daysLeft == -1` zeigt „Unbegrenzt“, sonst „N Tage übrig“ und „Läuft ab am `<expiryDate>`“ (Server-String unverändert). Dazu „Stand: Uhrzeit des Logins“.
  - „Abmelden“ setzt die Sitzung zurück und führt zum Login.
- Dateien: `views\account_view.cpp`, `resources.cpp`, `app.hpp`.
- Abnahme:
  - Account mit Avatar zeigt das Bild, ohne Avatar das Standardbild.
  - Befristete und permanente Lizenz werden korrekt angezeigt.
  - Abmelden und erneuter Login funktionieren ohne Neustart.

**(d) Einstellungen**
- Bau:
  - **Login merken:** Windows Credential Manager mit `CredWriteW`, `CRED_TYPE_GENERIC`, Ziel `PhantomAccount:login`, `CRED_PERSIST_LOCAL_MACHINE`. Gespeichert werden Benutzername und SHA-256-Hash als Blob, nie das Klartext-Passwort, nie in einer Datei.
    - Beim Start: `CredReadW` erfolgreich bedeutet Auto-Login.
    - Bei `FAIL`, beim Abmelden oder beim Ausschalten: `CredDeleteW`.
    - Grenze: Jeder Prozess desselben Windows-Benutzers kann den Eintrag lesen, und der Hash ist für diesen Server passwortgleich. Sauberer wäre ein widerrufbares Server-Token, siehe Abschnitt 5 Phase 4.
  - **Theme:** Preset-Auswahl aus `Theme::presets` (13 Stück), Akzentfarbe über `Widgets::DrawColorRow`, Schalter für `custom_accent_enabled`.
  - **Sprache:** DE oder EN. Standard aus `GetUserDefaultUILanguage`.
  - **Speicherort:** `%APPDATA%\PhantomAccount\settings.cfg` mit `settings_version=1`, `language=de|en`, `theme=<ThemeSharing::Export()>`.
    - Schreiben atomar nach dem Muster `utils/config/config.cpp:113-116`: `.tmp` schreiben, dann `MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH)`.
    - Laden über `ThemeSharing::Import`. Ein ungültiger Wert fällt auf die Standardwerte zurück.
- Dateien: `views\settings_view.cpp`, `settings.hpp`, `strings.hpp`.
- Abnahme:
  - „Merken“ an, App neu starten: Auto-Login. Der Eintrag ist in der Windows-Anmeldeinformationsverwaltung sichtbar. Nach dem Abmelden ist er weg.
  - `settings.cfg` enthält weder Passwort noch Hash.
  - Theme und Sprache überleben einen Neustart.
  - Eine beschädigte `settings.cfg` führt zum Start mit Standardwerten.
  - Selftest: Roundtrip der Einstellungen, jede String-ID hat DE und EN.

**(e) HWID-Reset anfragen**
- Bau:
  - Button auf der Account-Seite.
  - **Zusätzlich auf der Login-Karte bei `HWID_MISMATCH`.** Genau dort wird er gebraucht, denn ohne Login erreicht man die Account-Seite nicht.
  - Zwei-Klick-Bestätigung.
  - `hwid_reset.hpp` liefert bis zum Server-Endpunkt fest „noch nicht verfügbar“. Die UI zeigt „Noch nicht verfügbar – bitte Support kontaktieren“.
  - Der echte WinHTTP-Aufruf kommt erst, wenn der Endpunkt existiert. Dann nur HTTPS, Muster `DownloadUserAvatar`.
- Dateien: `hwid_reset.hpp`, `account_view.cpp`, `login_view.cpp`.
- Abnahme: Button sichtbar, zweiter Klick zeigt den Platzhalter. Ein Netzwerk-Mitschnitt zeigt keinen Request.

**(f) Minecraft-Status**
- Bau:
  - Alle 3 s im UI-Thread: `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` nach `javaw.exe` oder `java.exe`.
  - Dann `EnumWindows`: sichtbares Top-Level-Fenster dieser PID, dessen Titel mit „Minecraft“ beginnt. Vorbild für die reine Aufzählung ist `injector\injector\CloudInjector\src\main.cpp:166-191`.
  - **Kein `OpenProcess`, kein Lesen der Kommandozeile.**
  - Anzeige als Pill „Minecraft läuft“ oder „Minecraft läuft nicht“.
- Dateien: `minecraft_status.hpp`, `account_view.cpp`.
- Abnahme:
  - Minecraft 26.2 starten: Status wechselt binnen 3 s. Beenden: wechselt zurück.
  - Nur der Launcher offen: „läuft nicht“.
  - Eine andere Java-Anwendung offen: „läuft nicht“.
  - Selftest: Titel-Prüffunktion mit Beispieltiteln.

### Was der Server für (e) bräuchte

- **Endpunkt:** `POST /hwid-reset` auf dem Auth-Host, von Beginn an nur HTTPS.
- **Parameter** (form-urlencoded): `user`, `hash` (wie bei `/login`), `hwid` (HWID des anfragenden Geräts).
- **Auth:** dieselbe Prüfung von Benutzer, Hash, Lizenz und Bann wie bei `/login`, aber **ohne HWID-Vergleich**.
- **Antwort** (`text/plain`, im bestehenden Pipe-Stil):
  - `OK|PENDING` (Anfrage angelegt) oder `OK|DONE` (bei automatischer Freigabe)
  - `ALREADY_PENDING`
  - `RATE_LIMITED|<yyyy-mm-dd>` (nächster möglicher Termin)
  - Bestehende Token `FAIL`, `LICENSE_INVALID`, `This User is banned`
- **Rate-Limit**, serverseitig durchgesetzt: z. B. eine Anfrage pro Account in 30 Tagen, zusätzlich pro IP etwa 5 pro Stunde.
- **Protokoll:** Account, alte HWID, angefragte HWID, IP, Zeit, Status. Eine wiederholte Anfrage darf nichts doppelt anlegen.
- **Entschieden: manuelle Freigabe.** Antwort ist immer `OK|PENDING`; die App zeigt „Anfrage gesendet – wird geprüft“. Bei Freigabe durch den Admin entweder die Bindung löschen, sodass der nächste Login neu bindet, oder direkt auf die mitgesendete HWID setzen (empfohlen: die mitgesendete HWID, dann kann niemand anderes dazwischen binden).

## 5. Login-Sicherheit (Audit Punkt 16) – Empfehlung

Grundsatz: Kein Schritt darf die berechnete HWID oder das Wire-Format von `/login` ändern. Reihenfolge immer Server vor Client.

**Phase 0 – nur Clients, ohne Server-Änderung.** In der geteilten `src/auth/Authclient.cpp`; Client und App profitieren beide.
- `LoginRequestWithResponse` aus `injector\injector\AuthClient.cpp:20-44` übernehmen:
  - `WinHttpSetTimeouts(5000,5000,10000,10000)`
  - RAII-Handles mit NULL-Prüfung
  - URL-Kodierung der Felder. Für Hash und HWID ändert sich nichts, für Namen aus `[A-Za-z0-9-_.~]` ebenfalls nicht.
  - Antwort auf 4096 Byte begrenzen
- **Die Statuscode-Prüfung dort (`:41-42`) nicht ungeprüft übernehmen.** Sie lässt nur drei Fehler-Token bei Nicht-200 durch. `License expired`, `User not Found`, `NO_LICENSE` und `This User is banned` würden zu `INVALID_RESPONSE`, falls der Server sie mit 4xx sendet.
- HWID-Bugfix in `HWID.cpp:25-39`: `char value[64]{}` und Rückgabewerte prüfen. Schlägt das Lesen der Registry fehl, Login mit klarer Meldung abbrechen. Die bisherige HWID war in diesem Fall ohnehin Zufall.
- **`injector\injector\HWID.cpp:9` nicht übernehmen.** Dort führt ein Fehler bei `GetVolumeInformationA` zu einer leeren HWID. Bisher ergab das stabil `"0"`; die Änderung würde Rechner ohne `C:\` aussperren.
- Laufwerk `C:\` bleibt.
- Toten `LoginRequest()` (`Authclient.cpp:11-68`) löschen.
- Prüfung: `.\tools\Build-Client.ps1 -Action build -NoInstall`, dann Login im Client und in der App mit einem Bestandsaccount. `TEST-CHECKLIST.md` hat bisher keinen Login-Punkt und sollte einen bekommen.

**Phase 1 – Server.**
- HTTPS auf `auth.cloudclient.vip:443` mit gültigem Zertifikat und demselben `/login`-Handler. Für `dashboard.cloudclient.vip` existiert das schon.
- Port 80 läuft unverändert weiter.
- Rate-Limit auf `/login` pro IP und pro Account.

**Phase 2 – Clients.**
- Neue Builds von Client, App und Injector nutzen nur noch Port 443 mit `WINHTTP_FLAG_SECURE`.
- **Kein HTTP-Fallback im Code.** Ein Fallback wäre ein Downgrade-Angriffspunkt; alte Builds reden einfach weiter mit Port 80.
- Die App kann von der ersten Veröffentlichung an nur HTTPS sprechen, sobald Phase 1 steht.
- Port 80 erst schließen, wenn die Server-Logs dort keine Logins mehr zeigen.

**Phase 3 – Server, ohne Client-Änderung.**
- Den empfangenen SHA-256 nicht roh speichern, sondern `argon2id(sha256)` oder bcrypt mit Salt.
- Das geht als einmalige Migration über die vorhandenen Werte: kein Nutzer muss sich neu anmelden, keine Lizenz wird gesperrt.
- Die Clients senden weiter SHA-256.

**Phase 4 – optional, Server zuerst.**
- Widerrufbares Sitzungs-Token für „Login merken“ statt des gespeicherten Hashes.
- HWID v2 ohne festes Laufwerk nur als Zusatzfeld `hwid2`, das der Server bei erfolgreichem v1-Login mitbindet. Umstellung erst nach einer Übergangszeit.

## 6. Risiken und offene Fragen

- **Server-Code liegt nicht im Repo** (`src/auth/README.md:27-29`). Unbekannt sind: URL-Dekodierung im Body, HTTP-Statuscodes der Fehler-Token, HTTPS-Fähigkeit des Auth-Hosts, vorhandenes Rate-Limit, Art der HWID-Bindung.
- **Angepasstes ImGui:** Versionsstand und weitere Abhängigkeiten zu Client-Symbolen sind unbekannt, weil der Ordner nicht gelesen werden darf. Das zeigt sich beim Linken in Schritt (a).
- **Geteilte Dateien sind uncommittet oder ungetrackt:** `Authclient.*`, `theme.hpp`, `avatar_policy.hpp`, `share_code.hpp`, `sharing.hpp`. Sie sollten vor Projektstart im Client committet werden, was laut `CLAUDE.md` einen ausdrücklichen Auftrag braucht.
- **Login-Bypass** in `login.cpp:30`: vor einem Client-Release klären.
- **Legacy-Antwort `OK`:** Der Client-Parser macht daraus „PERMANENT“ (`Authclient.cpp:120-125`). Die Account-Seite zeigte dann „Unbegrenzt“ ohne Bestätigung vom Server. Der Injector-Parser unterscheidet das über `hasLicenseDetails`. Offen ist, ob der Live-Server immer vier Felder sendet.
- **Avatar-Endpunkt ist unauthentifiziert** und nimmt nur den Namen. Die App ändert daran nichts.
- **Unsignierte EXE (entschieden: erstmal unsigniert):** Prozessliste plus HTTP kann SmartScreen- oder AV-Warnungen auslösen.
- **Minecraft-Erkennung über den Fenstertitel:** Clients mit eigenem Titel (z. B. Lunar) werden nicht erkannt. Im Vollbild oder mit versteckter Taskleiste funktioniert sie.

## 7. Bewusst weggelassen

- Automatische Aktualisierung des Lizenzstatus; der Stand ist der Login-Zeitpunkt.
- Import von Theme-Codes als UI. `ThemeSharing::Import` ist vorhanden und in wenigen Zeilen nachrüstbar.
- Eigene randlose Titelleiste, Tray-Icon, Einzelinstanz-Sperre, Auto-Update, Installer.
- DPI-Skalierung des Layouts.

## 8. Nicht geprüft

- Inhalt von `src/dependencies/` (ImGui-Version, Anpassungen, `bytearray.h`). Die Symbolnamen stammen aus `menu/resources.cpp`.
- Verhalten des Live-Servers. Es wurde kein Request gesendet.
- Kein Build und kein Test ausgeführt. Alle Abnahmetests sind Vorgaben, keine Ergebnisse.
- `utils/theme/share_code.hpp` nur über Includes und Kopfzeilen eingesehen; die Validierung in `Decode` wurde nicht geprüft.
- `menu/custom_color_picker.cpp` nur über die Includes geprüft.
- `injector\injector\CloudInjector\src\main.cpp` nur Zeilen 160-195. `C:\Users\okeba\source\repos\CloudInjector` und `fusion-plus` nur per Suche durchsucht.
- `TEST-CHECKLIST.md` und `CHECKPOINT.md` nur nach Login-Stichworten durchsucht, ohne Treffer.
