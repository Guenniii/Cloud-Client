# Getesteter Projektstand

Datum: 2026-10-01. Minecraft 26.2; gemeinsamer Release/x64-Build.
Der Nutzer bestätigt Menü/ESP auf OpenGL und Vulkan, SeedCracker/OreSim,
Inventar- und Kampfmodule sowie Entladen bei offenem Minecraft als funktionierend.

- C++: der Git-Commit, der diese Datei erstmals hinzufügt.
- Java-Projekt: `../SeedcrackerX-master` relativ zum C++-Repository.
- Java-Commit: `70d978935e3a9dec639bca674076c3bcbd032678`.
- DLL-SHA256: `9a9d50af1df569862e450e4133aaee8d38776957608640e064b02017944d3c8b`.
- Bridge-JAR-SHA256: `79b2dc4fcf4e67ff260119474e1ae983224c6107336fc5c89eb9d783394218d0`.

Die Test-Checkliste steht in `TEST-CHECKLIST.md`; der gemeinsame Build ist in
`tools/README.md` beschrieben. Beide Git-Repositories bleiben lokal.
Quellcodeänderungen nach diesem Stand müssen in beiden Projekten passend
versioniert werden. Die ursprüngliche Bridge-JAR und DLL sind zusätzlich lokal
gesichert; Git versioniert die Binärdateien in `bin` nicht.
