#pragma once
#include <atomic>


inline std::atomic<bool> AutoArmor_Enabled=false, AutoArmor_KeepElytra=true;
inline std::atomic<int> AutoArmor_Delay=150;
inline std::atomic<bool> Refill_Enabled=false;
inline std::atomic<int> Refill_Delay=200, Refill_Threshold=16;
inline std::atomic<bool> HitEffect_Enabled=false;
inline std::atomic<bool> HitEffect_ConfirmedOnly=false;
inline std::atomic<int> HitEffect_Count=16, HitEffect_Lifetime=650;
inline std::atomic<int> HitEffect_Size=150, HitEffect_Spread=100, HitEffect_Glow=100;
inline std::atomic<bool> PredictDoubleHand_Enabled=false;
inline std::atomic<bool> PredictDoubleHand_Melee=true, PredictDoubleHand_Crystal=true, PredictDoubleHand_Anchor=true;
inline std::atomic<bool> PredictDoubleHand_Return=true;
inline std::atomic<int> PredictDoubleHand_Radius=3, PredictDoubleHand_Margin=0;
inline std::atomic<bool> ShieldBreaker_Enabled=false, ShieldBreaker_Automatic=true;
inline std::atomic<int> ShieldBreaker_Delay=600;
inline std::atomic<bool> PlayerESP_Glow=true;
inline std::atomic<bool> AutoArmor_Logs=false, PredictDoubleHand_Logs=false, HitEffect_Logs=false;

// Shared module settings. Input conversion belongs to input/key_mapping.hpp.
inline std::atomic<bool> FakeLag_Enabled = false;
inline std::atomic<int> FakeLag_Delay = 100;
inline std::atomic<bool> FakeLag_Burst = true;
inline std::atomic<bool> FakeLag_Debug = false;

inline std::atomic<bool> AutoTool_Enabled = false;
inline std::atomic<bool> AutoTool_ReturnSlot = true;
inline std::atomic<bool> AutoTool_ProtectTools = true;

inline std::atomic<bool> FastPlace_Enabled = false;

inline std::atomic<bool> CW_Enabled = false;
inline std::atomic<int> CW_Delay = 0;

inline std::atomic<bool> AutoTotem_Enabled = false;

inline std::atomic<bool> Menu_Enabled = false;

inline std::atomic<int> Reach_range = 6;
inline std::atomic<bool> Reach_Enabled = false;

inline std::atomic<bool> NoJumpDelay_Enabled = false;

inline std::atomic<bool> AutoSprint_Enabled = false;
inline std::atomic<bool> AutoPearlCatch_Enabled = false;
inline std::atomic<bool> AutoPearlCatch_SpoofHotbar = false;
inline std::atomic<bool> AutoPearlCatch_Request = false;
inline std::atomic<int> AutoPearlCatch_Delay = 100;

inline std::atomic<bool> ShortWindCharge_Enabled = false;
inline std::atomic<bool> ShortWindCharge_SpoofHotbar = false;
inline std::atomic<bool> ShortWindCharge_Jump = false;
inline std::atomic<int> ShortWindCharge_ThrowDelay = 0;
inline std::atomic<bool> ShortWindCharge_Request = false;

inline std::atomic<bool> SafeWalk_Enabled = false;
inline std::atomic<bool> SafeWalk_OnlyBlocks = false;

inline std::atomic<bool> HitCrystal_Enabled = false;

inline std::atomic<bool> AntiBot_Enabled = false;
inline std::atomic<bool> AntiBot_TabList = true;
inline std::atomic<bool> AntiBot_Profile = true;
inline std::atomic<bool> AntiBot_InvalidData = true;
inline std::atomic<int> AntiBot_SpawnGrace = 40;

inline std::atomic<bool> SafeAnchor_Enabled = false;
inline std::atomic<bool> SafeAnchor_Automatic = false;
inline std::atomic<bool> SafeAnchor_SpoofHotbar = false;
inline std::atomic<bool> SafeAnchor_AutoCharge = true;
inline std::atomic<bool> SafeAnchor_Shield = true;
inline std::atomic<bool> SafeAnchor_Detonate = true;
inline std::atomic<bool> SafeAnchor_Logs = true;
inline std::atomic<int> SafeAnchor_PlaceDelay = 0;
inline std::atomic<int> SafeAnchor_ChargeDelay = 75;
inline std::atomic<int> SafeAnchor_ShieldDelay = 75;
inline std::atomic<int> SafeAnchor_DetonateDelay = 75;

inline std::atomic<bool> BreachSwap_Enabled = false;
inline std::atomic<bool> BreachSwap_SpoofHotbar = false;

inline std::atomic<bool> AutoMace_Enabled = false;
inline std::atomic<bool> AutoMace_SpoofHotbar = false;
inline std::atomic<int> AutoMace_Range = 3;
inline std::atomic<int> AutoMace_Fov = 60;

inline std::atomic<bool> AimAssist_Enabled = false;
inline std::atomic<int> AimAssist_Range = 4;
inline std::atomic<int> AimAssist_Fov = 60;
inline std::atomic<int> AimAssist_Speed = 90;
inline std::atomic<int> AimAssist_Smoothness = 50;
inline std::atomic<int> AimAssist_Variation = 15;
inline std::atomic<bool> AimAssist_OnlyAttack = true;
inline std::atomic<bool> AimAssist_OnlyWeapon = true;
inline std::atomic<bool> AimAssist_Horizontal = false;

inline std::atomic<bool> Silent_Aim_Enabled = false;
inline std::atomic<int> SilentAim_Range = 4;
inline std::atomic<int> SilentAim_Fov = 60;

inline std::atomic<bool> SeedCracker_Enabled = false;

inline std::atomic<bool> OreSim_Enabled = false;
inline std::atomic<int> OreSim_Radius = 3;
inline std::atomic<bool> OreSim_AirCheck = true;
inline std::atomic<bool> OreSim_Ores[10] = {false,false,false,false,true,false,false,false,false,true};

inline std::atomic<bool> PlayerESP_Enabled = false;
inline std::atomic<int> PlayerESP_Range = 128;
inline std::atomic<int> PlayerESP_LineWidth = 2;

inline std::atomic<bool> PlayerESP_Skin = false;
