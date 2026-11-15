#pragma once
#include <array>
#include <algorithm>

namespace Menu::Navigation {
struct Rect {float x=0,y=0,w=0,h=0;};
struct Layout {
    std::array<Rect,9> tabs;
    std::array<Rect,2> groups;
    std::array<bool,9> iconsOnly{};
    float height=36,padding=10,gap=10,iconScale=1;
};
inline Layout Arrange(const std::array<float,9>& widths,float available,
                      const std::array<float,9>& iconWidths){
    available=(std::max)(available,1.f);Layout result;auto selected=widths;
    float between=14;
    const auto total=[&](){float sum=16+between+result.gap*7;for(float w:selected)sum+=w;return sum;};
    if(total()>available){
        result.padding=6;result.gap=6;between=10;
        for(int i=0;i<9;i++)selected[i]=(std::max)(iconWidths[i],widths[i]-8);
    }
    if(total()>available){
        for(int i=4;i<9;i++){result.iconsOnly[i]=true;selected[i]=iconWidths[i];}
    }
    if(total()>available){
        for(int i=0;i<4;i++){result.iconsOnly[i]=true;selected[i]=iconWidths[i];}
    }
    const float scale=(std::min)(1.f,available/total());result.iconScale=scale;
    result.padding*=scale;result.gap*=scale;between*=scale;
    float x=0;
    for(int group=0;group<2;group++){
        const int first=group?4:0,last=group?9:4;const float start=x;x+=4*scale;
        for(int i=first;i<last;i++){
            result.tabs[i]={x,4,selected[i]*scale,28};x+=selected[i]*scale;
            if(i+1<last)x+=result.gap;
        }
        x+=4*scale;result.groups[group]={start,0,x-start,36};
        if(!group)x+=between;
    }
    return result;
}
}
