#include "AuthClient.h"
#include "HWID.h"
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <iostream>
#include <sstream>


bool LoginRequest(const std::string& user, const std::string& passHash)
{
    HINTERNET hSession = WinHttpOpen(L"LoginClient",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        L"127.0.0.1",
        8080, 0);

    if (!hConnect) return false;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        L"/login",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);

    std::string body = "user=" + user + "&hash=" + passHash + "&hwid=" + GetHWID();;

    WinHttpSendRequest(
        hRequest,
        L"Content-Type: application/x-www-form-urlencoded\r\n",
        -1,
        (LPVOID)body.c_str(),
        body.size(),
        body.size(),
        0);

    WinHttpReceiveResponse(hRequest, nullptr);

    std::string response;
    DWORD bytesRead = 0;
    char buffer[128];

    do
    {
        WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead);
        if (bytesRead > 0)
            response.append(buffer, bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Debug-Hilfe:
    // OutputDebugStringA(response.c_str());

    return response.find("OK") != std::string::npos;
}

std::string LoginRequestWithResponse(const std::string& user, const std::string& passHash, const std::string& hwid)
{
    std::string response;
    // WinHttp Setup (bleibt gleich wie in deinem Code)
    // Achte darauf, dass der Port hier auf 8083 steht, passend zum Node-Server oben!
    HINTERNET hSession = WinHttpOpen(L"LoginClient", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    HINTERNET hConnect = WinHttpConnect(hSession, L"auth.cloudclient.vip", 80, 0);

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/login", NULL, NULL, NULL, 0);

    std::string body = "user=" + user + "&hash=" + passHash + "&hwid=" + hwid;

    WinHttpSendRequest(hRequest, L"Content-Type: application/x-www-form-urlencoded\r\n", -1, (LPVOID)body.c_str(), body.size(), body.size(), 0);
    WinHttpReceiveResponse(hRequest, nullptr);

    char buffer[128];
    DWORD bytesRead = 0;
    do {
        if (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
            response.append(buffer, bytesRead);
    } while (bytesRead > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Entferne Leerzeichen/Newlines
    response.erase(response.find_last_not_of(" \n\r\t") + 1);

    return response;
}

// Neue Funktion: Parst den Server-Response
// Format: "OK|daysLeft|expiryDate" oder "FAIL", "NO_LICENSE", etc.
LicenseInfo ParseLoginResponse(const std::string& response)
{
    LicenseInfo info = { false, 0, "", "" };

    if (response.empty())
    {
        info.errorMessage = "Empty response";
        return info;
    }

    // Suche nach dem Pipe-Trennzeichen
    size_t pos1 = response.find('|');

    // Wenn kein Pipe gefunden, ist es eine einfache Fehlermeldung
    if (pos1 == std::string::npos)
    {
        if (response == "OK")
        {
            info.success = true;
            info.daysLeft = -1; // Standard: unbegrenzt
            info.expiryDate = "PERMANENT";
        }
        else
        {
            info.success = false;
            info.errorMessage = response;
        }
        return info;
    }

    // Parse den Status (vor dem ersten |)
    std::string status = response.substr(0, pos1);

    if (status != "OK")
    {
        info.success = false;
        info.errorMessage = status;
        return info;
    }

    // Finde das zweite Pipe
    size_t pos2 = response.find('|', pos1 + 1);

    if (pos2 == std::string::npos)
    {
        // Nur ein Pipe - nur daysLeft
        std::string daysStr = response.substr(pos1 + 1);
        info.daysLeft = std::stoi(daysStr);
        info.expiryDate = "PERMANENT";
    }
    else
    {
        // Zwei Pipes - daysLeft und expiryDate
        std::string daysStr = response.substr(pos1 + 1, pos2 - pos1 - 1);
        std::string expiryStr = response.substr(pos2 + 1);

        info.daysLeft = std::stoi(daysStr);
        info.expiryDate = expiryStr;
    }

    info.success = true;
    return info;
}
