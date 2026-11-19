#ifndef PROCESS_MEMORY_H
#define PROCESS_MEMORY_H

#include <cstddef>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX // Иначе макросы min/max из windows.h ломают std::min/std::max
    #endif
    #include <windows.h>
    #include <psapi.h>
#else
    #include <cstdio>
    #include <unistd.h>
#endif

// Физическая память процесса (working set / RSS) в байтах, 0 при ошибке
inline std::size_t get_process_memory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
        return counters.WorkingSetSize;
    }
    return 0;
#else
    std::FILE* file = std::fopen("/proc/self/statm", "r");
    if (file == nullptr) return 0;
    long total_pages = 0;
    long resident_pages = 0;
    int read = std::fscanf(file, "%ld %ld", &total_pages, &resident_pages);
    std::fclose(file);
    if (read != 2) return 0;
    return static_cast<std::size_t>(resident_pages) * sysconf(_SC_PAGESIZE);
#endif
}

#endif
