package phantomui.seedcracker;

import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicBoolean;

// Compiled into seedcracker_client_task_bytes.hpp; no native callbacks.
public final class NativeClientTask implements Runnable {
    private static NativeClientTask instance;
    private final Object minecraft;
    private final Method execute, init, reset, tick, scan;
    private final Class<?> minecraftClass;
    private final AtomicBoolean queued = new AtomicBoolean();
    private volatile boolean enabled;
    private volatile long generation;
    private long appliedGeneration = -1;
    private boolean ready;
    private Object lastWorld;
    private long lastScan;

    private NativeClientTask(Class<?> bridge) throws Exception {
        minecraftClass = Class.forName("net.minecraft.client.Minecraft", true, bridge.getClassLoader());
        minecraft = minecraftClass.getMethod("getInstance").invoke(null);
        execute = minecraftClass.getMethod("execute", Runnable.class);
        init = bridge.getMethod("initCracking");
        reset = bridge.getMethod("resetCracking");
        tick = bridge.getMethod("tickCracking");
        scan = bridge.getMethod("scanChunkForConfirmedShipwrecks", int.class, int.class);
    }

    public static synchronized void request(Class<?> bridge, boolean enabled) throws Exception {
        if (instance == null) instance = new NativeClientTask(bridge);
        if (instance.enabled != enabled) {
            instance.enabled = enabled;
            instance.generation++;
        }
        if (instance.queued.compareAndSet(false, true)) {
            try {
                instance.execute.invoke(instance.minecraft, instance);
            } catch (Exception e) {
                instance.queued.set(false);
                throw e;
            }
        }
    }

    @Override public void run() {
        try {
            long currentGeneration = generation;
            if (appliedGeneration != currentGeneration) {
                if (ready) reset.invoke(null);
                ready = false;
                lastWorld = null;
                lastScan = 0;
                appliedGeneration = currentGeneration;
            }
            if (!enabled) return;
            Object world = minecraftClass.getField("level").get(minecraft);
            Object player = minecraftClass.getField("player").get(minecraft);
            if (world != lastWorld) {
                if (ready) reset.invoke(null);
                ready = false;
                lastWorld = world;
                lastScan = 0;
            }
            if (world == null || player == null) return;
            if (!ready) {
                init.invoke(null);
                ready = true;
                System.out.println("[SeedCracker] Clientthread-Pipeline bereit.");
            }
            tick.invoke(null);
            long now = System.nanoTime();
            if (lastScan != 0 && now - lastScan < 1_000_000_000L) return;
            lastScan = now;
            // Shipwreck data belongs to the Overworld only.
            Object dimension = world.getClass().getMethod("dimension").invoke(world);
            Class<?> levelClass = Class.forName("net.minecraft.world.level.Level", true, minecraftClass.getClassLoader());
            if (!dimension.equals(levelClass.getField("OVERWORLD").get(null))) return;
            double x = ((Number) player.getClass().getMethod("getX").invoke(player)).doubleValue();
            double z = ((Number) player.getClass().getMethod("getZ").invoke(player)).doubleValue();
            if (!Double.isFinite(x) || !Double.isFinite(z)) return;
            int cx = (int) Math.floor(x / 16.0);
            int cz = (int) Math.floor(z / 16.0);
            String hits = (String) scan.invoke(null, cx, cz);
            int count = hits == null || hits.trim().isEmpty() ? 0 : hits.trim().split("\\R").length;
            System.out.println("[SeedCracker] Scan Chunk " + cx + "," + cz + ": " + count + " bestaetigte Wracks");
        } catch (Exception e) {
            System.err.println("[SeedCracker] Clientthread-Auftrag fehlgeschlagen:");
            e.printStackTrace();
        } finally {
            queued.set(false);
        }
    }
}
