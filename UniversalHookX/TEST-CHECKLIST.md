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
