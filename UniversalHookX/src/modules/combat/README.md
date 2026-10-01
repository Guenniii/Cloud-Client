# Combat module organization

`Reach`, `HitCrystal` and `CwCrystal` expose their declarations and member state in `.hpp` files; method implementations live in the matching `.cpp` files. Headers forward-declare `CMinecraft`. JNI calls, Windows input, SDK access and worker implementation dependencies belong to the implementation files.

This split preserves method bodies, access levels, field initialization and constructor initialization. It does not change JNI reference ownership, targeting logic, timing or worker shutdown. `CwCrystal` still uses Lifecycle-managed workers.

Register new `.cpp` files in both Visual Studio project files. `cw.cpp` now owns only the CW crystal worker; independent runtime adapters coordinate the other features.
