#pragma once
#include "../oresim/oresim.hpp"
#include "../../modules/settings.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "glow.hpp"
#include "echo_trail_model.hpp"
#include "../../input/rotation/antibot.hpp"
#include <unordered_map>
#include <limits>
namespace EchoTrail {
struct Track {std::deque<EchoTrailModel::Sample> samples;bool friendPlayer=false;};
inline std::unordered_map<int,Track> tracks;
inline std::vector<double> frame;
inline jclass bridge=nullptr;
inline jmethodID frameMethod=nullptr;
inline double world=-1,retry=0;
inline bool previousBot=false,previousSelf=false;
inline void Clear(){tracks.clear();frame.clear();world=-1;}
inline void Shutdown(JNIEnv* env){Clear();if(env && bridge){env->DeleteGlobalRef(bridge);bridge=nullptr;frameMethod=nullptr;}}
inline void Render() {
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    if(!EchoTrail_Enabled || Menu_Enabled || pid!=GetCurrentProcessId()){Clear();return;}
    const double now=ImGui::GetTime();
    if(now<retry) return;
    auto* env=SeedCracker::GetMinecraftJNIEnv();if(!env || env->ExceptionCheck()){Clear();return;}
    JniSafety::LocalFrame local(env,8);if(!local){Clear();return;}
    auto check=[&](){if(!env->ExceptionCheck())return true;env->ExceptionDescribe();env->ExceptionClear();Clear();retry=now+5;return false;};
    if(!bridge) {
        if(!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")){Clear();retry=now+5;return;}
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.esp.EchoTrailBridge");
        if(!check() || !cls){retry=now+5;return;}
        frameMethod=env->GetStaticMethodID(cls,"frame","(IZZ)[D");
        if(!check() || !frameMethod){retry=now+5;return;}
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));if(!check() || !bridge)return;
    }
    {PlayerFilter::AntiBot filter(env);}
    const bool bot=AntiBot_Enabled.load(),self=EchoTrail_Self.load();
    if(previousBot!=bot || previousSelf!=self){Clear();previousBot=bot;previousSelf=self;}
    auto values=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frameMethod,EchoTrail_Range.load(),static_cast<jboolean>(bot),static_cast<jboolean>(self)));
    if(!check() || !values){Clear();return;}
    const int count=env->GetArrayLength(values);
    if(count<20 || count>20+256*6 || (count-20)%6){Clear();return;}
    frame.resize(count);env->GetDoubleArrayRegion(values,0,count,frame.data());if(!check())return;
    for(int i=0;i<20;++i) if(!std::isfinite(frame[i])){Clear();return;}
    if(world!=frame[19]){tracks.clear();world=frame[19];}
    const double lifetime=std::clamp(EchoTrail_Duration.load(),500,6000)/1000.0;
    for(auto it=tracks.begin();it!=tracks.end();) {
        EchoTrailModel::Prune(it->second.samples,now,lifetime);
        if(it->second.samples.empty())it=tracks.erase(it);else ++it;
    }
    for(int i=20;i<count;i+=6) {
        if(!std::isfinite(frame[i]) || frame[i]<0 || frame[i]>(std::numeric_limits<int>::max)())continue;
        const int id=static_cast<int>(frame[i]);
        if(!tracks.contains(id) && tracks.size()>=256)continue;
        auto& track=tracks[id];track.friendPlayer=frame[i+4]!=0;
        EchoTrailModel::Append(track.samples,{frame[i+1],frame[i+2],frame[i+3],now},lifetime);
    }
    auto* draw=ImGui::GetBackgroundDrawList();auto* vp=ImGui::GetMainViewport();
    auto screen=[&](OreSim::Point p){return ImVec2(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*.5));};
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    int rendered=0;const float width=float(std::clamp(EchoTrail_Width.load(),1,6));
    for(const auto& [id,track]:tracks) {
        if(track.samples.size()<2)continue;
        const int rgb=EchoTrail_Color.load();
        const ImU32 base=EchoTrail_Theme ? Theme::PlayerColor(track.friendPlayer,Friends_Color.load()) : IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);
        for(size_t i=1;i<track.samples.size() && rendered<8192;++i) {
            const auto& a=track.samples[i-1];const auto& b=track.samples[i];
            auto p=OreSim::Project(frame.data(),a.x-frame[16],a.y-frame[17],a.z-frame[18]);
            auto q=OreSim::Project(frame.data(),b.x-frame[16],b.y-frame[17],b.z-frame[18]);
            if(!OreSim::Clip(p,q))continue;
            const float fade=(EchoTrailModel::Opacity(now,a.time,lifetime)+EchoTrailModel::Opacity(now,b.time,lifetime))*.5f;
            if(fade<.015f)continue;
            auto color=Menu::Widgets::ColA(base,fade);auto from=screen(p),to=screen(q);
            if(EchoTrail_Glow)EspGlow::Line(draw,from,to,width,fade,base);
            draw->AddLine(from,to,color,width);++rendered;
        }
    }
    draw->PopClipRect();
}
}
