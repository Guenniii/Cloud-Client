#pragma once
namespace Rotation {
// Immediate JNI actions cannot be undone later in an update. Dispatch the
// highest-priority module first; a successful claim lasts through the update.
template<class Owner> struct Ownership {
    Owner claimed{}, active{};
    void Begin() { claimed=Owner{}; }
    bool Claim(Owner owner) {
        if(owner==Owner{} || (claimed!=Owner{} && claimed!=owner)) return false;
        claimed=owner;
        return true;
    }
    bool Release(Owner owner) {
        if(active!=owner) return false;
        active=Owner{};
        return true;
    }
};
}
