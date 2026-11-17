#pragma once
#include <atomic>

inline std::atomic<bool> LootBeacon_Enabled=false,LootBeacon_Totems=true,LootBeacon_Equipment=true,LootBeacon_Other=false,LootBeacon_Glow=true,LootBeacon_Theme=true;
inline std::atomic<int> LootBeacon_Range=64,LootBeacon_Height=3,LootBeacon_Width=2,LootBeacon_Opacity=65,LootBeacon_Color=0xF0B852;
inline std::atomic<bool> EchoTrail_Enabled=false, EchoTrail_Self=false, EchoTrail_Glow=true, EchoTrail_Theme=true;
inline std::atomic<int> EchoTrail_Duration=1800, EchoTrail_Width=2, EchoTrail_Range=128, EchoTrail_Color=0x6F42FF;


inline std::atomic<bool> AutoArmor_Enabled=false, AutoArmor_KeepElytra=true;
inline std::atomic<int> AutoArmor_Delay=150;
inline std::atomic<bool> Refill_Enabled=false, Refill_Totems=false;
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
inline std::atomic<bool> TriggerBot_Enabled=false, TriggerBot_OnlyWeapon=true;
inline std::atomic<int> TriggerBot_Delay=80;
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
inline std::atomic<int> AutoTotem_PopDelay = 200;
inline std::atomic<int> AutoTotem_SwapDelay = 100;
inline std::atomic<int> AutoTotem_RetryInterval = 1000;

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

inline std::atomic<bool> KeyPearl_Enabled = false;
inline std::atomic<bool> KeyPearl_SpoofHotbar = false;
inline std::atomic<bool> KeyPearl_Request = false;

inline std::atomic<bool> TotemWarner_Enabled = false;
inline std::atomic<int> TotemWarner_Range = 64;

inline std::atomic<bool> SafeWalk_Enabled = false;
inline std::atomic<bool> SafeWalk_OnlyBlocks = false;

inline std::atomic<bool> HitCrystal_Enabled = false;
inline std::atomic<int> HitCrystal_ObsidianDelay = 100;
inline std::atomic<int> HitCrystal_CrystalDelay = 100;
inline std::atomic<int> HitCrystal_Cooldown = 200;

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

inline std::atomic<bool> ItemESP_Enabled=false,ItemESP_Totems=true,ItemESP_Equipment=true,ItemESP_Other=true,ItemESP_Labels=true,ItemESP_Glow=true;
inline std::atomic<int> ItemESP_Range=64,ItemESP_Colors[3]={0xF5B84B,0xCE8CF7,0x45D6CA};
inline std::atomic<bool> Friends_Enabled=true,Friends_MiddleClick=false;
inline std::atomic<int> Friends_Color=0x45D6CA;
inline std::atomic<bool> ExplosionPreview_Enabled=false,ExplosionPreview_Crystals=true,ExplosionPreview_Anchors=true,ExplosionPreview_Target=true,ExplosionPreview_Glow=true;
inline std::atomic<int> ExplosionPreview_Range=16;
// World-space visual modules; native drawing is shared by Vulkan and OpenGL.
inline std::atomic<bool> ProjectilePrediction_HighlightBlock=true;
inline std::atomic<bool> StorageESP_ThemeColors=false;
inline std::atomic<int> StorageESP_Colors[4]={0xF5B84B,0x45D6CA,0xCE8CF7,0x688DF5};
inline std::atomic<bool> ProjectilePrediction_Enabled=false;
inline std::atomic<bool> ProjectilePrediction_Held=true,ProjectilePrediction_Flying=true,ProjectilePrediction_Impact=true,ProjectilePrediction_Glow=true;
inline std::atomic<int> ProjectilePrediction_Range=128,ProjectilePrediction_Ticks=100,ProjectilePrediction_LineWidth=2;
inline std::atomic<bool> SoundESP_Enabled=false,SoundESP_Steps=true,SoundESP_Projectiles=true,SoundESP_Explosions=true,SoundESP_Directions=true,SoundESP_Glow=true;
inline std::atomic<int> SoundESP_Range=64,SoundESP_Lifetime=2000;
inline std::atomic<bool> StorageESP_Enabled=false,StorageESP_Chests=true,StorageESP_Barrels=true,StorageESP_Shulkers=true,StorageESP_EnderChests=true,StorageESP_Labels=true,StorageESP_Glow=true;
inline std::atomic<int> StorageESP_Range=64,StorageESP_LineWidth=2;

inline std::atomic<bool> TargetHUD_Enabled=false,TargetHUD_Mini=false,TargetHUD_Glow=true,TargetHUD_Details=true,TargetHUD_Preview=true;
inline std::atomic<int> TargetHUD_Hold=1500,TargetHUD_Scale=100,TargetHUD_X=56,TargetHUD_Y=62;

inline std::atomic<int> SeedCracker_Radius=4,SeedCracker_BoxWidth=2;
// Applies the SeedCracker result to the ore simulation automatically.
inline std::atomic<bool> SeedCracker_AutoApplySeed=false;
inline std::atomic<bool> SeedCracker_Boxes=true,SeedCracker_BoxGlow=true;
