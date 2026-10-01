#include "../theme/theme.hpp"
#pragma once
#include "../oresim/oresim.hpp"
#include <algorithm>
#include "glow.hpp"
#include "../../modules/settings.hpp"

namespace PlayerSkinGeometry {
using Point = OreSim::Point;
inline int ClipPolygon(Point* polygon, int size) {
    for(int plane=0;plane<5 && size;++plane) {
        Point result[16]; int count=0;
        auto distance=[plane](Point p) {
            switch(plane) {case 0:return p.w-.001;case 1:return p.w+p.x;
            case 2:return p.w-p.x;case 3:return p.w+p.y;default:return p.w-p.y;}
        };
        Point a=polygon[size-1]; double da=distance(a);
        for(int i=0;i<size;++i) {
            Point b=polygon[i]; double db=distance(b);
            if(!std::isfinite(da) || !std::isfinite(db)) return 0;
            if((da>=0)!=(db>=0)) {
                double t=da/(da-db);
                result[count++]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.w+(b.w-a.w)*t};
            }
            if(db>=0) result[count++]=b;
            a=b; da=db;
        }
        size=count; std::copy(result,result+count,polygon);
    }
    return size;
}
inline void Draw(ImDrawList* draw, ImGuiViewport* vp, const std::vector<double>& data) {
    if(data.size()<19 || data.size()>19+60000*13 || (data.size()-19)%13) return;
    struct Quad {Point p[4]; ImU32 color; double depth;};
    std::vector<Quad> quads; quads.reserve((data.size()-19)/13);
    for(size_t i=19;i<data.size();i+=13) {
        Quad q{}; bool valid=true;
        for(int k=0;k<4;++k) {
            q.p[k]=OreSim::Project(data.data(),data[i+k*3],data[i+k*3+1],data[i+k*3+2]);
            valid &= std::isfinite(q.p[k].x)&&std::isfinite(q.p[k].y)&&std::isfinite(q.p[k].w);
            q.depth+=q.p[k].w*.25;
        }
        double color=data[i+12];
        if(!valid || !std::isfinite(color) || color<0 || color>4294967295.0) continue;
        auto argb=static_cast<unsigned int>(color);
        q.color=Theme::SkinTint(IM_COL32((argb>>16)&255,(argb>>8)&255,argb&255,(argb>>24)&255));
        quads.push_back(q);
    }
    std::stable_sort(quads.begin(),quads.end(),[](const Quad& a,const Quad& b){return a.depth>b.depth;});
    // A coarse union mask produces a halo around the skin silhouette, including bent limbs.
    // Adjacent skin pixels share one halo; no hitbox and no per-pixel glow draw calls.
    struct Polygon { ImVec2 points[16]; int n; ImU32 color; };
    std::vector<Polygon> polygons;polygons.reserve(quads.size());
    const float cell=(std::max)(3.0f,vp->Size.x/384.0f);
    const int width=(std::max)(1,int(std::ceil(vp->Size.x/cell))),height=(std::max)(1,int(std::ceil(vp->Size.y/cell)));
    std::vector<unsigned char> mask;
    if(PlayerESP_Glow) mask.resize(size_t(width)*height);
    for(auto& q:quads) {
        Point clipped[16];std::copy(q.p,q.p+4,clipped);
        int n=ClipPolygon(clipped,4);if(n<3) continue;
        Polygon poly{};poly.n=n;poly.color=q.color;
        float low=vp->Size.y,high=0;
        for(int i=0;i<n;++i) {
            poly.points[i]=ImVec2(float(vp->Pos.x+(clipped[i].x/clipped[i].w+1)*vp->Size.x*.5),
                float(vp->Pos.y+(1-clipped[i].y/clipped[i].w)*vp->Size.y*.5));
            low=(std::min)(low,poly.points[i].y-vp->Pos.y);high=(std::max)(high,poly.points[i].y-vp->Pos.y);
        }
        if(!mask.empty()) for(int y=std::clamp(int(low/cell),0,height-1);y<=std::clamp(int(high/cell),0,height-1);++y) {
            float scan=vp->Pos.y+(y+.5f)*cell,left=vp->Size.x,right=0;bool found=false;
            for(int i=0;i<n;++i) {
                auto a=poly.points[i],b=poly.points[(i+1)%n];
                if((a.y<=scan && b.y>scan) || (b.y<=scan && a.y>scan)) {
                    float x=a.x+(scan-a.y)*(b.x-a.x)/(b.y-a.y)-vp->Pos.x;
                    left=(std::min)(left,x);right=(std::max)(right,x);found=true;
                }
            }
            if(found) {
                int a=std::clamp(int(left/cell),0,width-1),b=std::clamp(int(right/cell),0,width-1);
                std::fill(mask.begin()+size_t(y)*width+a,mask.begin()+size_t(y)*width+b+1,1);
            }
        }
        double area=0;for(int i=0;i<n;++i) {auto a=poly.points[i],b=poly.points[(i+1)%n];area+=double(a.x)*b.y-double(a.y)*b.x;}
        if(area<0) std::reverse(poly.points,poly.points+n);
        polygons.push_back(poly);
    }
    auto savedFlags=draw->Flags;
    draw->Flags &= ~ImDrawListFlags_AntiAliasedFill; // Adjacent skin pixels must meet without transparent seams.
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    if(!mask.empty()) {
        auto occupied=[&](int x,int y) {return x>=0 && x<width && y>=0 && y<height && mask[size_t(y)*width+x]!=0;};
        // Merge straight mask edges to keep the geometry bounded even with detailed skins.
        for(int y=0;y<=height;++y) for(int x=0;x<width;) {
            if(occupied(x,y)==occupied(x,y-1)) {++x;continue;}
            int start=x++;while(x<width && occupied(x,y)!=occupied(x,y-1)) ++x;
            EspGlow::Line(draw,ImVec2(vp->Pos.x+start*cell,vp->Pos.y+y*cell),ImVec2(vp->Pos.x+x*cell,vp->Pos.y+y*cell),1,.75f);
        }
        for(int x=0;x<=width;++x) for(int y=0;y<height;) {
            if(occupied(x,y)==occupied(x-1,y)) {++y;continue;}
            int start=y++;while(y<height && occupied(x,y)!=occupied(x-1,y)) ++y;
            EspGlow::Line(draw,ImVec2(vp->Pos.x+x*cell,vp->Pos.y+start*cell),ImVec2(vp->Pos.x+x*cell,vp->Pos.y+y*cell),1,.75f);
        }
    }
    for(auto& poly:polygons) draw->AddConvexPolyFilled(poly.points,poly.n,poly.color);
    draw->PopClipRect();draw->Flags=savedFlags;
}
}
