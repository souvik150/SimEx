#include "utils/Affinity.h"

#include <pthread.h>
#include <sched.h>

#include <algorithm>

namespace {

inline bool applyAffinity(pthread_t handle, const std::vector<int>& cpus) {
    if (cpus.empty()) {
        return false;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    for (int cpu : cpus) {
        if (cpu < 0) {
            continue;
        }
        CPU_SET(static_cast<unsigned>(cpu), &set);
    }
    return pthread_setaffinity_np(handle, sizeof(cpu_set_t), &set) == 0;
}

}  // namespace

namespace cpu {

bool setCurrentThreadAffinity(const std::vector<int>& cpus) {
    return applyAffinity(pthread_self(), cpus);
}

bool setThreadAffinity(std::thread& thread, const std::vector<int>& cpus) {
    if (!thread.joinable()) {
        return false;
    }
    return applyAffinity(thread.native_handle(), cpus);
}

bool setThreadAffinity(std::thread& thread, int cpu) {
    if (cpu < 0) {
        return false;
    }
    return setThreadAffinity(thread, std::vector<int>{cpu});
}

}  // namespace cpu
