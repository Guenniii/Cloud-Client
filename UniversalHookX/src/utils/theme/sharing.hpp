#pragma once
#include "theme.hpp"
#include "share_code.hpp"
namespace ThemeSharing {
inline std::string Export(){
 return ThemeShareCode::Encode({Theme::presets[std::clamp(Theme::selected,0,Theme::Count-1)].name,
  {unsigned(Theme::custom_accent_enabled.load()),unsigned(Theme::custom_accent_rgb.load()),
   unsigned(Theme::custom_background_enabled.load()),unsigned(Theme::custom_background_rgb.load()),
   unsigned(Theme::menu_glow_percent.load()),unsigned(Theme::esp_glow_percent.load()),unsigned(Theme::gradient_enabled.load()),
   unsigned(Theme::secondary_rgb.load()),unsigned(Theme::gradient_strength.load()),unsigned(Theme::animated_accent.load()),
   unsigned(Theme::animation_period.load()),unsigned(Theme::player_colors_enabled.load()),unsigned(Theme::friend_rgb.load()),unsigned(Theme::enemy_rgb.load())}});
}
inline bool Import(std::string_view code,std::string& status){
 const auto design=ThemeShareCode::Decode(code);
 if(!design){status="Invalid or damaged theme code. Nothing changed.";return false;}
 int selected=-1;
 for(int i=0;i<Theme::Count;++i)if(design->preset==Theme::presets[i].name){selected=i;break;}
 if(selected<0){status="This preset is not available in this client version.";return false;}
 const auto& v=design->values;
 Theme::selected=selected;Theme::custom_accent_enabled=v[0]!=0;Theme::custom_accent_rgb=v[1];
 Theme::custom_background_enabled=v[2]!=0;Theme::custom_background_rgb=v[3];Theme::menu_glow_percent=v[4];Theme::esp_glow_percent=v[5];
 Theme::gradient_enabled=v[6]!=0;Theme::secondary_rgb=v[7];Theme::gradient_strength=v[8];Theme::animated_accent=v[9]!=0;
 Theme::animation_period=v[10];Theme::player_colors_enabled=v[11]!=0;Theme::friend_rgb=v[12];Theme::enemy_rgb=v[13];
 Theme::ApplyStyle();status="Theme imported. Save your profile to keep it.";return true;
}
}
