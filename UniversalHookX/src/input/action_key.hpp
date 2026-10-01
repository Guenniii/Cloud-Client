#pragma once
namespace Input {
class ActionKey {
    int key_=0;
    bool down_=false;
public:
    bool Press(int key,bool down,bool blocked) {
        if (key!=key_) { key_=key; down_=down; return false; }
        const bool pressed=down && !down_;
        down_=down;
        return key>0 && !blocked && pressed;
    }
};
}
