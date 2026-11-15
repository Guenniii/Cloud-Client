#pragma once
#include <algorithm>
#include <array>
namespace LootBeaconRules {
struct Selection {bool enabled;int range;std::array<bool,3> groups;};
inline Selection Merge(const Selection& a,const Selection& b) {
 return {a.enabled||b.enabled,(std::max)(a.enabled?a.range:0,b.enabled?b.range:0),
  {(a.enabled&&a.groups[0])||(b.enabled&&b.groups[0]),(a.enabled&&a.groups[1])||(b.enabled&&b.groups[1]),(a.enabled&&a.groups[2])||(b.enabled&&b.groups[2])}};
}
inline bool Includes(const Selection& selection,int category,double distanceSquared) {
 return selection.enabled && category>=0 && category<3 && selection.groups[category]
  && distanceSquared>=0 && distanceSquared<=double(selection.range)*selection.range;
}
}
