# SDK structure

- `java.hpp/.cpp`: JVM access and lifetime of the Minecraft/Fabric SDK wrappers.
- `CMinecraft.h/.cpp`: Minecraft JNI cache, initialization, accessors and click worker.
- `CFabric.h/.cpp`: Fabric JNI cache, class-loader lookup, initialization, accessors and click worker.

Headers contain declarations and the existing cache fields; implementation dependencies are local to `.cpp` files. Constructor/destructor bodies, JNI reference operations, initialization defaults and worker stop conditions are preserved by the split.

JNIEnv is thread-local. Keep the existing destruction order: join workers, clean up hooks, release bridges and SDK wrappers, then detach the owning worker thread. This refactor does not change reference ownership or repair unrelated JNI behavior.

Register each implementation file in both Visual Studio project files.
