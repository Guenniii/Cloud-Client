# Stability fixes

Minecraft/Fabric singleton instances are now owned global JNI references, safe to pass to the existing attached worker threads. Temporary singleton local references are consumed after promotion. SDK destruction releases the instance and the previously omitted Minecraft end-crystal reference. Failed thread attachment no longer dereferences a null JNIEnv.

Login requests capture input snapshots. The worker publishes its result through a packaged_task/future; UI strings and status are updated on the render thread only. The existing Lifecycle worker joining remains in effect during unload.

Scope: these fixes do not establish that every JNI lookup, partial initialization/retry path, shared setting, or graphics resource has been audited. SDK destructors still rely on the existing owner-thread cleanup before JNI detachment. Validate initialization, affected modules and unload in game.
