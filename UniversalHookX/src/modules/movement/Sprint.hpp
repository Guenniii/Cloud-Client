#ifndef SPRINT_HPP
#define SPRINT_HPP

#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <iostream>

class AutoSprint {
public:
    static void Run(JNIEnv* env, CMinecraft* mc) {
        if (!env || !mc || !mc->IsInitialized( ))
            return;

        jobject mc_inst = mc->GetInstance( );
        jobject playerObj = env->GetObjectField(mc_inst, mc->f_player);
        if (!playerObj)
            return;

        jobject inputObj = env->GetObjectField(playerObj, mc->f_input);
        if (inputObj) {
            jobject moveVectorObj = env->GetObjectField(inputObj, mc->f_move_vector);
            if (moveVectorObj) {

                // Vorwärts-Impuls aus Vec2 ('y' Field) auslesen
                jfloat forwardImpulse = env->GetFloatField(moveVectorObj, mc->f_vec2_y);

                // Sneaking Status abfragen
                jboolean isSneaking = env->CallBooleanMethod(playerObj, mc->m_is_shifting);

                // Sprint aktivieren wenn der Spieler sich vorwärts bewegt und nicht sneakt
                if (forwardImpulse > 0.0f && !isSneaking) {
                    if (mc->m_set_sprinting) {
                        env->CallVoidMethod(playerObj, mc->m_set_sprinting, JNI_TRUE);
                    }
                }
                env->DeleteLocalRef(moveVectorObj);
            }
            env->DeleteLocalRef(inputObj);
        }
        env->DeleteLocalRef(playerObj);
    }
};

#endif
