#include "widgets.hpp"
#include "palette.hpp"
#include "../modules/color_rgb.hpp"
#include "../dependencies/imgui/imgui_internal.h"
#include <unordered_map>
#include <cstdio>
#include <cmath>
#include <cstring>

namespace Menu::Widgets {
namespace {
struct Picker {
    float h=0,s=1,v=1,shownS=1,shownV=1,shownH=0,fade=0;
    int rgb=-1,original=0;
    char hex[9]{};
    void Sync(int next,bool snap=false) {
        if(rgb==next && !snap)return;
        auto c=ColorRGB::Unpack(next);float hue,sat,val;
        ImGui::ColorConvertRGBtoHSV(c[0],c[1],c[2],hue,sat,val);
        if(sat>.0001f && val>.0001f)h=hue; // Hue remains editable for gray/black.
        if(val>.0001f)s=sat;
        v=val;rgb=next;
        if(snap){shownS=s;shownV=v;shownH=h;}
        std::snprintf(hex,sizeof(hex),"%06X",static_cast<unsigned>(rgb));
    }
    void Apply(std::atomic<int>* output) {
        std::array<float,3> c{};ImGui::ColorConvertHSVtoRGB(h,s,v,c[0],c[1],c[2]);
        rgb=ColorRGB::Pack(c);output->store(rgb);
        std::snprintf(hex,sizeof(hex),"%06X",static_cast<unsigned>(rgb));
    }
};
ImU32 RGB(int color,float alpha=1.f){return ColA(IM_COL32((color>>16)&255,(color>>8)&255,color&255,255),alpha);}
std::unordered_map<ImGuiID,Picker> states;
void HueTrack(ImDrawList* draw,ImVec2 p,float width,float height,float alpha) {
    float radius=height*.5f;
    for(int i=0;i<132;i++) {
        float left=width*i/132.f,right=width*(i+1)/132.f,mid=(left+right)*.5f;
        float inset=0;
        if(mid<radius)inset=radius-std::sqrt((std::max)(0.f,radius*radius-(radius-mid)*(radius-mid)));
        if(mid>width-radius)inset=radius-std::sqrt((std::max)(0.f,radius*radius-(mid-width+radius)*(mid-width+radius)));
        auto a=ImColor::HSV(left/width,1,1,alpha),b=ImColor::HSV(right/width,1,1,alpha);
        draw->AddRectFilledMultiColor(ImVec2(p.x+left,p.y+inset),ImVec2(p.x+right,p.y+height-inset),a,b,b,a);
    }
}
}

void DrawCustomColorPopup(const char* label,std::atomic<int>* value,ImVec2 anchor,bool opened,float alpha) {
    auto& state=states[ImGui::GetID("##color_picker")];
    if(opened){state.Sync(value->load(),true);state.original=value->load();state.fade=0;ImGui::OpenPopup("##color_picker");}
    if(!ImGui::IsPopupOpen("##color_picker"))return;
    state.Sync(value->load());
    float dt=std::clamp(ImGui::GetIO().DeltaTime,0.f,.1f),ease=1-std::exp(-24*dt);
    state.fade+=(1-state.fade)*ease;
    auto vp=ImGui::GetMainViewport();constexpr float popupWidth=296,popupHeight=394;
    float x=std::clamp(anchor.x+112-popupWidth,vp->WorkPos.x+8,(std::max)(vp->WorkPos.x+8,vp->WorkPos.x+vp->WorkSize.x-popupWidth-8));
    float y=anchor.y+32;
    if(y+popupHeight>vp->WorkPos.y+vp->WorkSize.y-8)y=anchor.y-popupHeight-6;
    y=(std::max)(vp->WorkPos.y+8,y);
    ImGui::SetNextWindowPos(ImVec2(x,y+5*(1-state.fade)));
    ImGui::SetNextWindowSize(ImVec2(popupWidth,popupHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16,16));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,14);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*alpha*state.fade);
    ImGui::PushStyleColor(ImGuiCol_PopupBg,Palette::Card());
    ImGui::PushStyleColor(ImGuiCol_Border,Palette::Border());
    if(ImGui::BeginPopup("##color_picker",ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar)) {
        auto draw=ImGui::GetWindowDrawList();auto p=ImGui::GetCursorScreenPos();float opacity=alpha*state.fade;
        draw->AddText(p,ColA(Palette::Text(),opacity),label);
        draw->AddText(ImVec2(p.x,p.y+21),ColA(Palette::Muted(),opacity),"Custom color / live preview");
        ImGui::SetCursorScreenPos(ImVec2(p.x+242,p.y-2));
        if(ImGui::InvisibleButton("##close",ImVec2(22,24)))ImGui::CloseCurrentPopup();
        bool closeHover=ImGui::IsItemHovered();
        draw->AddCircleFilled(ImVec2(p.x+253,p.y+10),11,ColA(closeHover?Palette::Soft():Palette::Background(),opacity));
        draw->AddLine(ImVec2(p.x+250,p.y+7),ImVec2(p.x+256,p.y+13),ColA(Palette::Muted(),opacity),1.4f);
        draw->AddLine(ImVec2(p.x+250,p.y+13),ImVec2(p.x+256,p.y+7),ColA(Palette::Muted(),opacity),1.4f);

        ImVec2 sv(p.x,p.y+50),svEnd(sv.x+264,sv.y+164);
        ImGui::SetCursorScreenPos(sv);
        ImGui::InvisibleButton("##saturation_value",ImVec2(264,164));
        if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            auto mouse=ImGui::GetIO().MousePos;
            state.s=std::clamp((mouse.x-sv.x)/264.f,0.f,1.f);
            state.v=1-std::clamp((mouse.y-sv.y)/164.f,0.f,1.f);state.Apply(value);
        }
        // Two rounded gradient meshes preserve exact SV colors without a texture.
        int begin=draw->VtxBuffer.Size;
        draw->AddRectFilled(sv,svEnd,ColA(IM_COL32_WHITE,opacity),9);
        auto hue=ImColor::HSV(state.h,1,1);
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw,begin,draw->VtxBuffer.Size,sv,ImVec2(svEnd.x,sv.y),IM_COL32_WHITE,hue);
        begin=draw->VtxBuffer.Size;draw->AddRectFilled(sv,svEnd,IM_COL32(0,0,0,255),9);
        for(int i=begin;i<draw->VtxBuffer.Size;i++) {
            auto& vertex=draw->VtxBuffer[i];float shade=std::clamp((vertex.pos.y-sv.y)/164.f,0.f,1.f);
            vertex.col=IM_COL32(0,0,0,static_cast<int>(255*shade*opacity));
        }
        state.shownS+=(state.s-state.shownS)*ease;state.shownV+=(state.v-state.shownV)*ease;
        ImVec2 handle(sv.x+state.shownS*264,sv.y+(1-state.shownV)*164);
        draw->AddCircleFilled(handle,7,ColA(IM_COL32(15,15,24,210),opacity),32);
        draw->AddCircle(handle,5,ColA(IM_COL32_WHITE,opacity),32,2);

        ImVec2 hp(p.x,p.y+232);
        ImGui::SetCursorScreenPos(ImVec2(hp.x,hp.y-5));
        ImGui::InvisibleButton("##hue",ImVec2(264,24));
        if(ImGui::IsItemActive() && ImGui::IsMouseDown(0)) {
            state.h=std::clamp((ImGui::GetIO().MousePos.x-hp.x)/264.f,0.f,.999999f);state.Apply(value);
        }
        HueTrack(draw,hp,264,12,opacity);
        state.shownH+=(state.h-state.shownH)*ease;
        ImVec2 knob(hp.x+state.shownH*264,hp.y+6);
        draw->AddCircleFilled(knob,8,ColA(Palette::Card(),opacity),32);
        draw->AddCircleFilled(knob,5,ColA(ImColor::HSV(state.h,1,1),opacity),32);
        draw->AddCircle(knob,8,ColA(Palette::Border(),opacity),32,1);

        ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+265));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,7);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,8));
        ImGui::SetNextItemWidth(166);
        bool edited=ImGui::InputText("##hex",state.hex,sizeof(state.hex),ImGuiInputTextFlags_CharsUppercase|ImGuiInputTextFlags_CharsNoBlank);
        if(edited){int rgb;if(ColorRGB::ParseHex(state.hex,rgb)){value->store(rgb);state.Sync(rgb);}}
        if(!ImGui::IsItemActive())std::snprintf(state.hex,sizeof(state.hex),"%06X",static_cast<unsigned>(value->load()));
        ImGui::PopStyleVar(2);
        draw->AddText(ImVec2(p.x+178,p.y+266),ColA(Palette::Muted(),opacity),"Original");
        ImVec2 original(p.x+178,p.y+285);
        ImGui::SetCursorScreenPos(original);
        if(ImGui::InvisibleButton("##original",ImVec2(86,16))){value->store(state.original);state.Sync(state.original);}
        draw->AddRectFilled(original,ImVec2(original.x+86,original.y+16),RGB(state.original,opacity),5);

        draw->AddText(ImVec2(p.x,p.y+313),ColA(Palette::Muted(),opacity),"Quick colors");
        const int presets[]={0xF5B84B,0x45D6CA,0xCE8CF7,0x688DF5,0xF2779E,0xFFFFFF};
        for(int i=0;i<6;i++) {
            ImGui::PushID(i);ImVec2 at(p.x+i*45.f,p.y+334);
            ImGui::SetCursorScreenPos(at);
            if(ImGui::InvisibleButton("##preset",ImVec2(39,24))){value->store(presets[i]);state.Sync(presets[i]);}
            bool hovered=ImGui::IsItemHovered();
            draw->AddRectFilled(at,ImVec2(at.x+39,at.y+24),RGB(presets[i],opacity),6);
            if(hovered || value->load()==presets[i])draw->AddRect(ImVec2(at.x-2,at.y-2),ImVec2(at.x+41,at.y+26),ColA(Palette::Accent(),opacity),8,0,1.5f);
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+363));ImGui::Dummy(ImVec2(264,1));
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);ImGui::PopStyleVar(3);
}
}
