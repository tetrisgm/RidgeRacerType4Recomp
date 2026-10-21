/* Bounded moderate CPU load for acceptance: eight normal-priority workers,
 * 25 ms work / 25 ms sleep by default. No affinity or persistent jobs.
 * Build: gcc -O2 tools/win_cpu_load.c -o <local-output.exe>
 * Usage: win_cpu_load seconds [workers]. Reports achieved process CPU time. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static ULONGLONG deadline;
static DWORD WINAPI worker(void *arg) {
    volatile uint64_t value = (uintptr_t)arg + 1;
    while (GetTickCount64() < deadline) {
        ULONGLONG work_until = GetTickCount64()+25;
        do {
            for (unsigned i=0; i<1000; ++i)
                value = value*6364136223846793005ull+1442695040888963407ull;
        } while (GetTickCount64() < work_until && GetTickCount64() < deadline);
        Sleep(25);
    }
    return 0;
}
static uint64_t ticks(FILETIME t) {
    return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime;
}
int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) return 2;
    unsigned seconds = (unsigned)strtoul(argv[1], NULL, 10);
    unsigned count = argc == 3 ? (unsigned)strtoul(argv[2], NULL, 10) : 8;
    if (!seconds || seconds>180 || !count || count>16) return 2;
    HANDLE threads[16];
    ULONGLONG start=GetTickCount64(); deadline=start+seconds*1000ull;
    for (unsigned i=0; i<count; ++i) {
        threads[i]=CreateThread(NULL,0,worker,(void *)(uintptr_t)i,0,NULL);
        if (!threads[i]) return 3;
    }
    WaitForMultipleObjects(count,threads,TRUE,INFINITE);
    for (unsigned i=0; i<count; ++i) CloseHandle(threads[i]);
    FILETIME created, exited, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    double wall=(GetTickCount64()-start)/1000.0;
    double cpu=(ticks(kernel)+ticks(user))/1e7;
    SYSTEM_INFO si; GetSystemInfo(&si);
    printf("workers=%u wall_s=%.3f cpu_s=%.3f host_cpu_percent=%.2f logical_cpus=%lu\n",
           count,wall,cpu,100*cpu/wall/si.dwNumberOfProcessors,si.dwNumberOfProcessors);
    return 0;
}
