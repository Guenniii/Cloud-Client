#include "seedcracker_bridge.hpp"
#include "seedcracker_client_task_bytes.hpp"
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstring>
#include <sstream>
#include <mutex>

namespace SeedCracker {

    static std::mutex g_init_mutex;
    static jclass task_class=nullptr;
    static jmethodID request_method=nullptr;
    static jobject g_classloader = nullptr;
    static jclass g_bridge_class = nullptr;
    static jmethodID g_scan_chunk_method = nullptr;
    static jmethodID g_scan_player_chunk_method = nullptr;
    static jmethodID g_scan_confirmed_method = nullptr;
    static jmethodID g_scan_treasure_method = nullptr;
    static jmethodID g_init_cracking_method = nullptr;
    static jmethodID g_reset_cracking_method = nullptr;
    static jmethodID g_tick_cracking_method = nullptr;
    static jmethodID g_get_status_method = nullptr;
    static jmethodID g_set_hashed_seed_method = nullptr;
    static jmethodID g_finalize_method = nullptr;

    JNIEnv* GetMinecraftJNIEnv( ) {
        JavaVM* vms[1] = {nullptr};
        jsize vm_count = 0;

        if (JNI_GetCreatedJavaVMs(vms, 1, &vm_count) != JNI_OK || vm_count == 0) {
            std::printf("[SeedCracker] Fehler: Keine aktive Java-VM gefunden!\n");
            return nullptr;
        }

        JavaVM* vm = vms[0];
        JNIEnv* env = nullptr;

        jint res = vm->GetEnv((void**)&env, JNI_VERSION_1_8);
        if (res == JNI_EDETACHED) {
            if (vm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) {
                std::printf("[SeedCracker] Fehler: AttachCurrentThread fehlgeschlagen!\n");
                return nullptr;
            }
        } else if (res != JNI_OK) {
            std::printf("[SeedCracker] Fehler: GetEnv fehlgeschlagen mit Code %d\n", res);
            return nullptr;
        }

        return env;
    }

    static jobject FindGameClassLoader(JNIEnv* env) {
        jclass threadCls = env->FindClass("java/lang/Thread");
        jmethodID getAllStackTraces = env->GetStaticMethodID(threadCls, "getAllStackTraces",
                                                             "()Ljava/util/Map;");
        jobject traces = env->CallStaticObjectMethod(threadCls, getAllStackTraces);

        jclass mapCls = env->FindClass("java/util/Map");
        jmethodID keySetMid = env->GetMethodID(mapCls, "keySet", "()Ljava/util/Set;");
        jobject keySet = env->CallObjectMethod(traces, keySetMid);

        jclass setCls = env->FindClass("java/util/Set");
        jmethodID toArrayMid = env->GetMethodID(setCls, "toArray", "()[Ljava/lang/Object;");
        jobjectArray threads = (jobjectArray)env->CallObjectMethod(keySet, toArrayMid);

        jmethodID getCCL = env->GetMethodID(threadCls, "getContextClassLoader",
                                            "()Ljava/lang/ClassLoader;");
        jclass clCls = env->FindClass("java/lang/ClassLoader");
        jmethodID loadClassMid = env->GetMethodID(clCls, "loadClass",
                                                  "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring mcName = env->NewStringUTF("net.minecraft.client.Minecraft");

        jobject gameLoader = nullptr;
        const jsize n = env->GetArrayLength(threads);
        for (jsize i = 0; i < n; ++i) {
            jobject t = env->GetObjectArrayElement(threads, i);
            if (!t)
                continue;

            jobject cl = env->CallObjectMethod(t, getCCL);
            if (env->ExceptionCheck( )) {
                env->ExceptionClear( );
                continue;
            }
            if (!cl)
                continue;

            jobject mcCls = env->CallObjectMethod(cl, loadClassMid, mcName);
            if (env->ExceptionCheck( )) {
                env->ExceptionClear( );
                continue;
            }
            if (mcCls) {
                gameLoader = cl;
                env->DeleteLocalRef(mcCls);
                break;
            }
        }
        env->DeleteLocalRef(mcName);
        return gameLoader;
    }

    bool Init(JNIEnv* env, const std::string& bridgeJarPath) {
        std::lock_guard<std::mutex> lock(g_init_mutex);
        if (!env) {
            std::printf("[SeedCracker] Init-Fehler: JNIEnv ist nullptr!\n");
            return false;
        }

        if (g_bridge_class && g_init_cracking_method) {
            return true;
        }

        std::printf("[SeedCracker] Lade Bridge-Jar von: %s\n", bridgeJarPath.c_str( ));

        jclass fileClass = env->FindClass("java/io/File");
        jmethodID fileCtor = env->GetMethodID(fileClass, "<init>", "(Ljava/lang/String;)V");
        jstring jPath = env->NewStringUTF(bridgeJarPath.c_str( ));
        jobject fileObj = env->NewObject(fileClass, fileCtor, jPath);
        if (!fileObj) {
            std::printf("[SeedCracker] Fehler: Konnte java.io.File Objekt nicht erstellen.\n");
            return false;
        }

        jmethodID toURI = env->GetMethodID(fileClass, "toURI", "()Ljava/net/URI;");
        jobject uriObj = env->CallObjectMethod(fileObj, toURI);

        jclass uriClass = env->FindClass("java/net/URI");
        jmethodID toURL = env->GetMethodID(uriClass, "toURL", "()Ljava/net/URL;");
        jobject urlObj = env->CallObjectMethod(uriObj, toURL);
        if (env->ExceptionCheck( )) {
            std::printf("[SeedCracker] Ausnahme beim Erstellen der Jar-URL.\n");
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return false;
        }

        jobject gameLoader = FindGameClassLoader(env);
        if (!gameLoader) {
            std::printf("[SeedCracker] Fehler: Kein Thread mit Minecraft-ClassLoader gefunden.\n");
            return false;
        }

        jclass urlClass = env->FindClass("java/net/URL");
        jobjectArray urlArray = env->NewObjectArray(1, urlClass, urlObj);

        jclass urlClassLoaderClass = env->FindClass("java/net/URLClassLoader");
        jmethodID urlClassLoaderCtor = env->GetMethodID(urlClassLoaderClass, "<init>",
                                                        "([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
        jobject classLoader = env->NewObject(urlClassLoaderClass, urlClassLoaderCtor, urlArray, gameLoader);
        if (!classLoader || env->ExceptionCheck( )) {
            std::printf("[SeedCracker] Fehler beim Initialisieren des URLClassLoader.\n");
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return false;
        }
        g_classloader = env->NewGlobalRef(classLoader);

        jmethodID loadClass = env->GetMethodID(urlClassLoaderClass, "loadClass",
                                               "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring className = env->NewStringUTF("phantomui.seedcracker.SeedCrackerBridge");
        jobject bridgeClassObj = env->CallObjectMethod(g_classloader, loadClass, className);
        if (!bridgeClassObj || env->ExceptionCheck( )) {
            std::printf("[SeedCracker] Fehler: Klasse SeedCrackerBridge nicht ladbar.\n");
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return false;
        }
        g_bridge_class = (jclass)env->NewGlobalRef(bridgeClassObj);

        g_scan_chunk_method = env->GetStaticMethodID(g_bridge_class,
                                                     "scanChunkForShipwreckCandidates", "(II)Ljava/lang/String;");
        g_scan_confirmed_method = env->GetStaticMethodID(g_bridge_class,
                                                         "scanChunkForConfirmedShipwrecks", "(II)Ljava/lang/String;");
        g_scan_treasure_method = env->GetStaticMethodID(g_bridge_class,
                                                        "scanChunkForBuriedTreasure", "(II)Ljava/lang/String;");
        g_init_cracking_method = env->GetStaticMethodID(g_bridge_class, "initCracking", "()V");
        g_reset_cracking_method = env->GetStaticMethodID(g_bridge_class, "resetCracking", "()V");
        g_tick_cracking_method = env->GetStaticMethodID(g_bridge_class, "tickCracking", "()V");
        g_get_status_method = env->GetStaticMethodID(g_bridge_class,
                                                     "getCrackingStatus", "()Ljava/lang/String;");
        g_set_hashed_seed_method = env->GetStaticMethodID(g_bridge_class, "setHashedSeed", "(J)V");
        g_finalize_method = env->GetStaticMethodID(g_bridge_class, "finalizeWorldSeedCandidates", "()V");

        if (env->ExceptionCheck( )) {
            env->ExceptionClear( );
        }

        if (!g_scan_chunk_method || !g_scan_confirmed_method || !g_init_cracking_method || !g_tick_cracking_method || !g_get_status_method) {
            std::printf("[SeedCracker] Fehler: Konnte Pflicht-Methoden nicht finden.\n");
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return false;
        }

        if (!g_scan_treasure_method) {
            std::printf("[SeedCracker] scanChunkForBuriedTreasure optional fehlt.\n");
        }
        if (!g_reset_cracking_method) {
            std::printf("[SeedCracker] resetCracking optional fehlt.\n");
        }
        if (!g_set_hashed_seed_method) {
            std::printf("[SeedCracker] setHashedSeed optional fehlt.\n");
        }
        if (!g_finalize_method) {
            std::printf("[SeedCracker] finalizeWorldSeedCandidates optional fehlt.\n");
        }

        g_scan_player_chunk_method = env->GetStaticMethodID(g_bridge_class,
                                                            "scanPlayerChunkForShipwreckCandidates", "()Ljava/lang/String;");
        if (env->ExceptionCheck( )) {
            env->ExceptionClear( );
            g_scan_player_chunk_method = nullptr;
            std::printf("[SeedCracker] scanPlayerChunkForShipwreckCandidates nicht gefunden (optional, wird uebersprungen).\n");
        }

        std::printf("[SeedCracker] Erfolgreich initialisiert und bereit!\n");
        return true;
    }

    bool RequestClientUpdate(JNIEnv* env, bool enabled) {
        if (!env || !g_bridge_class || !g_classloader)
            return false;
        auto failed = [&]() {
            if (!env->ExceptionCheck()) return false;
            std::printf("[SeedCracker] Clientthread-Queue JNI-Fehler.\n");
            env->ExceptionDescribe();
            env->ExceptionClear();
            return true;
        };
        if (failed()) return false;
        if (!task_class) {
            jclass local = env->DefineClass(
                "phantomui/seedcracker/NativeClientTask", g_classloader,
                reinterpret_cast<const jbyte*>(kSeedCrackerClientTask),
                static_cast<jsize>(sizeof(kSeedCrackerClientTask)));
            if (failed() || !local) return false;
            task_class = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
            if (failed() || !task_class) return false;
        }
        if (!request_method) {
            request_method = env->GetStaticMethodID(task_class, "request", "(Ljava/lang/Class;Z)V");
            if (failed() || !request_method) return false;
        }
        env->CallStaticVoidMethod(task_class, request_method, g_bridge_class,
            static_cast<jboolean>(enabled ? JNI_TRUE : JNI_FALSE));
        return !failed();
    }

    void Shutdown(JNIEnv* env) {
        if(!env) return;
        if(g_classloader) {
            jclass type=env->GetObjectClass(g_classloader);
            if(type) {
                auto close=env->GetMethodID(type,"close","()V");
                if(close) env->CallVoidMethod(g_classloader,close);
                env->DeleteLocalRef(type);
            }
            if(env->ExceptionCheck()) env->ExceptionClear();
        }
        if(task_class) {env->DeleteGlobalRef(task_class);task_class=nullptr;}
        if(g_bridge_class) {env->DeleteGlobalRef(g_bridge_class);g_bridge_class=nullptr;}
        if(g_classloader) {env->DeleteGlobalRef(g_classloader);g_classloader=nullptr;}
    }

    jclass LoadExtensionClass(JNIEnv* env, const char* dottedName) {
        std::lock_guard<std::mutex> lock(g_init_mutex);
        if (!env || !g_classloader) return nullptr;
        jclass cls = env->GetObjectClass(g_classloader);
        if (!cls) return nullptr;
        auto method = env->GetMethodID(cls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        env->DeleteLocalRef(cls);
        if (!method) return nullptr;
        jstring name = env->NewStringUTF(dottedName);
        if (!name) return nullptr;
        auto result = static_cast<jclass>(env->CallObjectMethod(g_classloader, method, name));
        env->DeleteLocalRef(name);
        return result;
    }

    bool InitCracking(JNIEnv* env) {
        if (!env || !g_bridge_class || !g_init_cracking_method) {
            std::printf("[SeedCracker] InitCracking abgebrochen: ungültiger Zustand.\n");
            return false;
        }
        std::printf("[SeedCracker] Initialisiere Cracking-Pipeline (Features/DataStorage)...\n");
        env->CallStaticVoidMethod(g_bridge_class, g_init_cracking_method);
        if (env->ExceptionCheck( )) {
            std::printf("[SeedCracker] Ausnahme während initCracking.\n");
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return false;
        }
        std::printf("[SeedCracker] Cracking-Pipeline bereit.\n");
        return true;
    }

    void ResetCracking(JNIEnv* env) {
        if (!env || !g_bridge_class || !g_reset_cracking_method) {
            std::printf("[SeedCracker] ResetCracking: Methode nicht verfügbar.\n");
            return;
        }
        env->CallStaticVoidMethod(g_bridge_class, g_reset_cracking_method);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
        }
        std::printf("[SeedCracker] ResetCracking aufgerufen.\n");
    }

    void TickCracking(JNIEnv* env) {
        if (!env || !g_bridge_class || !g_tick_cracking_method)
            return;
        env->CallStaticVoidMethod(g_bridge_class, g_tick_cracking_method);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
        }
    }

    void SetHashedSeed(JNIEnv* env, long long hashedSeed) {
        if (!env || !g_bridge_class || !g_set_hashed_seed_method)
            return;
        env->CallStaticVoidMethod(g_bridge_class, g_set_hashed_seed_method, (jlong)hashedSeed);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
        }
    }

    void FinalizeWorldSeedCandidates(JNIEnv* env) {
        if (!env || !g_bridge_class || !g_finalize_method)
            return;
        env->CallStaticVoidMethod(g_bridge_class, g_finalize_method);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
        }
    }

    CrackingStatus GetCrackingStatus(JNIEnv* env) {
        CrackingStatus status{ };
        if (!env || !g_bridge_class || !g_get_status_method)
            return status;

        jstring jresult = (jstring)env->CallStaticObjectMethod(g_bridge_class, g_get_status_method);
        if (env->ExceptionCheck( ) || !jresult) {
            if (env->ExceptionCheck( )) {
                env->ExceptionDescribe( );
                env->ExceptionClear( );
            }
            return status;
        }

        const char* cstr = env->GetStringUTFChars(jresult, nullptr);
        std::string data(cstr ? cstr : "");
        env->ReleaseStringUTFChars(jresult, cstr);
        env->DeleteLocalRef(jresult);

        double b = 0, l = 0, w = 0;
        int wc = 0;
        char stateBuf[512] = {0};
        if (sscanf(data.c_str( ), "%lf,%lf,%lf,%d,%511s", &b, &l, &w, &wc, stateBuf) < 4)
            return status;

        status.baseBits = b;
        status.liftingBits = l;
        status.wantedBits = w;
        status.wrackCount = wc;

        if (strcmp(stateBuf, "IDLE") == 0) {
            status.state = CrackState::IDLE;
        } else if (strcmp(stateBuf, "COLLECTING") == 0) {
            status.state = CrackState::COLLECTING;
        } else if (strcmp(stateBuf, "FAILED") == 0) {
            status.state = CrackState::FAILED;
        } else if (strncmp(stateBuf, "MULTIPLE:", 9) == 0) {
            status.state = CrackState::MULTIPLE;
            status.candidates = std::atoi(stateBuf + 9);
        } else if (strncmp(stateBuf, "FOUND:", 6) == 0) {
            status.state = CrackState::FOUND;
            status.hasSeed = true;
            status.seed = std::stoll(stateBuf + 6);
        //    std::printf("[SeedCracker] >>> WELTSEED: %lld <<<\n", status.seed);
        } else if (strncmp(stateBuf, "CANDIDATES:", 11) == 0) {
            status.state = CrackState::CANDIDATES;
            // CANDIDATES:<count>:<seed1;seed2;...>
            int count = 0;
            const char* p = stateBuf + 11;
            count = std::atoi(p);
            status.candidates = count;
            const char* colon = strchr(p, ':');
            if (colon) {
                std::istringstream ss(colon + 1);
                std::string tok;
                while (std::getline(ss, tok, ';')) {
                    if (tok.empty( ))
                        continue;
                    try {
                        status.candidateSeeds.push_back(std::stoll(tok));
                    } catch (...) {
                    }
                }
            }
            std::printf("[SeedCracker] %d World-Seed-Kandidaten\n", status.candidates);
            for (long long s : status.candidateSeeds)
                std::printf("[SeedCracker]   CANDIDATE: %lld\n", s);
        } else if (strncmp(stateBuf, "STRUCTURE:", 10) == 0) {
            status.state = CrackState::STRUCTURE;
            int sc = 0;
            long long first = 0;
            int bc = 0;
            sscanf(stateBuf + 10, "%d:%lld:biomes=%d", &sc, &first, &bc);
            status.structureSeedCount = sc;
            status.biomeCount = bc;
        }

        return status;
    }

    static std::vector<ChestHit> ParseHits(JNIEnv* env, jstring jresult) {
        std::vector<ChestHit> results;
        if (!jresult)
            return results;

        const char* cstr = env->GetStringUTFChars(jresult, nullptr);
        std::string data(cstr ? cstr : "");
        env->ReleaseStringUTFChars(jresult, cstr);
        env->DeleteLocalRef(jresult);

        std::istringstream stream(data);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty( ))
                continue;
            ChestHit hit{ };
            if (sscanf(line.c_str( ), "%d,%d,%d", &hit.x, &hit.y, &hit.z) == 3)
                results.push_back(hit);
        }
        return results;
    }

    std::vector<ChestHit> ScanChunkForShipwreckCandidates(JNIEnv* env, int chunkX, int chunkZ) {
        if (!env || !g_bridge_class || !g_scan_chunk_method)
            return { };
        jstring jresult = (jstring)env->CallStaticObjectMethod(
            g_bridge_class, g_scan_chunk_method, chunkX, chunkZ);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return { };
        }
        return ParseHits(env, jresult);
    }

    std::vector<ChestHit> ScanPlayerChunkForShipwreckCandidates(JNIEnv* env) {
        if (!env || !g_bridge_class || !g_scan_player_chunk_method)
            return { };
        jstring jresult = (jstring)env->CallStaticObjectMethod(
            g_bridge_class, g_scan_player_chunk_method);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return { };
        }
        return ParseHits(env, jresult);
    }

    static std::vector<ConfirmedShipwreck> ParseConfirmed(JNIEnv* env, jstring jresult) {
        std::vector<ConfirmedShipwreck> results;
        if (!jresult)
            return results;

        const char* cstr = env->GetStringUTFChars(jresult, nullptr);
        std::string data(cstr ? cstr : "");
        env->ReleaseStringUTFChars(jresult, cstr);
        env->DeleteLocalRef(jresult);

        std::istringstream stream(data);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty( ))
                continue;
            ConfirmedShipwreck hit{ };
            if (sscanf(line.c_str( ), "%d,%d,%d,%d,%d",
                       &hit.chestX, &hit.chestY, &hit.chestZ,
                       &hit.regionChunkX, &hit.regionChunkZ) == 5)
                results.push_back(hit);
        }
        return results;
    }

    std::vector<ConfirmedShipwreck> ScanChunkForConfirmedShipwrecks(JNIEnv* env, int chunkX, int chunkZ) {
        if (!env || !g_bridge_class || !g_scan_confirmed_method)
            return { };
        jstring jresult = (jstring)env->CallStaticObjectMethod(
            g_bridge_class, g_scan_confirmed_method, chunkX, chunkZ);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return { };
        }
        return ParseConfirmed(env, jresult);
    }

    std::vector<ChestHit> ScanChunkForBuriedTreasure(JNIEnv* env, int chunkX, int chunkZ) {
        if (!env || !g_bridge_class || !g_scan_treasure_method)
            return { };
        jstring jresult = (jstring)env->CallStaticObjectMethod(
            g_bridge_class, g_scan_treasure_method, chunkX, chunkZ);
        if (env->ExceptionCheck( )) {
            env->ExceptionDescribe( );
            env->ExceptionClear( );
            return { };
        }
        return ParseHits(env, jresult);
    }

} // namespace SeedCracker
