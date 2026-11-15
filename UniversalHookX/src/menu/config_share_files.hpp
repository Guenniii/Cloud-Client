#pragma once
#include "../utils/config/share_code.hpp"
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <vector>
#pragma comment(lib,"comdlg32.lib")
namespace Menu::ConfigShareFiles {
inline std::filesystem::path Pick(bool save,std::string& error){
 wchar_t file[32768]=L"Shared config.ccfg";
 OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=GetActiveWindow();
 dialog.lpstrFilter=L"Shared config (*.ccfg)\0*.ccfg\0All files\0*.*\0";dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.lpstrDefExt=L"ccfg";
 dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
 if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))return file;
 if(CommDlgExtendedError())error="The file dialog could not be opened.";return {};
}
inline bool Read(const std::filesystem::path& path,std::string& code,std::string& error){
 std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
 if(ec||size>ConfigShareCode::MaxCode){error="Could not read this file, or it exceeds 180 KB.";return false;}
 std::ifstream input(path,std::ios::binary);std::string text(static_cast<size_t>(size),'\0');
 if(!input||!input.read(text.data(),static_cast<std::streamsize>(text.size()))){error="Could not read the config file.";return false;}
 code=std::move(text);return true;
}
inline bool Write(const std::filesystem::path& path,const std::string& code,std::string& error){
 if(code.empty()||code.size()>ConfigShareCode::MaxCode){error="No valid export is available.";return false;}
 auto temporary=path;temporary+=L".sharing.tmp";
 std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
 if(!output){error="Could not create the export file.";return false;}
 output.write(code.data(),static_cast<std::streamsize>(code.size()));output.flush();bool good=bool(output);output.close();
 if(!good||output.fail()||!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
  std::error_code ignored;std::filesystem::remove(temporary,ignored);error="Export failed; the existing file was preserved.";return false;
 }
 return true;
}
}
