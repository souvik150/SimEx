#pragma once

#include <thread>
#include <vector>

namespace cpu {

bool setCurrentThreadAffinity(const std::vector<int>& cpus);
bool setThreadAffinity(std::thread& thread, const std::vector<int>& cpus);
bool setThreadAffinity(std::thread& thread, int cpu);

}  // namespace cpu
