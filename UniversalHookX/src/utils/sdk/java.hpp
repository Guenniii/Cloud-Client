
#pragma once

#include <memory>

#include <Windows.h>
#include "../../dependencies/jni/jni.h"

#include "CMinecraft.h"
#include "CFabric.h"
#include "game_runtime.hpp"



class JNI final {
public:
    JNI( ) {
        /* Init main pointers */
        {
            jint result = JNI_GetCreatedJavaVMs(&p_jvm, 1, nullptr);

            if (result != 0) {
                printf("[-] JNI() failed to initialize p_jvm\n");
                MessageBoxA(0, "ERROR", "Check console", MB_ICONASTERISK);
            }

            p_jvm->AttachCurrentThread((void**)&p_env, nullptr);
        }

        /* Init game classes */
        {
            p_cminecraft = std::make_unique<CMinecraft>(p_jvm);
        //    p_cfabric = std::make_unique<CFabric>(p_jvm);
        }

        is_init = p_cminecraft && p_cminecraft->IsInitialized();
    }

    ~JNI( ) {
        p_cminecraft.reset();
        p_cfabric.reset();
        GameRuntime::Shutdown(GetEnv());
        if(p_jvm) p_jvm->DetachCurrentThread( );

        is_init = false;
    }

    bool GetInit( ) {
        return is_init;
    }

    JNIEnv* GetEnv( ) {
        JNIEnv* current=nullptr;
        if(!p_jvm)return nullptr;
        auto result=p_jvm->GetEnv(reinterpret_cast<void**>(&current),JNI_VERSION_1_8);
        if(result==JNI_EDETACHED && p_jvm->AttachCurrentThread(reinterpret_cast<void**>(&current),nullptr)!=JNI_OK)return nullptr;
        return current;
    }

    // In der JNI Klasse unter GetEnv():
    JavaVM* GetJVM( ) {
        return p_jvm;
    }
    
public:
    std::unique_ptr<CMinecraft> p_cminecraft;
    std::unique_ptr<CFabric> p_cfabric;
private:
    JavaVM* p_jvm;
    JNIEnv* p_env;

    bool is_init{false};
};

inline std::unique_ptr<JNI> p_jni;
