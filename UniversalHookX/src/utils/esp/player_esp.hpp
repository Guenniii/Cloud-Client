#include "../theme/theme.hpp"
#pragma once
#include "../oresim/oresim.hpp"
#include <algorithm>
#include "skin_geometry.hpp"
#include "glow.hpp"
#include "../../input/rotation/antibot.hpp"

namespace PlayerESP {
inline jclass bridge = nullptr;
inline jmethodID frameMethod = nullptr;
inline jclass skinBridge = nullptr;
inline jmethodID skinFrame = nullptr;
inline double retryAt = 0;
inline std::string error;
inline std::vector<double> frameValues, skinFrameValues; // Render-thread buffers.
inline bool Check(JNIEnv* env) {
    if (!env->ExceptionCheck()) return true;
    env->ExceptionDescribe();
    env->ExceptionClear();
    error = "Player ESP: Bridge-Jar pruefen / Spiel neu starten";
    std::printf("[PlayerESP] %s\n", error.c_str());
    retryAt = ImGui::GetTime() + 5;
    return false;
}
inline void Render() {
    if (!PlayerESP_Enabled) return;
    auto draw = ImGui::GetBackgroundDrawList();
    auto vp = ImGui::GetMainViewport();
    if (ImGui::GetTime() < retryAt) {
        if (!error.empty()) draw->AddText(ImVec2(vp->Pos.x+12, vp->Pos.y+12), IM_COL32(255,110,110,255), error.c_str());
        return;
    }
    auto env = SeedCracker::GetMinecraftJNIEnv();
    if (!env) { retryAt=ImGui::GetTime()+5; return; }
    if (env->PushLocalFrame(16)<0) { Check(env); return; }
    struct LocalFrame { JNIEnv* env; ~LocalFrame(){env->PopLocalFrame(nullptr);} } scope{env};
    if (!bridge) {
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) {
            Check(env); retryAt=ImGui::GetTime()+5; return;
        }
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.esp.PlayerEspBridge");
        if (!Check(env) || !cls) return;
        frameMethod=env->GetStaticMethodID(cls,"frame","(IZ)[D");
        if (!Check(env) || !frameMethod) return;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return;
    }
    { PlayerFilter::AntiBot filter(env); }
    if(PlayerESP_Skin) {
        if(!skinBridge) {
            auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.esp.PlayerSkinBridge");
            if(!Check(env) || !cls) return;
            skinFrame=env->GetStaticMethodID(cls,"frame","(IZ)[D");
            if(!Check(env) || !skinFrame) return;
            skinBridge=static_cast<jclass>(env->NewGlobalRef(cls));
            if(!Check(env) || !skinBridge) return;
        }
        auto skinData=static_cast<jdoubleArray>(env->CallStaticObjectMethod(skinBridge,skinFrame,PlayerESP_Range.load(), static_cast<jboolean>(AntiBot_Enabled.load())));
        if(!Check(env) || !skinData) return;
        auto length=env->GetArrayLength(skinData);
        if(length<19 || length>19+60000*13 || (length-19)%13) return;
        skinFrameValues.resize(length);
        auto& skinValues=skinFrameValues;
        env->GetDoubleArrayRegion(skinData,0,length,skinValues.data());
        if(!Check(env)) return;
        error.clear();
        PlayerSkinGeometry::Draw(draw,vp,skinValues);
        return; // Skin and hitbox modes are mutually exclusive.
    }
    auto data=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frameMethod,PlayerESP_Range.load(), static_cast<jboolean>(AntiBot_Enabled.load())));
    if (!Check(env) || !data) return;
    error.clear();
    const jsize count=env->GetArrayLength(data);
    if (count<19 || count>19+256*6 || (count-19)%6) return;
    frameValues.resize(count);
    auto& values=frameValues;
    env->GetDoubleArrayRegion(data,0,count,values.data());
    if (!Check(env)) return;
    const double* m=values.data();
    constexpr int edges[][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    const float thickness=static_cast<float>(std::clamp(PlayerESP_LineWidth.load(),1,4));
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    for (int i=19;i<count;i+=6) {
        bool finite=true;
        for(int k=0;k<6;++k) finite &= std::isfinite(values[i+k]);
        if(!finite || values[i]>values[i+3] || values[i+1]>values[i+4] || values[i+2]>values[i+5]) continue;
        OreSim::Point corners[8];
        for(int c=0;c<8;++c) corners[c]=OreSim::Project(m,
            values[i+((c&1)?3:0)]-m[16],values[i+((c&2)?4:1)]-m[17],values[i+((c&4)?5:2)]-m[18]);
        for(auto& edge:edges) {
            auto a=corners[edge[0]], b=corners[edge[1]];
            if(!OreSim::Clip(a,b)) continue;
            auto screen=[&](OreSim::Point p){return ImVec2(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*.5));};
            auto p=screen(a), q=screen(b);
            if(PlayerESP_Glow) EspGlow::Line(draw,p,q,thickness);
            draw->AddLine(p,q,IM_COL32(15,10,30,180),thickness+2);
            draw->AddLine(p,q,Theme::Current().accent,thickness);
        }
    }
    draw->PopClipRect();
}
}
