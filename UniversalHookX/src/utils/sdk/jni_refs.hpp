#pragma once
#include "../../dependencies/jni/jni.h"

namespace JniRefs {
// Consume a local reference and return an owned global reference for cross-thread use.
// The local is released even if allocating the global reference fails.
inline jobject PromoteLocal(JNIEnv* env, jobject local) {
    if (!env || !local) return nullptr;
    jobject global = env->NewGlobalRef(local);
    env->DeleteLocalRef(local);
    return global;
}
}
