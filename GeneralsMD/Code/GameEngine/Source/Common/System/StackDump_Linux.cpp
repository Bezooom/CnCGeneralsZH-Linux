/*
**  Linux Stack Dump Implementation
**  Uses libunwind + dladdr for symbolization
*/

#include "Common/StackDump.h"
#include "PAL_Timing.h"
#include <unwind.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <stdint.h>

AsciiString g_LastErrorDump;

// exe load bias for the addr2line fallback below (dladdr on any global in
// this object gives it; the whole game links statically into one PIE).
static uintptr_t exeBias(void)
{
    Dl_info info;
    if (dladdr((void *)exeBias, &info) && info.dli_fbase)
        return (uintptr_t)info.dli_fbase;
    return 0;
}

// dladdr only sees dynamic symbols, so static functions (like every TEST()
// body) resolve to nothing.  addr2line reads debuginfo and names them;
// RelWithDebInfo ships it, stripped builds fall back to hex gracefully.
static bool addr2lineName(void *ip, char *out, size_t outSize)
{
    char exe[1024];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0)
        return false;
    exe[n] = 0;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "addr2line -f -C -e '%s' %lx 2>/dev/null",
             exe, (unsigned long)((uintptr_t)ip - exeBias()));
    FILE *p = popen(cmd, "r");
    if (!p)
        return false;
    char func[1024] = { 0 };
    bool ok = false;
    if (fgets(func, sizeof(func), p))
    {
        func[strcspn(func, "\r\n")] = 0;
        if (func[0] && strcmp(func, "??") != 0)
        {
            strncpy(out, func, outSize - 1);
            out[outSize - 1] = 0;
            ok = true;
        }
    }
    pclose(p);
    return ok;
}

namespace {
// Internal callback for libunwind (file-local: debuglib's PAL_Timing.cpp
// has its own with the same name).
struct UnwindState {
    void** frames;
    size_t max_frames;
    size_t skip_frames;
    size_t count;
    StackDumpCallback callback;
    bool symbolize;
};

_Unwind_Reason_Code unwind_callback(_Unwind_Context* ctx, void* arg) {
    UnwindState* state = static_cast<UnwindState*>(arg);
    
    if (state->skip_frames > 0) {
        --state->skip_frames;
        return _URC_NO_REASON;
    }
    
    if (state->count < state->max_frames) {
        void* ip = reinterpret_cast<void*>(_Unwind_GetIP(ctx));
        state->frames[state->count++] = ip;
        
        if (state->symbolize && state->callback) {
            Dl_info info;
            if (dladdr(ip, &info) && info.dli_sname) {
                int status = 0;
                char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
                const char* symbol = (status == 0 && demangled) ? demangled : info.dli_sname;
                char line[1024];
                if (info.dli_fname) {
                    snprintf(line, sizeof(line), "  %s +0x%lx  [%s]", 
                             symbol, (char*)ip - (char*)info.dli_fbase, info.dli_fname);
                } else {
                    snprintf(line, sizeof(line), "  %s +0x%lx", 
                             symbol, (char*)ip - (char*)info.dli_fbase);
                }
                if (demangled) free(demangled);
                state->callback(line);
                state->callback("\n");
            } else {
                char line[1024];
                char func[512];
                if (addr2lineName(ip, func, sizeof(func)))
                    snprintf(line, sizeof(line), "  %s  [addr2line]\n", func);
                else
                    snprintf(line, sizeof(line), "  0x%lx\n", (uintptr_t)ip);
                state->callback(line);
            }
        }
        return _URC_NO_REASON;
    }
    return _URC_END_OF_STACK;
}
} // anonymous namespace

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void StackDump(StackDumpCallback callback) {
    if (!callback) callback = [](const char* line) { fputs(line, stderr); };
    
    void* frames[128];
    UnwindState state{frames, 128, 1, 0, callback, true};
    _Unwind_Backtrace(unwind_callback, &state);
}

void FillStackAddresses(void** addresses, unsigned int count, unsigned int skip) {
    UnwindState state{addresses, count, skip + 1, 0, nullptr, false};
    _Unwind_Backtrace(unwind_callback, &state);
}

void StackDumpFromAddresses(void** addresses, unsigned int count, StackDumpCallback callback) {
    if (!callback) callback = [](const char* line) { fputs(line, stderr); };
    
    for (unsigned int i = 0; i < count; ++i) {
        void* ip = addresses[i];
        Dl_info info;
        if (dladdr(ip, &info) && info.dli_sname) {
            int status = 0;
            char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            const char* symbol = (status == 0 && demangled) ? demangled : info.dli_sname;
            char line[1024];
            if (info.dli_fname) {
                snprintf(line, sizeof(line), "  %s +0x%lx  [%s]\n", 
                         symbol, (char*)ip - (char*)info.dli_fbase, info.dli_fname);
            } else {
                snprintf(line, sizeof(line), "  %s +0x%lx\n", 
                         symbol, (char*)ip - (char*)info.dli_fbase);
            }
            if (demangled) free(demangled);
            callback(line);
        } else {
            char line[1024];
            char func[512];
            if (addr2lineName(ip, func, sizeof(func)))
                snprintf(line, sizeof(line), "  %s  [addr2line]\n", func);
            else
                snprintf(line, sizeof(line), "  0x%lx\n", (uintptr_t)ip);
            callback(line);
        }
    }
}

void GetFunctionDetails(void* pointer, char* name, size_t nameSize,
                        char* filename, size_t filenameSize,
                        unsigned int* linenumber, unsigned int* address) {
    Dl_info info;
    if (dladdr(pointer, &info) && info.dli_sname) {
        int status = 0;
        char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        const char* symbol = (status == 0 && demangled) ? demangled : info.dli_sname;
        
        if (name) {
            snprintf(name, nameSize, "%s();", symbol);
        }
        if (filename && info.dli_fname) {
            strncpy(filename, info.dli_fname, filenameSize - 1);
            filename[filenameSize - 1] = '\0';
        }
        if (linenumber) *linenumber = 0; // libunwind doesn't give line numbers easily
        if (address) *address = (unsigned int)((uintptr_t)pointer - (uintptr_t)info.dli_fbase);
        
        if (demangled) free(demangled);
    } else {
        if (name) strncpy(name, "<unknown>", nameSize);
        if (filename) strncpy(filename, "<unknown>", filenameSize);
        if (linenumber) *linenumber = 0;
        if (address) *address = 0;
    }
}

// ------------------------------------------------------------
// Signal handler integration
// ------------------------------------------------------------
static void signal_stack_dump(int sig, siginfo_t* info, void* ucontext) {
    ucontext_t* uc = static_cast<ucontext_t*>(ucontext);
    
    // Print signal info
    char buf[256];
    snprintf(buf, sizeof(buf), 
             "\n********** SIGNAL %d ****************\n"
             "Signal: %d (%s)\n"
             "Address: %p\n",
             sig, sig, strsignal(sig), info->si_addr);
    
    auto log_cb = [](const char* line) {
        fputs(line, stderr);
        if (g_LastErrorDump.isNotEmpty()) {
            g_LastErrorDump.concat(line);
        }
    };
    
    log_cb(buf);
    log_cb("\nStack Dump:\n");
    
    // Capture stack from ucontext
    void* frames[128];
    UnwindState state{frames, 128, 0, 0, log_cb, true};
    _Unwind_Backtrace(unwind_callback, &state);
    
    log_cb("********** END SIGNAL DUMP ****************\n\n");
    
    // Re-raise with default handler
    signal(sig, SIG_DFL);
    raise(sig);
}

void StackDumpFromContext(ucontext_t* uc, StackDumpCallback callback) {
    if (!callback) callback = [](const char* line) { fputs(line, stderr); };
    
    // For Linux, we just do a normal backtrace since we don't have
    // easy access to modify the instruction pointer from ucontext
    (void)uc; // unused for now
    StackDump(callback);
}

void DumpExceptionInfo(int sig, siginfo_t* info, void* ucontext) {
    signal_stack_dump(sig, info, ucontext);
}

// ------------------------------------------------------------
// Install signal handlers for crash reporting
// ------------------------------------------------------------
void Platform_InstallCrashHandlers() {
    struct sigaction sa;
    sa.sa_sigaction = signal_stack_dump;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    
    // Stack for signal handler
    static stack_t ss;
    ss.ss_sp = malloc(SIGSTKSZ);
    ss.ss_size = SIGSTKSZ;
    ss.ss_flags = 0;
    sigaltstack(&ss, nullptr);
    
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}