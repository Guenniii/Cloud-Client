#include "../sdk/game_runtime.hpp"
#include "seedcracker_bridge.hpp"
#include "../../modules/settings.hpp"
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstring>
#include <sstream>
#include <mutex>
#include <chrono>

namespace SeedCracker {

    static std::mutex g_init_mutex;
    static bool g_ready = false;
    static std::chrono::steady_clock::time_point g_retry_after{};

    // ExceptionDescribe writes to Java stderr. Also copy the cause chain to the
    // native console, where injected-client diagnostics are normally read.
    static bool ReportException(JNIEnv* env, const char* operation) {
        auto error = env->ExceptionOccurred();
        if (!error) return false;
        env->ExceptionDescribe();
        env->ExceptionClear();
        auto throwable = env->FindClass("java/lang/Throwable");
        auto text = throwable ? env->GetMethodID(throwable, "toString", "()Ljava/lang/String;") : nullptr;
        auto cause = throwable ? env->GetMethodID(throwable, "getCause", "()Ljava/lang/Throwable;") : nullptr;
        if (env->ExceptionCheck()) env->ExceptionClear();
        std::printf("[Bridge] JNI failure during %s\n", operation);
        for (int depth=0; error && text && cause && depth<8; ++depth) {
            auto value=static_cast<jstring>(env->CallObjectMethod(error,text));
            if(env->ExceptionCheck()){env->ExceptionClear();break;}
            std::printf("[Bridge] %s%s\n",depth?"Caused by: ":"",GameRuntime::String(env,value).c_str());
            if(value)env->DeleteLocalRef(value);
            auto next=env->CallObjectMethod(error,cause);
            if(env->ExceptionCheck()){env->ExceptionClear();break;}
            const bool cycle=next&&env->IsSameObject(next,error);
            env->DeleteLocalRef(error);error=static_cast<jthrowable>(next);
            if(cycle)break;
        }
        if(error)env->DeleteLocalRef(error);
        if(throwable)env->DeleteLocalRef(throwable);
        if(env->ExceptionCheck())env->ExceptionClear();
        return true;
    }
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
    static jmethodID g_configure_scan_method = nullptr;
    static jmethodID g_verified_seeds_method = nullptr;

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

    static jobject FindGameClassLoader(JNIEnv* env) { return GameRuntime::Loader(env); }

    bool Init(JNIEnv* env, const std::string& bridgeJarPath) {
        std::lock_guard<std::mutex> lock(g_init_mutex);
        if (!env) {
            std::printf("[SeedCracker] Init-Fehler: JNIEnv ist nullptr!\n");
            return false;
        }

        if (ReportException(env, "entry to bridge initialization")) return false;
        if (g_ready) return true;
        const auto now=std::chrono::steady_clock::now();
        if(now<g_retry_after)return false;
        g_retry_after=now+std::chrono::seconds(10);
        if(env->PushLocalFrame(96)!=JNI_OK){ReportException(env,"bridge local frame");return false;}
        struct Attempt {
            JNIEnv* env;
            ~Attempt(){
                ReportException(env,"bridge initialization");
                if(!g_ready)Shutdown(env);
                env->PopLocalFrame(nullptr);
            }
        } attempt{env};

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
        const auto runtime=GameRuntime::Info();
        const bool compatibility263=runtime.version=="26.3"||runtime.version.starts_with("26.3-")||runtime.version.starts_with("26.3.");
        jobjectArray urlArray = env->NewObjectArray(compatibility263?2:1, urlClass, urlObj);
        if(compatibility263){
            auto external=env->GetMethodID(urlClass,"toExternalForm","()Ljava/lang/String;");
            auto text=static_cast<jstring>(env->CallObjectMethod(urlObj,external));
            const auto overlay="jar:"+GameRuntime::String(env,text)+"!/compat/26.3/";
            auto ctor=env->GetMethodID(urlClass,"<init>","(Ljava/lang/String;)V");
            auto path=env->NewStringUTF(overlay.c_str());auto url=env->NewObject(urlClass,ctor,path);
            if(env->ExceptionCheck()||!url){env->ExceptionDescribe();env->ExceptionClear();return false;}
            env->SetObjectArrayElement(urlArray,0,url);
        }
        std::printf("[Bridge] Runtime %s: selecting %s classes.\n",runtime.version.c_str(),compatibility263?"26.3 overlay":"26.2 base");

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
        // Bootstrap the loader subclass using only JDK types. Its child-first
        // policy isolates vendored seed libraries from older copies in mods.
        auto bootstrapLoad=env->GetMethodID(urlClassLoaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");
        auto bootstrapName=env->NewStringUTF("phantomui.bootstrap.BridgeClassLoader");
        auto bootstrapType=static_cast<jclass>(env->CallObjectMethod(classLoader,bootstrapLoad,bootstrapName));
        if(ReportException(env,"load isolated bridge loader")||!bootstrapType)return false;
        auto isolatedCtor=env->GetMethodID(bootstrapType,"<init>","([Ljava/net/URL;Ljava/lang/ClassLoader;)V");
        if(ReportException(env,"isolated loader constructor")||!isolatedCtor)return false;
        auto isolatedLoader=env->NewObject(bootstrapType,isolatedCtor,urlArray,gameLoader);
        if(ReportException(env,"create isolated bridge loader")||!isolatedLoader)return false;
        auto closeBootstrap=env->GetMethodID(urlClassLoaderClass,"close","()V");
        if(closeBootstrap)env->CallVoidMethod(classLoader,closeBootstrap);
        if(ReportException(env,"close bridge bootstrap loader"))return false;
        classLoader=isolatedLoader;
        if(compatibility263){
            auto resource=env->GetMethodID(urlClassLoaderClass,"findResource","(Ljava/lang/String;)Ljava/net/URL;");
            auto name=env->NewStringUTF("phantomui/seedcracker/SeedCrackerBridge.class");
            auto resolved=env->CallObjectMethod(classLoader,resource,name);
            auto external=env->GetMethodID(urlClass,"toExternalForm","()Ljava/lang/String;");
            auto text=resolved?static_cast<jstring>(env->CallObjectMethod(resolved,external)):nullptr;
            const auto location=GameRuntime::String(env,text);
            if(env->ExceptionCheck()||location.find("!/compat/26.3/")==std::string::npos){
                if(env->ExceptionCheck())env->ExceptionClear();
                std::printf("[Bridge] Missing 26.3 overlay. Replace the bridge JAR and restart Minecraft.\n");return false;
            }
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

        const auto method=[&](const char* name,const char* signature,bool required){
            auto id=env->GetStaticMethodID(g_bridge_class,name,signature);
            if(env->ExceptionCheck()){
                if(required)ReportException(env,name);
                else env->ExceptionClear();
            }
            if(!id)std::printf("[Bridge] %s method unavailable: %s %s\n",required?"Required":"Optional",name,signature);
            return id;
        };
        // A missing optional method must never leave a pending exception that
        // invalidates subsequent, required lookups.
        g_scan_chunk_method=method("scanChunkForShipwreckCandidates","(II)Ljava/lang/String;",true);
        g_scan_confirmed_method=method("scanChunkForConfirmedShipwrecks","(II)Ljava/lang/String;",true);
        g_init_cracking_method=method("initCracking","()V",true);
        g_tick_cracking_method=method("tickCracking","()V",true);
        g_get_status_method=method("getCrackingStatus","()Ljava/lang/String;",true);
        if(!g_scan_chunk_method||!g_scan_confirmed_method||!g_init_cracking_method||!g_tick_cracking_method||!g_get_status_method){
            std::printf("[Bridge] Initialization failed; retry in 10 seconds.\n");
            return false;
        }
        g_scan_treasure_method=method("scanChunkForBuriedTreasure","(II)Ljava/lang/String;",false);
        g_reset_cracking_method=method("resetCracking","()V",false);
        g_set_hashed_seed_method=method("setHashedSeed","(J)V",false);
        g_finalize_method=method("finalizeWorldSeedCandidates","()V",false);
        g_scan_player_chunk_method=method("scanPlayerChunkForShipwreckCandidates","()Ljava/lang/String;",false);
        g_configure_scan_method=method("configureScan","(I)V",true);
        g_verified_seeds_method=method("getVerifiedSeedsForWorld","()[J",false);
        if(!g_configure_scan_method)return false;
        g_ready=true;
        g_retry_after={};

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
        env->CallStaticVoidMethod(g_bridge_class,g_configure_scan_method,jint(std::clamp(SeedCracker_Radius.load(),1,16)));
        if(failed())return false;
        if (!task_class) {
            jclass local = LoadExtensionClass(env,"phantomui.seedcracker.NativeClientTask");
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
        g_ready=false;
        request_method=nullptr;
        g_configure_scan_method=nullptr;
        g_scan_chunk_method=g_scan_player_chunk_method=g_scan_confirmed_method=g_scan_treasure_method=nullptr;
        g_init_cracking_method=g_reset_cracking_method=g_tick_cracking_method=g_get_status_method=nullptr;
        g_set_hashed_seed_method=g_finalize_method=nullptr;
        g_verified_seeds_method=nullptr;
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

    std::vector<long long> GetVerifiedSeeds(JNIEnv* env) {
        std::vector<long long> seeds;
        if (!env || !g_bridge_class || !g_verified_seeds_method)
            return seeds;
        auto array = static_cast<jlongArray>(env->CallStaticObjectMethod(g_bridge_class, g_verified_seeds_method));
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            return seeds;
        }
        if (!array)
            return seeds;
        const jsize count = env->GetArrayLength(array);
        if (count > 0 && count < 4096) {
            seeds.resize(static_cast<size_t>(count));
            env->GetLongArrayRegion(array, 0, count, reinterpret_cast<jlong*>(seeds.data()));
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                seeds.clear();
            }
        }
        env->DeleteLocalRef(array);
        return seeds;
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
