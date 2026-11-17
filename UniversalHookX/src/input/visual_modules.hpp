#pragma once
#include "../modules/settings.hpp"
#include "loot_beacon_rules.hpp"
#include "../utils/esp/glow.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include "../utils/oresim/oresim.hpp"
#include "../menu/widgets.hpp"
#include "../utils/theme/theme.hpp"
#include <array>
#include <chrono>
#include <mutex>
#include <vector>

namespace VisualModules {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID frame=nullptr,clear=nullptr,labelMethod=nullptr;
inline bool running=false;
inline auto retry=std::chrono::steady_clock::time_point{};
inline std::vector<double> values;
inline bool Check(JNIEnv* env) {
    if(!env->ExceptionCheck()) return true;
    env->ExceptionDescribe();env->ExceptionClear();return false;
}
inline bool Load(JNIEnv* env) {
    if(bridge) return true;
    auto now=std::chrono::steady_clock::now();if(now<retry) return false;
    retry=now+std::chrono::seconds(5);
    if(!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar"))return false;
    auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.visual.VisualModulesBridge");
    if(!Check(env) || !cls) return false;
    JniSafety::Lookup api(env);
    frame=api.GetStaticMethodID(cls,"frame","([I)[D");clear=api.GetStaticMethodID(cls,"clear","()V");
    labelMethod=api.GetStaticMethodID(cls,"labels","()[Ljava/lang/String;");
    if(!frame || !clear || !labelMethod) return false;
    bridge=static_cast<jclass>(env->NewGlobalRef(cls));return Check(env) && bridge;
}
inline void Clear(JNIEnv* env,bool shutdown=false) {
    std::lock_guard lock(mutex);running=false;
    if(env && bridge && !env->ExceptionCheck()) {env->CallStaticVoidMethod(bridge,clear);Check(env);}
    if(shutdown && env && bridge) {env->DeleteGlobalRef(bridge);bridge=nullptr;}
    values.clear();
}
inline void Render() {
    bool enabled=(ProjectilePrediction_Enabled || SoundESP_Enabled || StorageESP_Enabled || ItemESP_Enabled || LootBeacon_Enabled || ExplosionPreview_Enabled || TotemWarner_Enabled) && !Menu_Enabled;
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);enabled &= pid==GetCurrentProcessId();
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env || env->ExceptionCheck())return;
    std::lock_guard lock(mutex);
    if(!enabled) {
        if(running && bridge) {env->CallStaticVoidMethod(bridge,clear);Check(env);}
        running=false;return;
    }
    JniSafety::LocalFrame local(env,8);if(!local || !Load(env))return;
    running=true;
    const LootBeaconRules::Selection itemSelection{ItemESP_Enabled.load(),ItemESP_Range.load(),{ItemESP_Totems.load(),ItemESP_Equipment.load(),ItemESP_Other.load()}};
    const LootBeaconRules::Selection beaconSelection{LootBeacon_Enabled.load(),LootBeacon_Range.load(),{LootBeacon_Totems.load(),LootBeacon_Equipment.load(),LootBeacon_Other.load()}};
    const auto scan=LootBeaconRules::Merge(itemSelection,beaconSelection);
    // ABI: 31 visual options. Java validates the exact length; combat options remain independent.
    std::array<jint,31> options={ProjectilePrediction_Enabled.load(),ProjectilePrediction_Held.load(),ProjectilePrediction_Flying.load(),ProjectilePrediction_Range.load(),ProjectilePrediction_Ticks.load(),
        SoundESP_Enabled.load(),SoundESP_Steps.load(),SoundESP_Projectiles.load(),SoundESP_Explosions.load(),SoundESP_Range.load(),SoundESP_Lifetime.load(),
        StorageESP_Enabled.load(),StorageESP_Range.load(),StorageESP_Chests.load(),StorageESP_Barrels.load(),StorageESP_Shulkers.load(),StorageESP_EnderChests.load(),
        scan.enabled,scan.range,scan.groups[0],scan.groups[1],scan.groups[2],ItemESP_Enabled.load() && ItemESP_Labels.load(),
        ExplosionPreview_Enabled.load(),ExplosionPreview_Range.load(),ExplosionPreview_Crystals.load(),ExplosionPreview_Anchors.load(),ExplosionPreview_Target.load(),AntiBot_Enabled.load(),
        TotemWarner_Enabled.load(),TotemWarner_Range.load()};
    auto config=env->NewIntArray(static_cast<jsize>(options.size()));if(!Check(env) || !config)return;
    env->SetIntArrayRegion(config,0,static_cast<jsize>(options.size()),options.data());if(!Check(env))return;
    auto data=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frame,config));if(!Check(env) || !data)return;
    auto length=env->GetArrayLength(data);if(length<19 || length>19+2048*9 || (length-19)%9)return;
    values.resize(length);env->GetDoubleArrayRegion(data,0,length,values.data());if(!Check(env))return;
    for(double value:values)if(!std::isfinite(value))return;
    std::vector<std::string> labels;
    auto texts=static_cast<jobjectArray>(env->CallStaticObjectMethod(bridge,labelMethod));
    if(!Check(env))return;
    if(texts) {auto n=env->GetArrayLength(texts);if(n>512)return;labels.reserve(n);
        for(jsize k=0;k<n;k++) {auto str=static_cast<jstring>(env->GetObjectArrayElement(texts,k));
            const char* utf=str?env->GetStringUTFChars(str,nullptr):nullptr;
            if(!Check(env))return;labels.emplace_back(utf?utf:"");
            if(utf)env->ReleaseStringUTFChars(str,utf);if(str)env->DeleteLocalRef(str);
        }
    }
    const double* m=values.data();auto vp=ImGui::GetMainViewport();auto draw=ImGui::GetBackgroundDrawList();
    auto project=[&](double x,double y,double z){return OreSim::Project(m,x-m[16],y-m[17],z-m[18]);};
    auto screen=[&](OreSim::Point p){return ImVec2(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*.5));};
    auto line=[&](OreSim::Point a,OreSim::Point b,ImU32 color,float width,bool glow){
        if(!OreSim::Clip(a,b))return;
        if(glow && Theme::EspGlow()>0)draw->AddLine(screen(a),screen(b),Menu::Widgets::ColA(color,.12f*Theme::EspGlow()),width+5);
        draw->AddLine(screen(a),screen(b),color,width);
    };
    auto beacon=[&](double x,double y,double z,float opacity) {
        const ImU32 base=LootBeacon_Theme ? Theme::Current().accent : Theme::FromRgb(LootBeacon_Color.load());
        const float strength=std::clamp(LootBeacon_Opacity.load(),10,100)/100.0f*opacity;
        const float width=float(std::clamp(LootBeacon_Width.load(),1,6));
        auto a=project(x,y,z),b=project(x,y+std::clamp(LootBeacon_Height.load(),1,8),z);
        if(OreSim::Clip(a,b)) {
            const auto from=screen(a),to=screen(b);
            if(LootBeacon_Glow)EspGlow::Line(draw,from,to,width,strength,base);
            draw->AddLine(from,to,Menu::Widgets::ColA(base,strength),width);
        }
        for(int k=0;k<16;++k) {
            const double angle=k*6.283185307179586/16,next=(k+1)*6.283185307179586/16;
            line(project(x+std::cos(angle)*.16,y,z+std::sin(angle)*.16),
                 project(x+std::cos(next)*.16,y,z+std::sin(next)*.16),Menu::Widgets::ColA(base,strength*.8f),1.5f,false);
        }
    };
    auto accent=Theme::Current().accent;
    constexpr int edges[][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    for(int i=19;i<length;i+=9) {
        int kind=static_cast<int>(m[i]),category=static_cast<int>(m[i+7]);
        float alpha=std::clamp(float(m[i+8]),0.f,1.f);
        auto color=Menu::Widgets::ColA(accent,alpha);
        if(kind==5) {
            const double x=(m[i+1]+m[i+4])*.5,y=(m[i+2]+m[i+5])*.5,z=(m[i+3]+m[i+6])*.5;
            const double distance=(x-m[16])*(x-m[16])+(y-m[17])*(y-m[17])+(z-m[18])*(z-m[18]);
            if(LootBeaconRules::Includes(beaconSelection,category,distance)) beacon(x,m[i+5]+.04,z,alpha);
            if(!LootBeaconRules::Includes(itemSelection,category,distance))continue;
        }
        if(kind==6) {
            const int type=static_cast<int>(m[i+4]);
            const double distance=(m[i+1]-m[16])*(m[i+1]-m[16])+(m[i+2]-m[17])*(m[i+2]-m[17])+(m[i+3]-m[18])*(m[i+3]-m[18]);
            if(!ItemESP_Labels || !LootBeaconRules::Includes(itemSelection,type,distance))continue;
        }
        if(kind==6 || kind==7) {
            if(category<0 || category>=static_cast<int>(labels.size()))continue;
            if(kind==6) {int type=std::clamp(int(m[i+4]),0,2);int rgb=ItemESP_Colors[type].load();color=IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);}
            auto p=project(m[i+1],m[i+2],m[i+3]);if(p.w<=.001 || std::abs(p.x)>p.w || std::abs(p.y)>p.w)continue;
            auto at=screen(p);auto size=ImGui::CalcTextSize(labels[category].c_str());at.x-=size.x*.5f;
            draw->AddRectFilled(ImVec2(at.x-5,at.y-3),ImVec2(at.x+size.x+5,at.y+size.y+3),IM_COL32(12,12,20,180),5);
            draw->AddText(at,color,labels[category].c_str());
        } else if(kind==8) {
            auto p=project(m[i+1],m[i+2],m[i+3]);if(p.w<=.001)continue;
            auto at=screen(p);if(ExplosionPreview_Glow)Menu::Widgets::DrawGlowCircle(draw,at,5,accent,alpha,6,true);draw->AddCircle(at,5,color,16,2);
        } else if(kind==0) {
            line(project(m[i+1],m[i+2],m[i+3]),project(m[i+4],m[i+5],m[i+6]),color,float(ProjectilePrediction_LineWidth.load()),ProjectilePrediction_Glow);
        } else if(kind==1 && ProjectilePrediction_Impact) {
            // The hit normal chooses the marker's plane, so wall impacts are visible too.
            const double r=.23;
            for(int s=0;s<24;s++) {
                auto point=[&](int k){double angle=k*6.283185307179586/24,u=std::cos(angle)*r,v=std::sin(angle)*r;
                    double x=m[i+1],y=m[i+2],z=m[i+3];
                    if(std::abs(m[i+4])>.5){y+=u;z+=v;}else if(std::abs(m[i+6])>.5){x+=u;y+=v;}else{x+=u;z+=v;}
                    return project(x,y,z);};
                line(point(s),point(s+1),color,2,ProjectilePrediction_Glow);
            }
        } else if(kind==2 && category>=0 && category<3) {
            auto p=project(m[i+1],m[i+2],m[i+3]);
            if(std::abs(p.w)<.001)continue;
            auto center=screen(p);
            if(p.w<0) {center.x=vp->Pos.x+vp->Size.x-(center.x-vp->Pos.x);center.y=vp->Pos.y+vp->Size.y-(center.y-vp->Pos.y);}
            bool outside=p.w<=0 || std::abs(p.x)>p.w || std::abs(p.y)>p.w;
            if(outside) {
                if(!SoundESP_Directions)continue;
                float dx=center.x-(vp->Pos.x+vp->Size.x*.5f),dy=center.y-(vp->Pos.y+vp->Size.y*.5f);
                float scale=(std::max)(std::abs(dx)/(vp->Size.x*.5f-80),std::abs(dy)/(vp->Size.y*.5f-45));
                if(scale>0){center.x=vp->Pos.x+vp->Size.x*.5f+dx/scale;center.y=vp->Pos.y+vp->Size.y*.5f+dy/scale;}
            }
            constexpr const char* labels[]={"Steps","Projectile","Explosion"};
            double dx=m[i+1]-m[16],dy=m[i+2]-m[17],dz=m[i+3]-m[18];
            char text[64];std::snprintf(text,sizeof(text),"%s %.0fm",labels[category],std::sqrt(dx*dx+dy*dy+dz*dz));
            if(SoundESP_Glow)Menu::Widgets::DrawGlowCircle(draw,center,4,accent,alpha,6,true);
            draw->AddCircleFilled(center,3,color,12);draw->AddText(ImVec2(center.x+9,center.y-7),color,text);
        } else if((kind==3 && category>=0 && category<4) || (kind==4 && ProjectilePrediction_HighlightBlock) || (kind==5 && category>=0 && category<3)) {
            if(kind==3 && !StorageESP_ThemeColors) {
                int rgb=StorageESP_Colors[category].load();
                color=Menu::Widgets::ColA(IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),alpha);
            }
            if(kind==5) {int rgb=ItemESP_Colors[category].load();color=Menu::Widgets::ColA(IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),alpha);}
            OreSim::Point corners[8];
            for(int c=0;c<8;c++)corners[c]=project((c&1)?m[i+4]:m[i+1],(c&2)?m[i+5]:m[i+2],(c&4)?m[i+6]:m[i+3]);
            if(kind==4) {
                constexpr int faces[][4]={{0,1,3,2},{4,6,7,5},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
                for(const auto& face:faces) {
                    ImVec2 polygon[4];bool valid=true;
                    for(int k=0;k<4;k++) {if(corners[face[k]].w<=.001){valid=false;break;}polygon[k]=screen(corners[face[k]]);}
                    if(valid)draw->AddConvexPolyFilled(polygon,4,Menu::Widgets::ColA(color,.08f));
                }
            }
            for(auto& edge:edges)line(corners[edge[0]],corners[edge[1]],color,kind==4?1.8f:float(StorageESP_LineWidth.load()),kind==4?ProjectilePrediction_Glow.load():kind==5?ItemESP_Glow.load():StorageESP_Glow.load());
            if(kind==3 && StorageESP_Labels) {
                auto p=project((m[i+1]+m[i+4])*.5,m[i+5]+.1,(m[i+3]+m[i+6])*.5);
                if(p.w>.001 && std::abs(p.x)<=p.w && std::abs(p.y)<=p.w) {
                    constexpr const char* names[]={"Chest","Barrel","Shulker","Ender chest"};auto at=screen(p);
                    draw->AddText(at,color,names[category]);
                }
            }
        }
    }
    draw->PopClipRect();
}
}
