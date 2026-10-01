#pragma once
#include "targeting.hpp"
#include <algorithm>
#include <limits>

namespace Rotation {
struct PlayerTarget { int id; Angles angles; double angle; };
// FOV is the full cone angle. Keep a valid previous target to avoid switching
// between nearby players as their relative angle changes slightly.
inline std::optional<PlayerTarget> ScorePlayer(int id, Point eye, Point target,
                                              Angles camera, double range, double fov) {
    const auto angles = LookAt(eye, target);
    const double distance = std::hypot(target.x-eye.x, target.y-eye.y, target.z-eye.z);
    if (!angles || !std::isfinite(distance) || distance>range || range<=0
        || !std::isfinite(camera.yaw) || !std::isfinite(camera.pitch)
        || !std::isfinite(range) || !std::isfinite(fov) || fov<=0) return {};
    constexpr double radians=3.14159265358979323846/180.0;
    const double p=camera.pitch*radians, t=angles->pitch*radians;
    const double dot=std::sin(p)*std::sin(t)+std::cos(p)*std::cos(t)*std::cos((angles->yaw-camera.yaw)*radians);
    const double angle=std::acos(std::clamp(dot,-1.0,1.0))/radians;
    if (angle>fov*0.5+1e-6) return {};
    return PlayerTarget{id,*angles,angle};
}
inline void ConsiderPlayer(std::optional<PlayerTarget>& best, PlayerTarget candidate, std::optional<int> previous) {
    if (best && previous && best->id==*previous) return;
    if (!best || (previous && candidate.id==*previous) || candidate.angle<best->angle
        || (candidate.angle==best->angle && candidate.id<best->id)) best=candidate;
}
}
