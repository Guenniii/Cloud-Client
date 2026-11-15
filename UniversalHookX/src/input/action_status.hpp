#pragma once
#include <mutex>
#include <string>
#include <utility>

namespace ActionStatus {
inline std::mutex mutex;
inline std::string lastConflict;
inline void Set(std::string value) { std::lock_guard lock(mutex); lastConflict=std::move(value); }
inline std::string Read() { std::lock_guard lock(mutex); return lastConflict; }
}
