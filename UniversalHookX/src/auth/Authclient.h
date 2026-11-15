#pragma once
#include <string>
#include <vector>

struct LicenseInfo
{
    bool success;
    int daysLeft;  // -1 = permanent/unbegrenzt
    std::string expiryDate;
    std::string errorMessage;
    std::string username; // Exact account spelling returned by the authenticated server.

    const std::string& DisplayName(const std::string& enteredName) const {
        return success && !username.empty() ? username : enteredName;
    }
};

bool LoginRequest(const std::string& user, const std::string& passHash);
std::string LoginRequestWithResponse(const std::string& user, const std::string& passHash, const std::string& hwid);

// Neue Funktion: Parst die Response und gibt LicenseInfo zurück
LicenseInfo ParseLoginResponse(const std::string& response);
std::vector<unsigned char> DownloadUserAvatar(const std::string& username);
