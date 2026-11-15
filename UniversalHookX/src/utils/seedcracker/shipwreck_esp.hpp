#pragma once
#include "seedcracker_bridge.hpp"
#include "../../modules/settings.hpp"
#include "../sdk/jni_safety.hpp"
#include "../oresim/oresim.hpp"
#include "../esp/glow.hpp"
#include <vector>

namespace ShipwreckESP {
inline jclass bridge=nullptr;
inline jmethodID frame=nullptr;
inline double retry=0;
inline void Shutdown(JNIEnv* env){if(env&&bridge)env->DeleteGlobalRef(bridge);bridge=nullptr;frame=nullptr;retry=0;}
inline void Render(){
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    if(!SeedCracker_Enabled||!SeedCracker_Boxes||Menu_Enabled||pid!=GetCurrentProcessId())return;
    const auto now=ImGui::GetTime();if(now<retry)return;
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env||env->ExceptionCheck())return;
    JniSafety::LocalFrame local(env,8);if(!local)return;
    auto check=[&](){if(!env->ExceptionCheck())return true;env->ExceptionDescribe();env->ExceptionClear();retry=now+5;return false;};
    if(!bridge){
        if(!SeedCracker::Init(env,"C:/Users/okeba/source/repos/UniversalHookX/UniversalHookX/bin/SeedCrackerBridge.jar")){retry=now+5;return;}
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.seedcracker.SeedCrackerBridge");if(!check()||!cls)return;
        frame=env->GetStaticMethodID(cls,"shipwreckFrame","()[D");if(!check()||!frame)return;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));if(!check()||!bridge)return;
    }
    auto raw=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frame));if(!check()||!raw)return;
    const auto count=env->GetArrayLength(raw);if(count<19||count>19+512*6||(count-19)%6)return;
    std::vector<double> data(count);env->GetDoubleArrayRegion(raw,0,count,data.data());if(!check())return;
    for(double v:data)if(!std::isfinite(v))return;
    const int edges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
    auto* vp=ImGui::GetMainViewport();auto* draw=ImGui::GetBackgroundDrawList();
    const auto color=Theme::Current().accent;const float width=float(std::clamp(SeedCracker_BoxWidth.load(),1,4));
    auto screen=[&](OreSim::Point p){return ImVec2(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*.5));};
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    for(int at=19;at<count;at+=6){
        if(data[at]>=data[at+3]||data[at+1]>=data[at+4]||data[at+2]>=data[at+5])continue;
        OreSim::Point points[8];
        for(int k=0;k<8;k++)points[k]=OreSim::Project(data.data(),data[at+((k&1)?3:0)]-data[16],
            data[at+1+((k&2)?3:0)]-data[17],data[at+2+((k&4)?3:0)]-data[18]);
        for(auto& edge:edges){auto a=points[edge[0]],b=points[edge[1]];if(!OreSim::Clip(a,b))continue;
            auto from=screen(a),to=screen(b);
            if(SeedCracker_BoxGlow)EspGlow::Line(draw,from,to,width,.8f,color);
            draw->AddLine(from,to,color,width);
        }
    }
    draw->PopClipRect();
}
}
