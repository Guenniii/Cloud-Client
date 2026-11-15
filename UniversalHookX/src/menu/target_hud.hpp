#pragma once
#include "target_hud_draw.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../utils/sdk/game_runtime.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include <Windows.h>
#include <array>
#include <chrono>
#include <mutex>

namespace Menu::TargetHud {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID frameMethod=nullptr,nameMethod=nullptr,clearMethod=nullptr,headMethod=nullptr;
inline bool active=false;
inline auto retry=std::chrono::steady_clock::time_point{};
inline bool Check(JNIEnv* env){
    if(!env->ExceptionCheck())return true;
    env->ExceptionDescribe();env->ExceptionClear();return false;
}
inline bool Load(JNIEnv* env){
    if(bridge)return true;
    auto now=std::chrono::steady_clock::now();if(now<retry)return false;retry=now+std::chrono::seconds(5);
    if(!SeedCracker::Init(env,"C:/Users/okeba/source/repos/UniversalHookX/UniversalHookX/bin/SeedCrackerBridge.jar"))return false;
    auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.hud.TargetHudBridge");if(!Check(env)||!cls)return false;
    JniSafety::Lookup api(env);
    frameMethod=api.GetStaticMethodID(cls,"frame","(IZZ)[F");
    nameMethod=api.GetStaticMethodID(cls,"name","()Ljava/lang/String;");
    clearMethod=api.GetStaticMethodID(cls,"clear","()V");
    headMethod=api.GetStaticMethodID(cls,"head","()[I");
    if(!frameMethod||!nameMethod||!clearMethod||!headMethod)return false;
    bridge=static_cast<jclass>(env->NewGlobalRef(cls));return Check(env)&&bridge;
}
inline void Reset(JNIEnv* env){
    if(active&&env&&bridge){env->CallStaticVoidMethod(bridge,clearMethod);Check(env);}
    active=false;ClearPresentation();
}
inline void Shutdown(JNIEnv* env){
    std::lock_guard lock(mutex);Reset(env);
    if(env&&bridge)env->DeleteGlobalRef(bridge);
    bridge=nullptr;frameMethod=nameMethod=clearMethod=headMethod=nullptr;retry={};
}
inline void Render(){
    std::lock_guard lock(mutex);
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    const bool visible=TargetHUD_Enabled&&pid==GetCurrentProcessId();
    // Acquire JNI only while enabled or when an existing observer needs cleanup.
    if(!visible&&!active){ClearPresentation();return;}
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env||!Check(env)){ClearPresentation();return;}
    if(!visible){Reset(env);return;}
    JniSafety::LocalFrame locals(env,16);if(!locals||!Load(env))return;
    const bool preview=Menu_Enabled&&TargetHUD_Preview;
    if(Menu_Enabled&&!preview){Reset(env);return;}
    active=true;
    auto values=static_cast<jfloatArray>(env->CallStaticObjectMethod(bridge,frameMethod,
        jint(std::clamp(TargetHUD_Hold.load(),500,5000)),jboolean(AntiBot_Enabled.load()),jboolean(preview)));
    if(!Check(env)||!values){ClearPresentation();return;}
    const auto count=env->GetArrayLength(values);
    if(count==0){
        if(preview){Snapshot fallback{"Preview",16,20,4,15,3,0,-1,true};Present(&fallback);}
        else Present(nullptr);
        return;
    }
    if(count!=8){ClearPresentation();return;}
    std::array<jfloat,8> data{};env->GetFloatArrayRegion(values,0,8,data.data());if(!Check(env))return;
    for(auto value:data)if(!std::isfinite(value)){ClearPresentation();return;}
    if(data[0]<0||data[0]>16000000||data[1]<0||data[1]>16000000||data[3]<=0||data[3]>1000000)return;
    auto name=static_cast<jstring>(env->CallStaticObjectMethod(bridge,nameMethod));if(!Check(env)||!name)return;
    auto label=GameRuntime::String(env,name);if(label.empty()||label.size()>128)return;
    Snapshot snapshot{label,std::clamp(data[2],0.f,data[3]),data[3],std::clamp(data[4],0.f,1000000.f),
        std::clamp(data[5],0.f,20.f),std::clamp(data[6],0.f,10000.f),int(data[0]),int(data[1]),preview};
    auto head=static_cast<jintArray>(env->CallStaticObjectMethod(bridge,headMethod));
    if(Check(env)&&head&&env->GetArrayLength(head)==64){
        std::array<jint,64> pixels{};env->GetIntArrayRegion(head,0,64,pixels.data());
        if(Check(env)){
            for(int i=0;i<64;i++){auto argb=static_cast<unsigned int>(pixels[i]);
                snapshot.head[i]=IM_COL32((argb>>16)&255,(argb>>8)&255,argb&255,255);}
            snapshot.hasHead=true;
        }
    }
    Present(&snapshot);
}
}
