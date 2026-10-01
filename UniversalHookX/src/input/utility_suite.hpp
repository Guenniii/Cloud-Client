#pragma once
#include "../modules/settings.hpp"
#include "assist_options.hpp"
#include "../utils/sdk/java.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include "../utils/oresim/oresim.hpp"
#include "../menu/widgets.hpp"
#include "../utils/theme/theme.hpp"
#include "rotation/antibot.hpp"
#include <mutex>
#include <chrono>

namespace UtilitySuite {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID publish=nullptr,clear=nullptr,shield=nullptr,frame=nullptr,drain=nullptr;
inline auto drainAt=std::chrono::steady_clock::time_point{};
inline auto retry=std::chrono::steady_clock::time_point{};
inline std::vector<double> frameValues;
inline auto renderLogAt=std::chrono::steady_clock::time_point{};
inline int predictState=-1;
inline bool Check(JNIEnv* env) {
    if(!env->ExceptionCheck()) return true;
    env->ExceptionDescribe();env->ExceptionClear();return false;
}
inline bool Load(JNIEnv* env) {
    if(!env || env->ExceptionCheck()) return false;
    if(bridge) return true;
    auto now=std::chrono::steady_clock::now();if(now<retry) return false;
    retry=now+std::chrono::seconds(5);
    if(!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
    auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.assist.UtilitySuiteBridge");
    if(!Check(env) || !cls) return false;
    JniSafety::Lookup api(env);
    publish=api.GetStaticMethodID(cls,"publish","([I)V");
    clear=api.GetStaticMethodID(cls,"clear","()V");
    shield=api.GetStaticMethodID(cls,"shieldClick","()Z");
    frame=api.GetStaticMethodID(cls,"hitFrame","()[D");
    drain=api.GetStaticMethodID(cls,"drainLogs","()Ljava/lang/String;");
    if(!publish || !clear || !shield || !frame || !drain) return false;
    bridge=static_cast<jclass>(env->NewGlobalRef(cls));
    if(Check(env) && bridge) { std::printf("[Assist] Bridge ready.\n");return true; }
    return false;
}
inline void Clear(JNIEnv* env,bool shutdown=false) {
    std::lock_guard lock(mutex);
    if(env && bridge) {
        if(!env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,clear);Check(env); }
        if(shutdown) {env->DeleteGlobalRef(bridge);bridge=nullptr;}
    }
}
inline void Publish(JNIEnv* env) {
    std::lock_guard lock(mutex);
    if(!env || env->ExceptionCheck()) return;
    JniSafety::LocalFrame local(env,16);if(!local || !Load(env)) return;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    bool active=!Menu_Enabled && process==GetCurrentProcessId();
    int state=(PredictDoubleHand_Enabled?1:0)|(PredictDoubleHand_Logs?2:0)|(active?4:0);
    if(state!=predictState) {
        predictState=state;
        if(PredictDoubleHand_Logs) std::printf("[PredictDoubleHand] Native enabled=%d diagnostics=%d running=%d (menu=%d foreground=%d)\n",
            int(PredictDoubleHand_Enabled.load()),int(PredictDoubleHand_Logs.load()),int(active),int(Menu_Enabled.load()),int(process==GetCurrentProcessId()));
    }
    if(!active) {env->CallStaticVoidMethod(bridge,clear);Check(env);return;}
    { PlayerFilter::AntiBot filter(env); }
    Protocol::Payload<jint> values{};
    values[Protocol::Index(Protocol::Option::AUTO_ARMOR)] = AutoArmor_Enabled.load();
    values[Protocol::Index(Protocol::Option::REFILL)] = Refill_Enabled.load();
    values[Protocol::Index(Protocol::Option::HIT_EFFECT)] = HitEffect_Enabled.load();
    values[Protocol::Index(Protocol::Option::PREDICT_DOUBLE_HAND)] = PredictDoubleHand_Enabled.load();
    values[Protocol::Index(Protocol::Option::SHIELD_BREAKER)] = ShieldBreaker_Enabled.load();
    values[Protocol::Index(Protocol::Option::ANTI_BOT)] = AntiBot_Enabled.load();
    values[Protocol::Index(Protocol::Option::ARMOR_DELAY)] = AutoArmor_Delay.load();
    values[Protocol::Index(Protocol::Option::KEEP_ELYTRA)] = AutoArmor_KeepElytra.load();
    values[Protocol::Index(Protocol::Option::REFILL_DELAY)] = Refill_Delay.load();
    values[Protocol::Index(Protocol::Option::REFILL_THRESHOLD)] = Refill_Threshold.load();
    values[Protocol::Index(Protocol::Option::PARTICLE_COUNT)] = HitEffect_Count.load();
    values[Protocol::Index(Protocol::Option::PARTICLE_LIFETIME)] = HitEffect_Lifetime.load();
    values[Protocol::Index(Protocol::Option::PREDICT_MELEE)] = PredictDoubleHand_Melee.load();
    values[Protocol::Index(Protocol::Option::PREDICT_CRYSTAL)] = PredictDoubleHand_Crystal.load();
    values[Protocol::Index(Protocol::Option::PREDICT_ANCHOR)] = PredictDoubleHand_Anchor.load();
    values[Protocol::Index(Protocol::Option::DANGER_RADIUS)] = PredictDoubleHand_Radius.load();
    values[Protocol::Index(Protocol::Option::DAMAGE_MARGIN)] = PredictDoubleHand_Margin.load();
    values[Protocol::Index(Protocol::Option::RETURN_SLOT)] = PredictDoubleHand_Return.load();
    values[Protocol::Index(Protocol::Option::SHIELD_AUTOMATIC)] = ShieldBreaker_Automatic.load();
    values[Protocol::Index(Protocol::Option::SHIELD_DELAY)] = ShieldBreaker_Delay.load();
    values[Protocol::Index(Protocol::Option::AUTO_TOTEM)] = AutoTotem_Enabled.load();
    values[Protocol::Index(Protocol::Option::ARMOR_LOGS)] = AutoArmor_Logs.load();
    values[Protocol::Index(Protocol::Option::PREDICT_LOGS)] = PredictDoubleHand_Logs.load();
    values[Protocol::Index(Protocol::Option::HIT_LOGS)] = HitEffect_Logs.load();
    values[Protocol::Index(Protocol::Option::CONFIRMED_ONLY)] = HitEffect_ConfirmedOnly.load();
    values[Protocol::Index(Protocol::Option::PARTICLE_SPREAD)] = HitEffect_Spread.load();
    const auto count=static_cast<jsize>(values.size());
    auto array=env->NewIntArray(count);if(!Check(env) || !array) return;
    env->SetIntArrayRegion(array,0,count,values.data());if(!Check(env)) return;
    env->CallStaticVoidMethod(bridge,publish,array);if(!Check(env)) return;
    auto now=std::chrono::steady_clock::now();
    if(now>=drainAt) {
        drainAt=now+std::chrono::milliseconds(500);
        auto logs=static_cast<jstring>(env->CallStaticObjectMethod(bridge,drain));
        if(Check(env) && logs) {
            const char* chars=env->GetStringUTFChars(logs,nullptr);
            if(chars) {std::printf("%s",chars);env->ReleaseStringUTFChars(logs,chars);}
            Check(env);
        }
    }
}
inline bool TryShieldClick() {
    if(!ShieldBreaker_Enabled || Menu_Enabled) return false;
    auto env=SeedCracker::GetMinecraftJNIEnv();std::lock_guard lock(mutex);
    if(!env || !bridge || env->ExceptionCheck()) return false;
    bool result=env->CallStaticBooleanMethod(bridge,shield)==JNI_TRUE;
    return Check(env) && result;
}
inline void Render() {
    if(!HitEffect_Enabled || Menu_Enabled) return;
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env) return;
    std::lock_guard lock(mutex);if(!bridge || env->ExceptionCheck()) return;
    JniSafety::LocalFrame local(env,8);if(!local) {Check(env);return;}
    auto array=static_cast<jdoubleArray>(env->CallStaticObjectMethod(bridge,frame));
    if(!Check(env) || !array) return;
    int n=env->GetArrayLength(array);
    auto now=std::chrono::steady_clock::now();bool report=HitEffect_Logs && now>=renderLogAt;
    if(report) renderLogAt=now+std::chrono::seconds(2);
    if(n<19 || n>19+256*5 || (n-19)%5) {
        if(report) std::printf("[HitEffect] Render frame: %d values (no active particles / invalid frame).\n",n);
        return;
    }
    frameValues.resize(n);env->GetDoubleArrayRegion(array,0,n,frameValues.data());if(!Check(env)) return;
    auto m=frameValues.data();for(int i=0;i<n;++i) if(!std::isfinite(m[i])) return;
    auto vp=ImGui::GetMainViewport();auto draw=ImGui::GetBackgroundDrawList();
    draw->PushClipRect(vp->Pos,ImVec2(vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y),true);
    int projected=0;
    const float size=std::clamp(HitEffect_Size.load(),50,300)/150.0f;
    const float glow=std::clamp(HitEffect_Glow.load(),0,100)/100.0f;
    for(int i=19;i<n;i+=5) {
        auto p=OreSim::Project(m,m[i]-m[16],m[i+1]-m[17],m[i+2]-m[18]);
        if(p.w<=.001 || std::abs(p.x)>p.w || std::abs(p.y)>p.w) continue;
        ImVec2 center(float(vp->Pos.x+(p.x/p.w+1)*vp->Size.x*.5),float(vp->Pos.y+(1-p.y/p.w)*vp->Size.y*.5));
        float radius=std::clamp(float(m[i+4]*vp->Size.y/p.w),2.25f,7.5f)*size,alpha=std::clamp(float(m[i+3]),0.0f,1.0f);
        if(glow>0) Menu::Widgets::DrawGlowCircle(draw,center,radius,Theme::Current().accent,alpha*glow,6);
        draw->AddCircleFilled(center,radius,Menu::Widgets::ColA(Theme::Current().accent,alpha),12);
        ++projected;
    }
    draw->PopClipRect();
    if(report) std::printf("[HitEffect] Render particles=%d visible=%d\n",(n-19)/5,projected);
}
}
