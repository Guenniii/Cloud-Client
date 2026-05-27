#include "../../utils/sdk/CMinecraft.h"
#include "../../utils/Silentaim.hpp"
#include "cw.hpp"
#include "CwCrystal.hpp"
#include "Reach.hpp"
#include "AutoTotem.hpp"
#include "HitCrystal.hpp"
#include "../movement/NoJumpDelay.hpp"
#include "../movement/Sprint.hpp"
#include "../../base.hpp"
#include "../../utils/sdk/java.hpp"
#include "../settings.hpp"
#include <Windows.h>


static HitCrystal* g_sword_obsidian = nullptr;
static CwCrystal* g_crystal = nullptr;
static Reach* g_reach = nullptr;
static AutoTotem* g_auto_totem = nullptr;
static ULONGLONG g_last_reach_write = 0;

void CW::Update( ) {
    if (!p_jni || !p_jni->p_cminecraft) {
        return;
    }


    if (FastPlace_Enabled)
    {
        p_jni->p_cminecraft->SetRightClickDelay(0);
    }

    if (CW_Enabled)
    {
        
        if (!g_crystal) {
            g_crystal = new CwCrystal(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
            g_crystal->Start( );
            p_jni->p_cminecraft->SetRightClickDelay(0);
        }
    }

    if (Reach_Enabled)
    {
        if (!g_reach) {
            g_reach = new Reach(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }
        const ULONGLONG now = GetTickCount64();
        if (now - g_last_reach_write >= 150) {
            g_reach->SetReach(Reach_range);
            g_last_reach_write = now;
        }
    } 

    if (AutoTotem_Enabled) {
        if (!g_auto_totem) {
            g_auto_totem = new AutoTotem(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }
        g_auto_totem->Tick( );
    }

    if (NoJumpDelay_Enabled) { // Angenommen, du hast eine Checkbox/Variable dafür
        // Wir nutzen die statische Methode aus der .hpp
        // Da du p_cminecraft.get() nutzt, um an die Instanz zu kommen:
        NoJumpDelay::Run(p_jni->GetEnv( ), p_jni->p_cminecraft.get( ));
    }

    if (AutoSprint_Enabled) {
        AutoSprint::Run(p_jni->GetEnv( ), p_jni->p_cminecraft.get( ));
    }

    if (HitCrystal_Enabled) {
        if (!g_sword_obsidian) {
            g_sword_obsidian = new HitCrystal(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }

        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
            g_sword_obsidian->Run( );
        }
    }

    if (Silent_Aim_Enabled) {
        if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) {

            // 1. Erstelle das Request-Objekt (wie im Java-Beispiel)
            SilentAimRequest req;
            req.yaw = 180.0f; // Dein mathematisch errechnetes Ziel (z.B. vom Crystal)
            req.pitch = 0.0f;
            req.profile = Profile::COMBAT; // COMBAT, PLACE oder PRECISE
            req.syncVisualHead = true;     // Zeigt Kopfdrehung für andere Spieler

            // 2. Berechne den sanften Übergang (Spring, Tremor, GCD-Snap)
            SilentAim::aim(p_jni->GetEnv( ), p_jni->p_cminecraft->GetInstance( ), req);

            // 3. Schicke das offizielle manipulierte Paket ab
            SilentAim::doSilentRotationJNI(p_jni->GetEnv( ), p_jni->p_cminecraft->GetInstance( ));
        } else {
            SilentAim::reset( ); // Resettet die Dämpfung wenn man loslässt
        }
    }

    

}

void CW::RenderOverlay( ) {
}

void CW::RenderHud( ) {
}

void CW::RenderMenu( ) {
}

std::string CW::GetName( ) {
    return "CW";
}

std::string CW::GetCategory( ) {
    return "Combat";
}

int CW::GetKey( ) {
    return 0;
}

bool CW::IsEnabled( ) {
    return true;
}

void CW::SetEnabled(bool v) {
}

void CW::Toggle( ) {
}

void AutoCW() {

    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0)
    {
        return;
    }

    


}
