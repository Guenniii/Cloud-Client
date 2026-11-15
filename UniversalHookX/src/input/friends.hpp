#pragma once
#include "../modules/settings.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../dependencies/imgui/imgui.h"
#include "friend_names.hpp"
#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
namespace Friends {
inline std::string applied,error,notice;
inline bool loaded=false,published=false,lastEnabled=false;
inline jclass bridge=nullptr;inline jmethodID configure=nullptr,targetName=nullptr;
inline double retry=0;
inline std::filesystem::path Path() {
    HMODULE module=nullptr;wchar_t path[32768]={};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&loaded),&module))return {};
    if(!GetModuleFileNameW(module,path,32768))return {};
    return std::filesystem::path(path).parent_path()/L"friends.txt";
}
inline void Load() {
    if(loaded)return;loaded=true;std::ifstream file(Path());std::string value;
    if(file) {value.resize(4096);file.read(value.data(),value.size());value.resize(static_cast<size_t>(file.gcount()));
        if(!file.bad() && file.peek()==std::char_traits<char>::eof()) {
            if(auto names=FriendNames::Parse(value))applied=FriendNames::Join(*names);else error="Saved friend list contains invalid names.";
        }else error="Saved friend list is too large.";
    }
}
inline std::vector<std::string> Names() {Load();return FriendNames::Parse(applied).value_or(std::vector<std::string>{});}
inline bool Save(const std::vector<std::string>& names) {
    auto value=FriendNames::Join(names);auto path=Path();if(path.empty()){error="Storage path is unavailable.";return false;}
    auto temporary=path;temporary+=L".tmp";
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
    if(!file || !(file<<value)){error="Could not save the friend list.";return false;}
    file.close();
    if(file.fail() || !MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {error="Could not save the friend list.";return false;}
    applied=value;published=false;error.clear();return true;
}
inline bool Add(std::string_view name) {
    if(!FriendNames::Valid(name)){error="Account name: 1-16 letters, numbers or underscores.";return false;}
    auto names=Names();if(FriendNames::Contains(names,name)){error.clear();notice=std::string(name)+" is already a friend.";return true;}
    if(names.size()>=128){error="A maximum of 128 friends is allowed.";return false;}
    names.emplace_back(name);if(!Save(names))return false;notice=std::string(name)+" added.";return true;
}
inline bool Remove(std::string_view name) {
    auto names=Names();auto lower=FriendNames::Lower(name);
    std::erase_if(names,[&](const auto& item){return FriendNames::Lower(item)==lower;});
    if(!Save(names))return false;notice=std::string(name)+" removed.";return true;
}
inline void Sync() {
    Load();bool enabled=Friends_Enabled.load();if(published && enabled==lastEnabled)return;
    if(ImGui::GetTime()<retry)return;
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env || env->ExceptionCheck())return;
    JniSafety::LocalFrame local(env,8);if(!local)return;
    if(!bridge) {
        if(!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar"))return;
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.player.FriendsBridge");
        if(cls && !env->ExceptionCheck()) {
            auto method=env->GetStaticMethodID(cls,"configure","(ZLjava/lang/String;)V");
            auto target=env->ExceptionCheck()?nullptr:env->GetStaticMethodID(cls,"targetName","()Ljava/lang/String;");
            if(method && target && !env->ExceptionCheck()){bridge=static_cast<jclass>(env->NewGlobalRef(cls));configure=method;targetName=target;}
        }
    }
    if(bridge && configure) {auto text=env->NewStringUTF(applied.c_str());if(text)env->CallStaticVoidMethod(bridge,configure,static_cast<jboolean>(enabled),text);}
    if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();retry=ImGui::GetTime()+5;return;}
    if(bridge){published=true;lastEnabled=enabled;}
}
inline void PollMiddleClick() {
    static bool previous=false;bool down=(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;bool pressed=down && !previous;previous=down;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    if(!pressed || !Friends_MiddleClick || !Friends_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || !bridge || !targetName)return;
    auto env=SeedCracker::GetMinecraftJNIEnv();if(!env || env->ExceptionCheck())return;
    JniSafety::LocalFrame local(env,4);if(!local)return;
    auto name=static_cast<jstring>(env->CallStaticObjectMethod(bridge,targetName));
    if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();return;}if(!name)return;
    const char* utf=env->GetStringUTFChars(name,nullptr);
    if(!utf){if(env->ExceptionCheck())env->ExceptionClear();return;}
    std::string account=utf;env->ReleaseStringUTFChars(name,utf);
    if(!account.empty() && Add(account)){Sync();std::printf("[Friends] %s\n",notice.c_str());}
}
inline void Clear(JNIEnv* env) {
    if(env && bridge) {auto empty=env->NewStringUTF("");if(empty){env->CallStaticVoidMethod(bridge,configure,JNI_FALSE,empty);env->DeleteLocalRef(empty);}if(env->ExceptionCheck())env->ExceptionClear();env->DeleteGlobalRef(bridge);}
    bridge=nullptr;configure=targetName=nullptr;published=false;
}
}
