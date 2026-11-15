#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../input/friends.hpp"
namespace Menu::Pages {
using namespace Menu::Widgets;
void DrawFriendsPage(ImDrawList* draw,ImVec2 area,float width,float height,float alpha) {
    Friends::Load();
    draw->AddRectFilled(area,ImVec2(area.x+width,area.y+height),ColA(Palette::Card(),alpha),12);
    draw->AddRect(area,ImVec2(area.x+width,area.y+height),ColA(Palette::Border(),alpha),12);
    ImGui::SetCursorScreenPos(area);ImGui::BeginChild("FriendsContent",ImVec2(width,height),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    area=ImGui::GetCursorScreenPos();draw=ImGui::GetWindowDrawList();width=ImGui::GetContentRegionAvail().x;
    ImVec2 pos(area.x+20,area.y+18);float rowWidth=width-40;
    draw->AddText(pos,ColA(Palette::Text(),alpha),"Friends");
    draw->AddText(ImVec2(pos.x,pos.y+22),ColA(Palette::Muted(),alpha),"Manage account names. Targeting modules ignore friends.");
    float y=pos.y+54;
    ImGui::PushID("protection");
    DrawCheckboxRow(draw,ImVec2(pos.x,y),rowWidth,"Protect friends",&Friends_Enabled,alpha);y+=32;
    ImGui::PopID();ImGui::PushID("middle_click");
    DrawCheckboxRow(draw,ImVec2(pos.x,y),rowWidth,"Add friends with middle click",&Friends_MiddleClick,alpha);y+=32;
    ImGui::PopID();ImGui::PushID("friend_color");
    DrawColorRow(draw,ImVec2(pos.x,y),rowWidth,"ESP color",&Friends_Color,alpha);y+=42;
    ImGui::PopID();
    static char name[64]={};
    ImGui::SetCursorScreenPos(ImVec2(pos.x,y));ImGui::SetNextItemWidth(rowWidth-115);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(12,9));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ColV(Palette::Background(),alpha));ImGui::PushStyleColor(ImGuiCol_Text,ColV(Palette::Text(),alpha));
    bool add=ImGui::InputTextWithHint("##friend_name","Account name ...",name,sizeof(name),ImGuiInputTextFlags_EnterReturnsTrue);
    auto size=ImGui::GetItemRectSize();ImGui::PopStyleColor(2);ImGui::PopStyleVar(2);
    ImGui::SetCursorScreenPos(ImVec2(pos.x+rowWidth-105,y));
    add|=DrawRoundedButton(draw,"##add_friend","Add friend",ImVec2(105,size.y),Palette::Accent(),Theme::Current().hover,Theme::Current().onAccent,alpha,8,true);
    if(add && Friends::Add(name)){name[0]=0;Friends::Sync();}
    y+=size.y+12;
    const auto& message=Friends::error.empty()?Friends::notice:Friends::error;
    draw->AddText(ImVec2(pos.x,y),ColA(Friends::error.empty()?Palette::Muted():IM_COL32(220,80,100,255),alpha),message.empty()?"Middle-click a player to add them.":message.c_str());y+=32;
    auto names=Friends::Names();char count[48];std::snprintf(count,sizeof(count),"Friends (%zu)",names.size());
    draw->AddText(ImVec2(pos.x,y),ColA(Palette::Label(),alpha),count);y+=28;
    if(names.empty()){draw->AddText(ImVec2(pos.x,y),ColA(Palette::Muted(),alpha),"No friends added yet.");y+=28;}
    for(const auto& account:names) {
        ImGui::PushID(account.c_str());
        draw->AddRectFilled(ImVec2(pos.x,y),ImVec2(pos.x+rowWidth,y+38),ColA(Palette::Background(),alpha),8);
        int rgb=Friends_Color.load();draw->AddCircleFilled(ImVec2(pos.x+14,y+19),3,ColA(IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255),alpha));
        draw->AddText(ImVec2(pos.x+26,y+10),ColA(Palette::Text(),alpha),account.c_str());
        ImGui::SetCursorScreenPos(ImVec2(pos.x+rowWidth-92,y+5));
        if(DrawRoundedButton(draw,"##remove","Remove",ImVec2(84,28),Palette::Soft(),Theme::Current().hover,Palette::Accent(),alpha,6,true)) {Friends::Remove(account);Friends::Sync();}
        ImGui::PopID();y+=46;
    }
    ImGui::SetCursorScreenPos(ImVec2(pos.x,y+8));ImGui::Dummy(ImVec2(1,1));ImGui::EndChild();
}
}
