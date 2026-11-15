#pragma once
#include "resources.hpp"
#include "../utils/config/config.hpp"
#include "../modules/settings.hpp"
#include <algorithm>

namespace Menu::Watermark {
inline void Draw(){
    if(!Config::watermark_enabled)return;
    const auto texture=Resources::LogoTexture();if(!texture)return;
    const auto* vp=ImGui::GetMainViewport();
    const float size=(std::min)(56.f,(std::min)(vp->WorkSize.x,vp->WorkSize.y));
    if(size<=0)return;
    const float maxX=(std::max)(0.f,vp->WorkSize.x-size),maxY=(std::max)(0.f,vp->WorkSize.y-size);
    const float x=Config::watermark_x<0?(std::max)(0.f,maxX-20):std::clamp(float(Config::watermark_x),0.f,maxX);
    const float y=std::clamp(float(Config::watermark_y),0.f,maxY);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+x,vp->WorkPos.y+y));
    ImGui::SetNextWindowSize(ImVec2(size,size));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    auto flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings
        |ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing;
    if(!Menu_Enabled)flags|=ImGuiWindowFlags_NoInputs;
    if(ImGui::Begin("##logo_watermark",nullptr,flags)){
        const auto pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##drag_logo",ImVec2(size,size));
        if(Menu_Enabled&&ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
            Config::watermark_x=int(std::clamp(x+ImGui::GetIO().MouseDelta.x,0.f,maxX));
            Config::watermark_y=int(std::clamp(y+ImGui::GetIO().MouseDelta.y,0.f,maxY));
        }
        ImGui::GetWindowDrawList()->AddImage(texture,pos,ImVec2(pos.x+size,pos.y+size));
    }
    ImGui::End();ImGui::PopStyleVar();
}
}
