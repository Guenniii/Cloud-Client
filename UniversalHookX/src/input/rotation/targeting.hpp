#pragma once
#include <cmath>
#include <optional>
namespace Rotation {
struct Point { double x,y,z; };
struct Angles { float yaw,pitch; };
inline std::optional<Angles> LookAt(Point eye, Point target) {
    const double dx=target.x-eye.x, dy=target.y-eye.y, dz=target.z-eye.z;
    if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz)) return {};
    const double horizontal=std::hypot(dx,dz);
    if(horizontal==0 && dy==0) return {};
    constexpr double degrees=180.0/3.14159265358979323846;
    return Angles{static_cast<float>(std::atan2(dz,dx)*degrees-90.0),
                  static_cast<float>(-std::atan2(dy,horizontal)*degrees)};
}
// Strict bound preserves the existing nearest-crystal selection/tie handling.
inline bool Nearer(Point eye, Point candidate, double& bestDistance) {
    const double distance=std::hypot(candidate.x-eye.x,candidate.y-eye.y,candidate.z-eye.z);
    if(!std::isfinite(distance) || distance>=bestDistance) return false;
    bestDistance=distance;
    return true;
}
}
