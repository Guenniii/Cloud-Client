#include "AuthClient.h"
#include "HWID.h"
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <iostream>
#include <sstream>
#include "avatar_policy.hpp"


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
// Format: "OK|daysLeft|expiryDate|username|beta"; username and beta are optional for legacy servers.
LicenseInfo ParseLoginResponse(const std::string& response)
{
    LicenseInfo info = { false, 0, "", "", "", false };

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
        const size_t pos3 = response.find('|', pos2 + 1);
        std::string expiryStr = response.substr(pos2 + 1, pos3 == std::string::npos ? std::string::npos : pos3 - pos2 - 1);

        if (pos3 != std::string::npos) {
            // Names never contain '|', so a 4th pipe starts the beta flag ("0"/"1"); any other
            // trailing metadata is malformed and drops both optional fields.
            const size_t pos4 = response.find('|', pos3 + 1);
            const std::string betaStr = pos4 == std::string::npos ? "0" : response.substr(pos4 + 1);
            auto accountName = response.substr(pos3 + 1, pos4 == std::string::npos ? std::string::npos : pos4 - pos3 - 1);
            // Keep the server spelling intact; invalid optional metadata uses the legacy fallback.
            bool valid = !accountName.empty() && accountName.size() <= 256 && (betaStr == "0" || betaStr == "1");
            for (unsigned char ch : accountName)
                valid &= ch >= 32 && ch != 127 && ch != '|';
            info.beta = valid && betaStr == "1";
            if (valid) info.username = std::move(accountName);
        }

        info.daysLeft = std::stoi(daysStr);
        info.expiryDate = expiryStr;
    }

    info.success = true;
    return info;
}

// Image-only HTTPS request. An absent image or transport failure keeps the embedded avatar.
std::vector<unsigned char> DownloadUserAvatar(const std::string& username) {
    if(username.empty() || username.size()>256)return {};
    struct HttpHandle {HINTERNET handle;~HttpHandle(){if(handle)WinHttpCloseHandle(handle);}};
    HttpHandle session{WinHttpOpen(L"CloudClientProfile",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.handle)return {};
    WinHttpSetTimeouts(session.handle,2000,2000,2000,2000);
    HttpHandle connection{WinHttpConnect(session.handle,L"dashboard.cloudclient.vip",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.handle)return {};
    auto path="/profile/avatar?user="+AvatarPolicy::EncodeName(username);
    std::wstring wide(path.begin(),path.end());
    HttpHandle request{WinHttpOpenRequest(connection.handle,L"GET",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.handle)return {};
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    if(!WinHttpSendRequest(request.handle,L"Accept: image/png\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request.handle,nullptr))return {};
    DWORD status=0,statusSize=sizeof(status);
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX) || status!=200)return {};
    wchar_t contentType[128]={};DWORD typeSize=sizeof(contentType);
    if(!WinHttpQueryHeaders(request.handle,WINHTTP_QUERY_CONTENT_TYPE,WINHTTP_HEADER_NAME_BY_INDEX,contentType,&typeSize,WINHTTP_NO_HEADER_INDEX) || _wcsicmp(contentType,L"image/png")!=0)return {};
    std::vector<unsigned char> image;unsigned char buffer[8192];DWORD read=0;
    const auto deadline=GetTickCount64()+8000;
    do {
        if(GetTickCount64()>deadline || !WinHttpReadData(request.handle,buffer,sizeof(buffer),&read) || image.size()+read>AvatarPolicy::MaxBytes)return {};
        image.insert(image.end(),buffer,buffer+read);
    }while(read);
    return AvatarPolicy::ValidImage(image)?image:std::vector<unsigned char>{};
}
