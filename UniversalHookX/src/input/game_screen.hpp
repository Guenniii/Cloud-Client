#pragma once
#include "../utils/sdk/jni_safety.hpp"

namespace Input {
// Minecraft 26.2 owns the active screen through Gui.screen(), not a
// Minecraft.screen field. Unknown state blocks hotkeys until the next frame.
inline bool GameScreenOpen(JNIEnv* env, jobject minecraft) {
    if (!env || !minecraft || env->ExceptionCheck()) return true;
    JniSafety::LocalFrame frame(env, 8);
    if (!frame) { env->ExceptionClear(); return true; }
    JniSafety::Lookup api(env);
    jclass minecraftClass = api.GetObjectClass(minecraft);
    jfieldID guiField = api.GetFieldID(minecraftClass, "gui", "Lnet/minecraft/client/gui/Gui;");
    jobject gui = api.GetObjectField(minecraft, guiField);
    if (!gui) return true;
    jclass guiClass = api.GetObjectClass(gui);
    jmethodID screenMethod = api.GetMethodID(guiClass, "screen", "()Lnet/minecraft/client/gui/screens/Screen;");
    if (!screenMethod) return true;
    jobject screen = env->CallObjectMethod(gui, screenMethod);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return true; }
    return screen != nullptr;
}
}
