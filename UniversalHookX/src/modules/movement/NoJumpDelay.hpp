#ifndef NO_JUMP_DELAY_HPP
#define NO_JUMP_DELAY_HPP

#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <cstdio>

class NoJumpDelay {
public:
    static void Run(JNIEnv* env, CMinecraft* mc) {
        if (!env || !mc || !mc->IsInitialized( ))
            return;

        jobject mc_inst = mc->GetInstance( );
        jobject playerObj = env->GetObjectField(mc_inst, mc->f_player);
        if (!playerObj)
            return;

        if (mc->f_no_jump_delay) {
            env->SetIntField(playerObj, mc->f_no_jump_delay, 0);
        } else {
            std::printf("[-] NoJumpDelay: jumpDelay field ID missing in SDK Cache.\n");
        }

        env->DeleteLocalRef(playerObj);
    }
};

#endif
