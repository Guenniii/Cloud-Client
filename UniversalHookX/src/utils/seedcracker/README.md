# Seedcracker Stufe 1 – Proof of Concept

Ziel dieser Stufe: beweisen, dass ihr per JNI aus eurem C++-Client heraus
zusaetzliche Java-Klassen in die laufende Minecraft-JVM laden und darüber
Live-Chunk-Daten auslesen könnt – exemplarisch an Schiffswrack-Chest-Kandidaten.

**Was hier bewusst NICHT drin ist (kommt in Stufe 2/3):**
- Biome-Filter
- Die genaue Schiffswrack-Geometrie-Validierung
- Das eigentliche Seed-Cracking (`mc_feature` / `mc_reversal` / `latticg` / `SeedCracker`-Singleton)

## 1. Bridge-Jar bauen

`java/phantomui/seedcracker/SeedCrackerBridge.java` braucht beim Kompilieren
Minecrafts (deobfuskierte) Klassen als Classpath. Am einfachsten:

1. Kopiert die Datei in einen `src/main/java/phantomui/seedcracker/`-Ordner
   innerhalb eures geklonten SeedcrackerX-Gradle-Projekts (das hat den
   kompletten Loom/Mapping-Classpath schon fertig konfiguriert).
2. `./gradlew build` – das erzeugt eure Bridge-Klasse mit im Standard-Jar
   (oder baut euch ein eigenes, kleines Gradle-Modul nur für diese eine Klasse,
   falls ihr nicht den ganzen SeedcrackerX-Code mitbauen wollt).
3. Das Ergebnis-Jar (mind. `phantomui/seedcracker/SeedCrackerBridge.class`
   enthaltend) auf die Festplatte legen, z. B. `seedcracker_bridge.jar`.

## 2. Wichtige Voraussetzung: Mapping-Kompatibilität

`SeedCrackerBridge.java` nutzt Klassen-/Methodennamen wie
`net.minecraft.client.Minecraft`, `net.minecraft.world.level.Level` usw. –
das sind die **von Fabric zur Laufzeit remappten** (deobfuskierten) Namen.

**Das funktioniert nur, wenn der Minecraft-Client, in den ihr injiziert,
über den Fabric-Loader läuft** (der remapped die Klassen beim Start).
Läuft der Client vanilla/ohne Loader, sind die echten Namen zur Laufzeit
obfuskiert (z. B. `fxz`, `dqe`, ...) und dieser Code findet die Klassen nicht.



## 3. C++ einbinden

```cpp
#include "seedcracker_bridge.hpp"

// Einmalig beim Aktivieren des Moduls:
JNIEnv* env = SeedCracker::GetMinecraftJNIEnv( );
bool ok = SeedCracker::Init(env, "C:\\pfad\\zu\\seedcracker_bridge.jar");

// Auf Knopfdruck / per Timer (NICHT jeden Frame - blockiert 5-15ms):
auto hits = SeedCracker::ScanPlayerChunkForShipwreckCandidates(env);
for (auto& hit : hits) {
    // hit.x, hit.y, hit.z - erste rohe Chest-Kandidaten, ungefiltert nach Biome/Form
}
```

## 4. Nächste Schritte (Stufe 2)

- Biome-Check ergänzen (`Features.SHIPWRECK.isValidBiome`, braucht `mc_feature`
  als zusätzliche Dependency im Bridge-Jar)
- Die Geometrie-Validierung aus `ShipwreckFinder#onChestFound` übernehmen
  (unterscheidet "ist wirklich ein Schiffswrack" von "zufällige Truhe")
- `mc_feature`, `mc_reversal`, `latticg` als Dependencies einbinden und eine
  eigene, schlanke Version von `SeedCracker`/`DataStorage` bauen, die die
  gefundenen Positionen sammelt und den Seed eingrenzt
- Neue Menü-Seite (gleiches Muster wie Config/Settings) für Fortschritt/Ergebnis
