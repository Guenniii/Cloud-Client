#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
namespace Lifecycle {
inline wchar_t tracePath[MAX_PATH]{};
inline void InitTrace(HMODULE module) {
 GetModuleFileNameW(module,tracePath,MAX_PATH);
 wchar_t* slash=wcsrchr(tracePath,L'\\');
 if(slash) wcscpy_s(slash+1,MAX_PATH-(slash+1-tracePath),L"unload-diagnostic.log");
}
inline void Trace(const char* stage) {
 if(!tracePath[0]) return;
 HANDLE file=CreateFileW(tracePath,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE) return;
 char line[512];SYSTEMTIME time;GetLocalTime(&time);
 int length=wsprintfA(line,"%02u:%02u:%02u [thread %lu] %s\r\n",time.wHour,time.wMinute,time.wSecond,GetCurrentThreadId(),stage);
 DWORD written;WriteFile(file,line,length,&written,nullptr);FlushFileBuffers(file);CloseHandle(file);
}

inline std::atomic<bool> requested{false}, workersStopped{false}, renderStopped{false};
inline std::atomic<unsigned> callbacks{0};
inline std::mutex workersMutex;
inline std::recursive_mutex renderMutex;
inline std::vector<std::thread> workers;
struct Callback { Callback(){callbacks.fetch_add(1);} ~Callback(){callbacks.fetch_sub(1);} };
template<class F> void Spawn(F&& fn) {
 std::lock_guard<std::mutex> lock(workersMutex);
 if(!requested) workers.emplace_back(std::forward<F>(fn));
}
inline void JoinWorkers() {
 std::vector<std::thread> pending;
 {std::lock_guard<std::mutex> lock(workersMutex);pending.swap(workers);}
 for(auto& worker:pending) if(worker.joinable()) worker.join();
 workersStopped=true;
}
// After entry points are detached, also wait for callback epilogues to leave our image.
inline bool WaitOutsideImage(HMODULE module) {
 auto base=reinterpret_cast<uintptr_t>(module);
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
 auto end=base+nt->OptionalHeader.SizeOfImage;
 for(;;) {
  bool inside=callbacks.load()!=0, failed=false;
  HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
  if(snapshot==INVALID_HANDLE_VALUE) return false;
  THREADENTRY32 entry{};entry.dwSize=sizeof(entry);
  if(!Thread32First(snapshot,&entry)) {CloseHandle(snapshot);return false;}
  do {
   if(entry.th32OwnerProcessID!=GetCurrentProcessId() || entry.th32ThreadID==GetCurrentThreadId()) continue;
   HANDLE thread=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
   if(!thread) {if(GetLastError()!=ERROR_INVALID_PARAMETER) failed=true;continue;}
   if(SuspendThread(thread)==DWORD(-1)) {CloseHandle(thread);failed=true;continue;}
   CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL;
   BOOL valid=GetThreadContext(thread,&context);
#ifdef _WIN64
   auto ip=context.Rip;
#else
   auto ip=context.Eip;
#endif
   // Always resume before closing, waiting, allocating or running destructors.
   ResumeThread(thread);CloseHandle(thread);
   if(!valid) failed=true;
   else if(ip>=base && ip<end) inside=true;
  } while(Thread32Next(snapshot,&entry));
  CloseHandle(snapshot);
  if(failed) return false;
  if(!inside && callbacks.load()==0) return true;
  Sleep(1);
 }
}

}
