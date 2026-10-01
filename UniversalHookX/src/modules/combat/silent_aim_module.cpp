#include "../../input/game_screen.hpp"
#include "../../input/rotation/player_targeting.hpp"
#include "../../utils/SilentAim.hpp"
#include "../../utils/sdk/CMinecraft.h"
#include "../../utils/sdk/java.hpp"
#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
    class SilentAimModule final : public RuntimeModule {
    public:
        SilentAimModule( ) : RuntimeModule("SilentAim", "Combat", Silent_Aim_Enabled) { }
    private:
        std::optional<int> targetId;
        ULONGLONG lastSoftLog = 0;
        ULONGLONG lastAttackMs = 0;
        int failStreak = 0;

        float lockYaw = 0.f;
        float lockPitch = 0.f;
        bool lockActive = false;

        static constexpr ULONGLONG kAttackCooldownMs = 520;
        static constexpr float kLockLerp = 0.45f;

        void SoftLog(const char* msg) {
            const ULONGLONG now = GetTickCount64( );
            if (now - lastSoftLog < 500)
                return;
            lastSoftLog = now;
            std::printf("[SilentAim] %s\n", msg);
        }

        void ClearState(const char* reason = nullptr) {
            if (reason)
                SoftLog(reason);
            targetId.reset( );
            failStreak = 0;
            lockActive = false;
            SilentAim::reset(SilentAim::Owner::Standalone);
            SilentAim::reset(SilentAim::Owner::HitCrystal);
        }

        static float NormalizeYaw(float yaw) {
            while (yaw > 180.f)
                yaw -= 360.f;
            while (yaw < -180.f)
                yaw += 360.f;
            return yaw;
        }

        static float LerpAngle(float from, float to, float t) {
            return from + NormalizeYaw(to - from) * t;
        }

        /*
         * Nur den KOPF drehen (yHeadRot) – Kamera (yRot/xRot) bleibt beim Spieler.
         *
         * Wichtig gegen Flicker:
         * Vanilla setzt yHeadRot jeden Tick Richtung yRot zurueck.
         * Deshalb hart ueberschreiben und yHeadRotO = yHeadRot (kein Render-Lerp-Kampf).
         */
        static void AimHeadOnly(JNIEnv* env, jobject player, float targetYaw) {
            if (AutoMace_Enabled || !env || !player)
                return;
            jclass cls = env->GetObjectClass(player);
            jfieldID fHead = env->GetFieldID(cls, "yHeadRot", "F");
            jfieldID fHeadO = env->GetFieldID(cls, "yHeadRotO", "F");
            env->DeleteLocalRef(cls);
            if (env->ExceptionCheck( ))
                env->ExceptionClear( );
            if (!fHead)
                return;

            const float yaw = NormalizeYaw(targetYaw);
            env->SetFloatField(player, fHead, yaw);
            if (env->ExceptionCheck( ))
                env->ExceptionClear( );
            // Gleiches Prev-Value → Renderer interpoliert nicht hin und her
            if (fHeadO) {
                env->SetFloatField(player, fHeadO, yaw);
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
            }
        }

        static jobject FindEntityById(JNIEnv* env, jobject mc, int entityId) {
            if (!env || !mc)
                return nullptr;

            jclass mcClass = env->GetObjectClass(mc);
            if (!mcClass)
                return nullptr;
            jfieldID f_level = env->GetFieldID(mcClass, "level", "Lnet/minecraft/client/multiplayer/ClientLevel;");
            env->DeleteLocalRef(mcClass);
            if (!f_level || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                return nullptr;
            }

            jobject level = env->GetObjectField(mc, f_level);
            if (!level)
                return nullptr;

            jclass levelClass = env->GetObjectClass(level);
            jmethodID m_entities = env->GetMethodID(levelClass, "entitiesForRendering", "()Ljava/lang/Iterable;");
            env->DeleteLocalRef(levelClass);
            if (!m_entities || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                env->DeleteLocalRef(level);
                return nullptr;
            }

            jobject iterable = env->CallObjectMethod(level, m_entities);
            env->DeleteLocalRef(level);
            if (!iterable || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                return nullptr;
            }

            jclass iterableClass = env->FindClass("java/lang/Iterable");
            jmethodID m_iterator = env->GetMethodID(iterableClass, "iterator", "()Ljava/util/Iterator;");
            env->DeleteLocalRef(iterableClass);
            jobject iterator = env->CallObjectMethod(iterable, m_iterator);
            env->DeleteLocalRef(iterable);
            if (!iterator)
                return nullptr;

            jclass iteratorClass = env->FindClass("java/util/Iterator");
            jmethodID m_hasNext = env->GetMethodID(iteratorClass, "hasNext", "()Z");
            jmethodID m_next = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
            env->DeleteLocalRef(iteratorClass);

            jobject found = nullptr;
            while (env->CallBooleanMethod(iterator, m_hasNext) == JNI_TRUE) {
                if (env->ExceptionCheck( )) {
                    env->ExceptionClear( );
                    break;
                }
                jobject entity = env->CallObjectMethod(iterator, m_next);
                if (!entity || env->ExceptionCheck( )) {
                    if (env->ExceptionCheck( ))
                        env->ExceptionClear( );
                    continue;
                }
                jclass entClass = env->GetObjectClass(entity);
                jmethodID m_getId = env->GetMethodID(entClass, "getId", "()I");
                env->DeleteLocalRef(entClass);
                if (!m_getId || env->ExceptionCheck( )) {
                    if (env->ExceptionCheck( ))
                        env->ExceptionClear( );
                    env->DeleteLocalRef(entity);
                    continue;
                }
                const int id = env->CallIntMethod(entity, m_getId);
                if (env->ExceptionCheck( )) {
                    env->ExceptionClear( );
                    env->DeleteLocalRef(entity);
                    continue;
                }
                if (id == entityId) {
                    found = entity;
                    break;
                }
                env->DeleteLocalRef(entity);
            }
            env->DeleteLocalRef(iterator);
            return found;
        }

        static float GetAttackStrength(JNIEnv* env, jobject player) {
            if (!env || !player)
                return 1.0f;
            jclass cls = env->GetObjectClass(player);
            jmethodID m = env->GetMethodID(cls, "getAttackStrengthScale", "(F)F");
            env->DeleteLocalRef(cls);
            if (!m || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                return 1.0f;
            }
            const float scale = env->CallFloatMethod(player, m, 0.5f);
            if (env->ExceptionCheck( )) {
                env->ExceptionClear( );
                return 1.0f;
            }
            return scale;
        }

        bool AttackEntity(JNIEnv* env, CMinecraft* cmc, jobject mc, jobject player, jobject entity) {
            if (!env || !cmc || !mc || !player || !entity)
                return false;
            if (!cmc->f_game_mode || !cmc->m_attack)
                return false;

            jobject gameMode = env->GetObjectField(mc, cmc->f_game_mode);
            if (!gameMode || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                return false;
            }

            env->CallVoidMethod(gameMode, cmc->m_attack, player, entity);
            if (env->ExceptionCheck( )) {
                env->ExceptionClear( );
                env->DeleteLocalRef(gameMode);
                return false;
            }
            env->DeleteLocalRef(gameMode);

            if (cmc->m_swing_arm && cmc->o_main_hand) {
                env->CallVoidMethod(player, cmc->m_swing_arm, cmc->o_main_hand);
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
            }
            return true;
        }

        // Wie HitCrystal: nur Schwerter (_sword in descriptionId)
        static bool IsHoldingSword(JNIEnv* env, CMinecraft* cmc, jobject player) {
            if (!env || !cmc || !player || !cmc->m_get_main_hand_item)
                return false;
            jobject stack = env->CallObjectMethod(player, cmc->m_get_main_hand_item);
            if (!stack || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                return false;
            }
            if (cmc->m_stack_is_empty) {
                jboolean empty = env->CallBooleanMethod(stack, cmc->m_stack_is_empty);
                if (env->ExceptionCheck( )) {
                    env->ExceptionClear( );
                    env->DeleteLocalRef(stack);
                    return false;
                }
                if (empty) {
                    env->DeleteLocalRef(stack);
                    return false;
                }
            }
            if (!cmc->m_stack_get_item) {
                env->DeleteLocalRef(stack);
                return false;
            }
            jobject item = env->CallObjectMethod(stack, cmc->m_stack_get_item);
            if (!item || env->ExceptionCheck( )) {
                if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
                env->DeleteLocalRef(stack);
                return false;
            }
            jclass itemClass = env->GetObjectClass(item);
            jmethodID getDesc = env->GetMethodID(itemClass, "getDescriptionId", "()Ljava/lang/String;");
            env->DeleteLocalRef(itemClass);
            bool isSword = false;
            if (getDesc && !env->ExceptionCheck( )) {
                jstring desc = (jstring)env->CallObjectMethod(item, getDesc);
                if (desc && !env->ExceptionCheck( )) {
                    const char* s = env->GetStringUTFChars(desc, nullptr);
                    if (s) {
                        isSword = (strstr(s, "_sword") != nullptr);
                        env->ReleaseStringUTFChars(desc, s);
                    }
                    env->DeleteLocalRef(desc);
                } else if (env->ExceptionCheck( ))
                    env->ExceptionClear( );
            } else if (env->ExceptionCheck( ))
                env->ExceptionClear( );
            env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            return isSword;
        }

        void Tick( ) override {
            if (!Silent_Aim_Enabled) {
                if (targetId || lockActive)
                    ClearState( );
                return;
            }
            if (!p_jni || !p_jni->p_cminecraft) {
                ClearState("kein JNI / Minecraft");
                return;
            }
            if (Menu_Enabled) {
                if (targetId || lockActive)
                    ClearState("Menue offen");
                return;
            }
            DWORD process = 0;
            GetWindowThreadProcessId(GetForegroundWindow( ), &process);
            if (process != GetCurrentProcessId( )) {
                if (targetId || lockActive)
                    ClearState("Fenster nicht fokussiert");
                return;
            }

            auto* cmc = p_jni->p_cminecraft.get( );
            JNIEnv* env = p_jni->GetEnv( );
            jobject mc = cmc->GetInstance( );
            if (!env || !mc) {
                ClearState("env/mc null");
                return;
            }
            if (Input::GameScreenOpen(env, mc)) {
                if (targetId || lockActive)
                    ClearState("Screen/Inventar offen");
                return;
            }

            jobject player = env->GetObjectField(mc, cmc->f_player);
            if (!player) {
                SoftLog("player null");
                return;
            }

            // Nur mit Schwert in der Hand: Aim aktiv
            if (!IsHoldingSword(env, cmc, player)) {
                if (targetId || lockActive)
                    ClearState("kein Schwert");
                env->DeleteLocalRef(player);
                return;
            }

            const int range = std::clamp(SilentAim_Range.load( ), 1, 6);
            const int fov = std::clamp(SilentAim_Fov.load( ), 10, 180);
            const auto target = Rotation::FindPlayerTarget(env, mc, range, fov, targetId);

            if (!target) {
                char buf[96];
                snprintf(buf, sizeof(buf), "kein Target (range=%d fov=%d)", range, fov);
                if (targetId || lockActive)
                    ClearState(buf);
                else
                    SoftLog(buf);
                env->DeleteLocalRef(player);
                return;
            }

            if (!targetId || *targetId != target->id) {
                std::printf("[SilentAim] lock id=%d (Kopf aimt, Kamera free)\n", target->id);
                failStreak = 0;
                lockActive = false;
            }
            targetId = target->id;

            const float targetYaw = target->angles.yaw;
            const float targetPitch = target->angles.pitch;

            // Interner Silent-Lock nachfuehren
            if (!lockActive) {
                lockYaw = targetYaw;
                lockPitch = targetPitch;
                lockActive = true;
            } else {
                lockYaw = LerpAngle(lockYaw, targetYaw, kLockLerp);
                lockPitch = lockPitch + (targetPitch - lockPitch) * kLockLerp;
                if (lockPitch > 90.f)
                    lockPitch = 90.f;
                if (lockPitch < -90.f)
                    lockPitch = -90.f;
            }

            // 1) Controller silent (kein yRot/xRot) – Kamera bleibt
            SilentAimRequest trackReq;
            trackReq.yaw = lockYaw;
            trackReq.pitch = lockPitch;
            trackReq.syncVisualHead = false;
            float ty = lockYaw, tp = lockPitch;
            SilentAim::apply(env, mc, trackReq, SilentAim::Owner::Standalone, &ty, &tp);

            // 2) Nur Kopf visuell auf Target – First-Person-Sicht unberuehrt
            AimHeadOnly(env, player, lockYaw);

            // 3) Angriff nur bei Linksklick + Cooldown
            // AutoMace owns manual clicks while enabled; do not send a second attack.
            if (AutoMace_Enabled || BreachSwap_Enabled || !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                env->DeleteLocalRef(player);
                return;
            }
            const ULONGLONG now = GetTickCount64( );
            if (now - lastAttackMs < kAttackCooldownMs) {
                env->DeleteLocalRef(player);
                return;
            }
            if (GetAttackStrength(env, player) < 0.85f) {
                env->DeleteLocalRef(player);
                return;
            }

            // Kurz Server-Blick (set/restore in apply), Kamera danach wieder normal
            SilentAimRequest hitReq;
            hitReq.yaw = lockYaw;
            hitReq.pitch = lockPitch;
            hitReq.syncVisualHead = false; // Head bleibt ueber AimHeadOnly gesteuert

            float yaw = 0.f, pitch = 0.f;
            if (!SilentAim::apply(env, mc, hitReq, SilentAim::Owner::HitCrystal, &yaw, &pitch)) {
                yaw = lockYaw;
                pitch = lockPitch;
            }

            jobject entity = FindEntityById(env, mc, target->id);
            if (!entity) {
                failStreak++;
                SoftLog("Entity nicht in World");
                env->DeleteLocalRef(player);
                return;
            }

            if (!AttackEntity(env, cmc, mc, player, entity)) {
                failStreak++;
                std::printf("[SilentAim] ATTACK FAILED id=%d streak=%d\n", target->id, failStreak);
                env->DeleteLocalRef(entity);
                env->DeleteLocalRef(player);
                return;
            }

            // Nach Hit: Kopf wieder auf Lock (apply kann Head nicht angefasst haben)
            AimHeadOnly(env, player, lockYaw);

            lastAttackMs = now;
            failStreak = 0;
            static ULONGLONG lastOk = 0;
            if (now - lastOk > 800) {
                lastOk = now;
                std::printf("[SilentAim] OK id=%d headLock=%.1f camera=free\n", target->id, lockYaw);
            }

            env->DeleteLocalRef(entity);
            env->DeleteLocalRef(player);
        }
    };
} // namespace

std::unique_ptr<ModuleBase> RuntimeModules::CreateSilentAim( ) {
    return std::make_unique<SilentAimModule>( );
}
