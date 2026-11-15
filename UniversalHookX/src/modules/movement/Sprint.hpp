#ifndef SPRINT_HPP
#define SPRINT_HPP

#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <cstdio>
#include <iostream>

class AutoSprint {
public:
    static void Run(JNIEnv* env, CMinecraft* mc) {
        if (!env || !mc || !mc->IsInitialized( ))
            return;
        // IsInitialized() does not cover these IDs; a null ID crashes the JVM.
        if (!mc->f_input || !mc->f_move_vector || !mc->f_vec2_y || !mc->m_is_shifting || !mc->m_set_sprinting) {
            static bool logged = false;
            if (!logged) { std::printf("[-] AutoSprint: SDK-IDs fehlen, Modul inaktiv.\n"); logged = true; }
            return;
        }

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
                if (env->ExceptionCheck()) { env->ExceptionClear(); isSneaking = JNI_TRUE; }

                // Sprint aktivieren wenn der Spieler sich vorwärts bewegt und nicht sneakt
                if (forwardImpulse > 0.0f && !isSneaking) {
                    env->CallVoidMethod(playerObj, mc->m_set_sprinting, JNI_TRUE);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                }
                env->DeleteLocalRef(moveVectorObj);
            }
            env->DeleteLocalRef(inputObj);
        }
        env->DeleteLocalRef(playerObj);
    }
};

#endif
