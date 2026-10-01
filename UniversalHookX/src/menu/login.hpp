#pragma once
#include <string>

namespace Menu::Login {
    bool IsLoggedIn( );
    const std::string& UserName( );
    const std::string& LicenseDays( );
    const std::string& LicenseExpiry( );
    void Render(bool menuEnabled, void (*drawParticles)( ));
} // namespace Menu::Login
