#pragma once
#include "seed_settings.hpp"
#include "../seedcracker/seedcracker_bridge.hpp"
#include "../../modules/settings.hpp"
#include "../../dependencies/imgui/imgui.h"
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <cstdio>
#include <charconv>
#include <string_view>

namespace OreSim {
// Render-thread state only. Java owns world access and chunk caching.
inline jclass bridge = nullptr;
inline jmethodID request = nullptr, frame = nullptr, getStatus = nullptr;
inline std::string status;
inline std::vector<double> frameValues; // Render-thread buffer, retained between frames.
inline double nextRequest = 0, nextInit = 0;
inline int previousMask = -1, previousRadius = -1;
inline bool previousAir = false;
inline long long previousSeed = 0;
inline bool previousHasSeed = false;
struct Point { double x, y, w; };
inline Point Project(const double* m, double x, double y, double z) {
    return {m[0]*x+m[4]*y+m[8]*z+m[12], m[1]*x+m[5]*y+m[9]*z+m[13],
            m[3]*x+m[7]*y+m[11]*z+m[15]};
}
inline bool Clip(Point& a, Point& b) {
    // Homogeneous clipping prevents huge lines when a box crosses the camera.
    double lo=0, hi=1;
    const double aa[]={a.w-0.001,a.w+a.x,a.w-a.x,a.w+a.y,a.w-a.y};
    const double bb[]={b.w-0.001,b.w+b.x,b.w-b.x,b.w+b.y,b.w-b.y};
    for(int i=0;i<5;++i) {
        if(!std::isfinite(aa[i]) || !std::isfinite(bb[i])) return false;
        if(aa[i]<0 && bb[i]<0) return false;
        if(aa[i]<0) lo=(std::max)(lo,aa[i]/(aa[i]-bb[i]));
        if(bb[i]<0) hi=(std::min)(hi,aa[i]/(aa[i]-bb[i]));
    }
    if(lo>hi) return false;
    Point d{b.x-a.x,b.y-a.y,b.w-a.w}, start=a;
    a={start.x+lo*d.x,start.y+lo*d.y,start.w+lo*d.w};
    b={start.x+hi*d.x,start.y+hi*d.y,start.w+hi*d.w};
    return true;
}
inline bool Check(JNIEnv* env) {
    if(!env->ExceptionCheck()) return true;
    env->ExceptionDescribe(); env->ExceptionClear();
    status="JNI-Fehler: Bridge-Jar pruefen, Spiel neu starten";
    return false;
}
inline bool Initialize(JNIEnv* env) {
    if(bridge) return true;
    if(!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
    auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.oresim.OreSimBridge");
    if(!Check(env) || !cls) return false;
    request=env->GetStaticMethodID(cls,"request","(IIZJZ)V");
    if(!Check(env) || !request) return false;
    frame=env->GetStaticMethodID(cls,"frame","()[D");
    if(!Check(env) || !frame) return false;
    getStatus=env->GetStaticMethodID(cls,"getStatus","()Ljava/lang/String;");
    if(!Check(env) || !getStatus) return false;
    bridge=static_cast<jclass>(env->NewGlobalRef(cls));
    return Check(env) && bridge;
}
inline void Render() {
    int mask=0;
    if(OreSim_Enabled) for(int i=0;i<10;++i) if(OreSim_Ores[i]) mask|=1<<i;
    if(!OreSim_Enabled && previousMask<=0) return;
    double now=ImGui::GetTime();
    if(!bridge && now<nextInit) return;
    auto env=SeedCracker::GetMinecraftJNIEnv();
    if(!env) { nextInit=now+5; return; }
    if(env->PushLocalFrame(32)<0) { env->ExceptionClear(); return; }
    struct LocalFrame { JNIEnv* env; ~LocalFrame(){env->PopLocalFrame(nullptr);} } scope{env};
    if(!Initialize(env)) { nextInit=now+5; return; }
    if(mask!=previousMask || OreSim_Radius.load()!=previousRadius || OreSim_AirCheck!=previousAir || manualSeed!=previousSeed || hasManualSeed!=previousHasSeed || now>=nextRequest) {
        env->CallStaticVoidMethod(bridge,request,mask,OreSim_Radius.load(),static_cast<jboolean>(OreSim_AirCheck),static_cast<jlong>(manualSeed),static_cast<jboolean>(hasManualSeed));
        if(!Check(env)) { nextRequest=now+5; return; }
        previousMask=mask; previousRadius=OreSim_Radius.load(); previousAir=OreSim_AirCheck; previousSeed=manualSeed; previousHasSeed=hasManualSeed;
        nextRequest=now+0.05;
        auto message=static_cast<jstring>(env->CallStaticObjectMethod(bridge,getStatus));
        if(!Check(env)) return;
        if(message) {
            const char* text=env->GetStringUTFChars(message,nullptr);
            if(text) {
                std::string updated=text; env->ReleaseStringUTFChars(message,text);
                if(updated!=status) { status=updated; std::printf("[OreSim] %s\n",status.c_str()); }
            }
            if(!Check(env)) return;
        }
    }
    if(!OreSim_Enabled) { status.clear(); return; }
    auto draw=ImGui::GetBackgroundDrawList();
    auto vp=ImGui::GetMainViewport();
    draw->AddText(ImVec2(vp->Pos.x+12,vp->Pos.y+vp->Size.y-30),IM_COL32(210,225,255,255),("OreSim: "+status).c_str());
    if(!hasManualSeed) return;
    auto data=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frame));
    if(!Check(env) || !data) return;
    jsize length=env->GetArrayLength(data);
    if(length<19 || length>8019 || (length-19)%4!=0) return;
    frameValues.resize(length);
    auto& values=frameValues;
    env->GetDoubleArrayRegion(data,0,length,values.data());
    if(!Check(env)) return;
    const double* m=values.data();
    constexpr ImU32 colors[]={IM_COL32(90,90,90,220),IM_COL32(225,200,175,220),IM_COL32(255,215,40,220),
        IM_COL32(255,60,60,220),IM_COL32(65,240,245,240),IM_COL32(75,105,255,220),IM_COL32(235,145,70,220),
        IM_COL32(60,245,120,220),IM_COL32(245,235,220,220),IM_COL32(185,110,90,240)};
    constexpr int edges[][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    for(int i=19;i<length;i+=4) {
        if(!std::isfinite(values[i+3]) || values[i+3]<0 || values[i+3]>9) continue;
        int kind=static_cast<int>(values[i+3]);
        Point corners[8];
        for(int c=0;c<8;++c) corners[c]=Project(m,values[i]+(c&1)-m[16],values[i+1]+((c>>1)&1)-m[17],values[i+2]+((c>>2)&1)-m[18]);
        for(auto& edge:edges) {
            Point a=corners[edge[0]], b=corners[edge[1]];
            if(!Clip(a,b)) continue;
            auto screen=[&](Point p) {return ImVec2(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*0.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*0.5));};
            draw->AddLine(screen(a),screen(b),colors[kind],1.2f);
        }
    }
    draw->PopClipRect();
}
}
