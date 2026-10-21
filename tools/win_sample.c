/* External Windows sampling profiler for an already shell-launched runtime.
 * Records only code addresses; no symbols/debug tools in the target required.
 * Sampling briefly suspends selected threads: use for attribution, never FPS
 * acceptance. CSV is written after sampling, not on the target's hot path.
 * Build: gcc -O2 tools/win_sample.c -o <local-output.exe>
 * Usage: win_sample PID seconds new-output.csv [all]
 * The default selects the earliest process thread; all selects every thread. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uintptr_t base, end; char name[MAX_PATH]; } Module;
typedef struct { uint64_t us; uintptr_t pc, stack[16]; DWORD tid; } Sample;
static Module modules[512];
static unsigned module_count;
static uint64_t ft_value(FILETIME f) {
    return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime;
}
static const Module *module_at(uintptr_t pc) {
    for (unsigned i=0; i<module_count; ++i)
        if (pc >= modules[i].base && pc < modules[i].end) return &modules[i];
    return NULL;
}
static void load_modules(DWORD pid) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32 me = {0}; me.dwSize = sizeof me;
    unsigned count = 0;
    if (Module32First(snapshot, &me)) do {
        Module *m = &modules[count++];
        m->base = (uintptr_t)me.modBaseAddr; m->end = m->base + me.modBaseSize;
        snprintf(m->name, sizeof m->name, "%s", me.szModule);
    } while (count < 512 && Module32Next(snapshot, &me));
    if (count) module_count = count;
    CloseHandle(snapshot);
}
int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) return 2;
    int all = argc == 5 && !strcmp(argv[4], "all");
    if (argc == 5 && !all) return 2;
    DWORD pid = (DWORD)strtoul(argv[1], NULL, 10);
    unsigned seconds = (unsigned)strtoul(argv[2], NULL, 10);
    if (!pid || !seconds || seconds > 60 || GetFileAttributesA(argv[3]) != INVALID_FILE_ATTRIBUTES)
        return 2;
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
    if (!process) return 3;
    FILETIME created, exit_time, kernel, user;
    if (!GetProcessTimes(process, &created, &exit_time, &kernel, &user)) return 3;
    uint64_t origin = ft_value(created);
    load_modules(pid);
    if (!module_count) return 3;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = {0}; te.dwSize = sizeof te;
    HANDLE target = NULL; uint64_t earliest = UINT64_MAX;
    HANDLE targets[256]; unsigned target_count = 0;
    if (Thread32First(snapshot, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE t = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, te.th32ThreadID);
        if (all) {
            if (t && target_count < 256) targets[target_count++] = t;
            else if (t) CloseHandle(t);
            continue;
        }
        FILETIME c, e, k, u;
        if (t && GetThreadTimes(t, &c, &e, &k, &u) && ft_value(c) < earliest) {
            if (target) CloseHandle(target);
            target = t; earliest = ft_value(c);
        } else if (t) CloseHandle(t);
    } while (Thread32Next(snapshot, &te));
    CloseHandle(snapshot);
    if (!all && target) targets[target_count++] = target;
    if (!target_count) return 3;
    unsigned capacity = seconds * 1000 * target_count, count = 0;
    if (capacity > 200000) capacity = 200000;
    Sample *samples = calloc(capacity, sizeof *samples);
    if (!samples) return 4;
    HANDLE timer = CreateWaitableTimerExW(NULL, NULL, 0x2u, TIMER_ALL_ACCESS);
    ULONGLONG deadline = GetTickCount64() + seconds * 1000ull;
    while (GetTickCount64() < deadline && WaitForSingleObject(process, 0) == WAIT_TIMEOUT && count < capacity) {
        LARGE_INTEGER due; due.QuadPart = all ? -50000 : -10000;
        if (timer && SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
            WaitForSingleObject(timer, INFINITE);
        else Sleep(1);
        for (unsigned ti=0; ti<target_count && count<capacity; ++ti) {
        target = targets[ti];
        if (SuspendThread(target) == (DWORD)-1) continue;
        CONTEXT ctx = {0}; ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(target, &ctx)) {
            Sample *s = &samples[count++]; FILETIME now;
            GetSystemTimePreciseAsFileTime(&now);
            s->us = (ft_value(now)-origin)/10; s->pc = (uintptr_t)ctx.Rip;
            s->tid = GetThreadId(target);
            uintptr_t words[128]; SIZE_T got = 0;
            if (ReadProcessMemory(process, (void *)(uintptr_t)ctx.Rsp, words, sizeof words, &got)) {
                unsigned n = 0;
                for (unsigned i=0; i<got/sizeof *words && n<16; ++i)
                    if (words[i] >= modules[0].base && words[i] < modules[0].end)
                        s->stack[n++] = words[i]-modules[0].base;
            }
        }
        ResumeThread(target);
        }
    }
    if (timer) CloseHandle(timer);
    load_modules(pid); /* GL/audio drivers can be loaded after process launch. */
    for (unsigned ti=0; ti<target_count; ++ti) CloseHandle(targets[ti]);
    CloseHandle(process);
    FILE *f = fopen(argv[3], "w"); if (!f) return 5;
    fputs("us,tid,module,rva", f);
    for (unsigned i=0;i<16;i++) fprintf(f, ",stack%u", i);
    fputc('\n', f);
    for (unsigned i=0;i<count;i++) {
        Sample *s=&samples[i]; const Module *m=module_at(s->pc);
        fprintf(f, "%llu,%lu,%s,%llx", (unsigned long long)s->us, s->tid, m?m->name:"unknown",
                (unsigned long long)(m?s->pc-m->base:s->pc));
        for (unsigned j=0;j<16;j++) fprintf(f, ",%llx", (unsigned long long)s->stack[j]);
        fputc('\n', f);
    }
    free(samples);
    return fclose(f) ? 5 : 0;
}
