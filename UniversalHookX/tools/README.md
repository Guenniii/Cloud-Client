# Client build and installation

DLL and bridge JAR remain separate files. Run from the native project directory:

```powershell
.\tools\Build-Client.ps1 -Action build
.\tools\Build-Client.ps1 -Action install -DryRun
.\tools\Build-Client.ps1 -Action install
```

Close Minecraft before installation. Building does not replace the files in `bin`.
A successful build writes both artifacts, logs, source hashes and `manifest.json`
to a unique directory under `obj/client-build`. Installation selects the latest
successful build. A failed build never changes that selection.

The defaults use Release/x64 and the sibling `SeedcrackerX-master` Java project.
Change the Java location with `-JavaRoot "C:\path\SeedcrackerX-master"`. Select a
specific prepared pair with `-Output "C:\path\successful-build-directory"` when
installing. `-DryRun` verifies hashes and target access without installing.

Requirements: Python 3.9+, JDK 25 (`javac` on PATH), Visual Studio C++ tools with
the project's v145 toolset, Vulkan SDK and the project's existing Gradle/Loom
dependency cache. The build uses local dependencies and does not download them.
If dependency setup is missing, run the existing Java project's Gradle setup first.

The Java compiler rebuilds all local Java sources. The existing bridge JAR in
`bin` supplies the established vendored libraries and resources; its hash is
recorded in the manifest. Keep this bootstrap JAR available. If changing vendored
dependency versions or resource packaging, regenerate it with the existing
Gradle `bridgeJar` task first. This helper does not replace Gradle dependency or
resource management.

The native/Java JNI option layout is checked before compiling. Source changes
during the build prevent creation of a ready manifest. Installation validates
artifact hashes, opens both target binaries against concurrent writes, saves a
backup and verifies the written contents. On a write failure it attempts to restore
both originals before releasing the handles. Successful installation records
`installed.json`. Restart Minecraft to load the new Java classes.

Advanced options (Visual Studio/Vulkan location, cache and bootstrap JAR):

```powershell
python .\tools\client_build.py --help
python .\tools\test_client_build.py
```
