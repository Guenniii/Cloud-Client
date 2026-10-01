#pragma once
#include "../../dependencies/jni/jni.h"
#include <utility>
namespace JniSafety {
template<class F> struct ScopeExit { F fn; ~ScopeExit() { fn(); } };
class LocalFrame {
    JNIEnv* env_; bool active_;
public:
    explicit LocalFrame(JNIEnv* env, jint capacity=128) : env_(env), active_(env && env->PushLocalFrame(capacity)==JNI_OK) {}
    ~LocalFrame() { if(active_) env_->PopLocalFrame(nullptr); }
    explicit operator bool() const { return active_; }
    LocalFrame(const LocalFrame&)=delete;
};
// Lookup failures are optional unless the caller explicitly requires the result.
// Never make another JNI lookup while a Java exception is pending.
class Lookup {
    JNIEnv* e;
    template<class T> T checked(T value) { if(e->ExceptionCheck()) { e->ExceptionClear(); return T{}; } return value; }
public:
    explicit Lookup(JNIEnv* env):e(env) {}
    jclass GetObjectClass(jobject o) { return o?checked(e->GetObjectClass(o)):nullptr; }
    jstring NewStringUTF(const char* s) { return s?checked(e->NewStringUTF(s)):nullptr; }
    jobjectArray NewObjectArray(jsize n,jclass c,jobject o) { return c?checked(e->NewObjectArray(n,c,o)):nullptr; }
    template<class... A> jobject NewObject(jclass c,jmethodID m,A... args) { return c&&m?checked(e->NewObject(c,m,args...)):nullptr; }
    jclass FindClass(const char* n) { return checked(e->FindClass(n)); }
    jfieldID GetFieldID(jclass c,const char* n,const char* s) { return c?checked(e->GetFieldID(c,n,s)):nullptr; }
    jfieldID GetStaticFieldID(jclass c,const char* n,const char* s) { return c?checked(e->GetStaticFieldID(c,n,s)):nullptr; }
    jmethodID GetMethodID(jclass c,const char* n,const char* s) { return c?checked(e->GetMethodID(c,n,s)):nullptr; }
    jmethodID GetStaticMethodID(jclass c,const char* n,const char* s) { return c?checked(e->GetStaticMethodID(c,n,s)):nullptr; }
    jobject GetStaticObjectField(jclass c,jfieldID f) { return c&&f?checked(e->GetStaticObjectField(c,f)):nullptr; }
    jobject GetObjectField(jobject o,jfieldID f) { return o&&f?checked(e->GetObjectField(o,f)):nullptr; }
    jclass GetSuperclass(jclass c) { return c?checked(e->GetSuperclass(c)):nullptr; }
    jsize GetArrayLength(jarray a) { return a?checked(e->GetArrayLength(a)):0; }
    jobject GetObjectArrayElement(jobjectArray a,jsize i) { return a?checked(e->GetObjectArrayElement(a,i)):nullptr; }
    template<class... A> jobject CallObjectMethod(jobject o,jmethodID m,A... args) { return o&&m?checked(e->CallObjectMethod(o,m,args...)):nullptr; }
    template<class... A> jobject CallStaticObjectMethod(jclass c,jmethodID m,A... args) { return c&&m?checked(e->CallStaticObjectMethod(c,m,args...)):nullptr; }
};
}
