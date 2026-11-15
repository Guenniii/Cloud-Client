#pragma once
#include "widgets.hpp"
#include "resources.hpp"
#include "../dependencies/font/IconsFontAwesome5.h"
#include <array>
#include "../utils/theme/theme.hpp"
#include "../modules/settings.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <optional>

namespace Menu::TargetHud {
struct Snapshot {
    std::string name;
    float health=0,maximum=20,absorption=0,armor=0,distance=0;
    int generation=0,id=0;
    bool preview=false;
    std::array<ImU32,64> head{};
    bool hasHead=false;
};
struct Animation {
    int generation=-1,id=-1;
    float health=0,trail=0,appear=0;
    bool valid=false;
    void Reset(){*this=Animation{};}
};
inline Animation animation;
inline std::optional<Snapshot> retained;
inline void ClearPresentation(){animation.Reset();retained.reset();}
inline std::string FitName(const std::string& value,ImFont* font,float size,float width) {
    if(font->CalcTextSizeA(size,FLT_MAX,0,value.c_str()).x<=width)return value;
    auto cut=value.size();
    while(cut>0){--cut;while(cut>0&&(static_cast<unsigned char>(value[cut])&0xC0)==0x80)--cut;
        auto text=value.substr(0,cut)+"...";if(font->CalcTextSizeA(size,FLT_MAX,0,text.c_str()).x<=width)return text;}
    return "...";
}
inline void Draw(const Snapshot& data,bool showing=true) {
    const float ratio=std::clamp(data.health/(std::max)(1.f,data.maximum),0.f,1.f);
    if(!animation.valid||animation.generation!=data.generation){
        animation={data.generation,data.id,ratio,ratio,0,true};
    }else if(animation.id!=data.id){
        // Switching opponents must not replay the entrance or mix their health bars.
        animation.id=data.id;animation.health=animation.trail=ratio;
    }
    const float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f);
    animation.health+=(ratio-animation.health)*(1-std::exp(-14*dt));
    animation.trail=(std::max)(animation.health,animation.trail+(ratio-animation.trail)*(1-std::exp(-4*dt)));
    animation.appear=std::clamp(animation.appear+(showing?dt/.24f:-dt/.18f),0.f,1.f);
    const float alpha=animation.appear*animation.appear*(3-2*animation.appear);
    if(alpha<=0)return;
    auto* vp=ImGui::GetMainViewport();const bool mini=TargetHUD_Mini.load();
    const float scale=std::clamp(TargetHUD_Scale.load(),75,150)/100.0f;
    const float width=(std::min)((mini?240.f:328.f)*scale,vp->WorkSize.x);
    const float height=(mini?64.f:TargetHUD_Details?112.f:90.f)*scale;
    const float spaceX=(std::max)(0.f,vp->WorkSize.x-width),spaceY=(std::max)(0.f,vp->WorkSize.y-height);
    static float dragX=0,dragY=0;
    static int savedX=-1,savedY=-1;
    if(savedX!=TargetHUD_X.load()){savedX=TargetHUD_X.load();dragX=float(std::clamp(savedX,0,100));}
    if(savedY!=TargetHUD_Y.load()){savedY=TargetHUD_Y.load();dragY=float(std::clamp(savedY,0,100));}
    const ImVec2 pos(vp->WorkPos.x+spaceX*dragX/100.f,vp->WorkPos.y+spaceY*dragY/100.f+8.f*scale*(1-alpha));
    ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(ImVec2(width,height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings
        |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing;
    if(!Menu_Enabled||!showing)flags|=ImGuiWindowFlags_NoInputs;
    if(ImGui::Begin("##target_hud",nullptr,flags)) {
        ImGui::InvisibleButton("##move_target",ImVec2(width,height));
        // Apply per-frame deltas with sub-percent accumulation for smooth dragging.
        if(Menu_Enabled&&showing&&ImGui::IsItemActive()){
            if(spaceX>0)dragX=std::clamp(dragX+ImGui::GetIO().MouseDelta.x/spaceX*100,0.f,100.f);
            if(spaceY>0)dragY=std::clamp(dragY+ImGui::GetIO().MouseDelta.y/spaceY*100,0.f,100.f);
            TargetHUD_X=savedX=int(std::lround(dragX));TargetHUD_Y=savedY=int(std::lround(dragY));
        }
        auto* draw=ImGui::GetWindowDrawList();const auto& theme=Theme::Current();
        const auto at=[&](float x,float y){return ImVec2(pos.x+x*scale,pos.y+y*scale);};
        const auto color=[&](ImU32 value,float opacity=1.f){return Widgets::ColA(value,alpha*opacity);};
        const ImVec2 end(pos.x+width,pos.y+height);
        if(TargetHUD_Glow){
            draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),false);
            Widgets::DrawGlowRect(draw,pos,end,14*scale,theme.accent,.5f*alpha,12);
            draw->PopClipRect();
        }
        draw->AddRectFilled(pos,end,color(theme.card),14*scale);
        draw->AddRect(pos,end,color(theme.border,.8f),14*scale);
        auto* font=ImGui::GetFont();const float localWidth=width/scale;
        const auto text=[&](float x,float y,float size,ImU32 value,const char* label){
            draw->AddText(font,size*scale,at(x,y),color(value),label);
        };
        const auto textWidth=[&](float size,const char* label){return font->CalcTextSizeA(size*scale,FLT_MAX,0,label).x/scale;};
        const auto glyph=[&](float x,float y,float size,ImU32 value,const char* label){
            if(icons)draw->AddText(icons,size*scale,at(x,y),color(value),label);
        };
        const float avatar=mini?32.f:48.f,left=mini?12.f:16.f,top=mini?14.f:16.f;
        draw->AddRectFilled(at(left,top),at(left+avatar,top+avatar),color(theme.soft),10*scale);
        if(data.hasHead){
            const float inset=mini?3.f:4.f,pixel=(avatar-inset*2)/8;
            for(int y=0;y<8;y++)for(int x=0;x<8;x++)
                draw->AddRectFilled(at(left+inset+x*pixel,top+inset+y*pixel),
                    at(left+inset+(x+1)*pixel,top+inset+(y+1)*pixel),color(data.head[y*8+x]));
        }else glyph(left+avatar*.28f,top+avatar*.24f,avatar*.48f,theme.accent,ICON_FA_USER);
        char hp[40],maximum[40],extra[40];
        std::snprintf(hp,sizeof(hp),"%.1f",data.health);
        std::snprintf(maximum,sizeof(maximum),"/ %.0f HP",data.maximum);
        std::snprintf(extra,sizeof(extra),"+%.0f",data.absorption);
        const ImU32 gold=Theme::Blend(theme.text,IM_COL32(227,165,44,255),.65f);
        if(mini){
            const float numberWidth=textWidth(13,hp),numberX=localWidth-12-numberWidth;
            auto name=FitName(data.name,font,12*scale,(std::max)(1.f,(numberX-54-10)*scale));
            text(54,12,12,theme.text,name.c_str());text(numberX,12,13,theme.text,hp);
            if(data.preview)text(54,47,9,theme.label,"Preview");
            if(data.absorption>0)text(localWidth-12-textWidth(9,extra),47,9,gold,extra);
        }else{
            const float reserve=data.preview?62.f:16.f;
            auto name=FitName(data.name,font,15*scale,(std::max)(1.f,(localWidth-78-reserve)*scale));
            text(78,15,15,theme.text,name.c_str());
            if(data.preview)text(localWidth-16-textWidth(9,"Preview"),19,9,theme.label,"Preview");
            text(78,38,21,theme.text,hp);
            text(78+textWidth(21,hp)+6,47,10,theme.label,maximum);
            if(data.absorption>0){
                const float badgeWidth=textWidth(11,extra)+14,badgeX=localWidth-16-badgeWidth;
                draw->AddRectFilled(at(badgeX,41),at(badgeX+badgeWidth,62),color(gold,.13f),6*scale);
                text(badgeX+7,45,11,gold,extra);
            }
            if(TargetHUD_Details){
                char armor[40],distance[40];std::snprintf(armor,sizeof(armor),"%.0f / 20",data.armor);
                std::snprintf(distance,sizeof(distance),"%.1f m",data.distance);
                glyph(16,76,11,theme.label,ICON_FA_SHIELD_ALT);text(34,74,11,theme.label,armor);
                const float distanceX=localWidth-16-textWidth(11,distance);
                glyph(distanceX-19,76,11,theme.label,ICON_FA_CROSSHAIRS);text(distanceX,74,11,theme.label,distance);
            }
        }
        const float barLeft=mini?54.f:16.f,barRight=localWidth-(mini?12.f:16.f);
        const float barY=mini?36.f:height/scale-18.f,barH=mini?5.f:6.f,barWidth=barRight-barLeft;
        draw->AddRectFilled(at(barLeft,barY),at(barRight,barY+barH),color(theme.soft),barH*scale*.5f);
        if(animation.trail>.001f)draw->AddRectFilled(at(barLeft,barY),at(barLeft+barWidth*animation.trail,barY+barH),color(theme.accent,.28f),barH*scale*.5f);
        if(animation.health>.001f){
            const float fill=barLeft+barWidth*animation.health;
            draw->AddRectFilled(at(barLeft,barY),at(fill,barY+barH),color(theme.accent),barH*scale*.5f);
            if(fill-barLeft>4)draw->AddLine(at(barLeft+2,barY+1),at(fill-2,barY+1),color(IM_COL32_WHITE,.17f),scale);
        }
        if(data.absorption>0){
            const float fraction=std::clamp(data.absorption/(data.maximum+data.absorption),0.f,1.f);
            draw->AddRectFilled(at(barLeft,barY-4),at(barLeft+barWidth*fraction,barY-2),color(gold),scale);
        }
    }
    ImGui::End();ImGui::PopStyleVar();
}
// Retain only the final visual snapshot during the short exit transition.
inline void Present(const Snapshot* snapshot){
    if(snapshot)retained=*snapshot;
    if(!retained)return;
    Draw(*retained,snapshot!=nullptr);
    if(!snapshot&&animation.appear<=0)ClearPresentation();
}
}
