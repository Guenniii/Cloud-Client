#pragma once
#include <string>

namespace Menu::Login {
    // Per-frame view of the login state for alternative login screens (Orbit). Pointers = state the view may change.
    struct View {
        char* username; size_t username_size;
        char* password; size_t password_size;
        bool* show_password;
        bool loginInProgress;
        bool login_success_pending;
        bool success_exit;
        float success_exit_t;
        float* intro_t;
        bool* intro_done;
        const std::string* responseText;
        const std::string* license_days;
        const std::string* license_expiry;
        const std::string* last_logged_user;
        float login_alpha;
        float success_exit_duration; // SUCCESS_EXIT_DURATION, for the progress cells
        float success_timer;         // login_success_timer (0..success_hold while the success message is held)
        float success_hold;          // LOGIN_SUCCESS_HOLD
    };

    // Maps a raw server/status string to the user-facing text (unknown strings pass through).
    std::string StatusText(const std::string& raw);

    bool IsLoggedIn( );
    const std::string& UserName( );
    const std::string& LicenseDays( );
    const std::string& LicenseExpiry( );
    bool HasBeta( ); // from the auth response; false for servers that don't send the flag
    void Render(bool menuEnabled, void (*drawParticles)( ));
} // namespace Menu::Login
