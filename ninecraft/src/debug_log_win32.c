#include <ninecraft/debug_log.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

#include <ancmp/android_dlfcn.h>

#include <io.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define NINECRAFT_DEBUG_PATH_CAPACITY 32768
#define NINECRAFT_CRASH_STACK_WORDS 64
#define NINECRAFT_CRASH_CODE_CANDIDATES 16

typedef BOOL (WINAPI *ninecraft_mini_dump_write_dump_t)(
    HANDLE process,
    DWORD process_id,
    HANDLE file,
    MINIDUMP_TYPE dump_type,
    PMINIDUMP_EXCEPTION_INFORMATION exception,
    PMINIDUMP_USER_STREAM_INFORMATION user_stream,
    PMINIDUMP_CALLBACK_INFORMATION callback);

static bool ninecraft_debug_log_enabled;
static const char *volatile ninecraft_crash_phase = "early startup";
static volatile LONG ninecraft_handling_crash;
static HMODULE ninecraft_dbghelp_module;
static ninecraft_mini_dump_write_dump_t ninecraft_mini_dump_write_dump;
static wchar_t ninecraft_crash_dump_path[NINECRAFT_DEBUG_PATH_CAPACITY];

static bool ninecraft_get_executable_sibling_path(
    wchar_t *path,
    size_t path_capacity,
    const wchar_t *file_name) {
    wchar_t *separator;
    DWORD path_length;
    size_t directory_length;
    size_t file_name_length;

    if (!path || !path_capacity || !file_name) {
        return false;
    }

    path_length = GetModuleFileNameW(NULL, path, (DWORD)path_capacity);
    if (!path_length || path_length >= path_capacity) {
        return false;
    }

    separator = wcsrchr(path, L'\\');
    if (!separator) {
        separator = wcsrchr(path, L'/');
    }
    if (!separator) {
        return false;
    }

    directory_length = (size_t)(separator - path) + 1;
    file_name_length = wcslen(file_name) + 1;
    if (directory_length + file_name_length > path_capacity) {
        return false;
    }
    memcpy(path + directory_length, file_name, file_name_length * sizeof(wchar_t));
    return true;
}

static void ninecraft_crash_write(const char *format, ...) {
    char buffer[2048];
    va_list args;
    int length;
    DWORD written;
    HANDLE output;

    va_start(args, format);
#if defined(_MSC_VER)
    length = _vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
    length = vsnprintf(buffer, sizeof(buffer), format, args);
#endif
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        length = (int)strlen(buffer);
    }

    output = GetStdHandle(STD_ERROR_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE && length > 0) {
        WriteFile(output, buffer, (DWORD)length, &written, NULL);
        FlushFileBuffers(output);
    }
}

static const char *ninecraft_exception_name(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:
            return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
            return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:
            return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT:
            return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INVALID_OPERATION:
            return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR:
            return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:
            return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_STACK_OVERFLOW:
            return "EXCEPTION_STACK_OVERFLOW";
        default:
            return "unknown";
    }
}

static bool ninecraft_address_is_executable(uintptr_t address) {
    MEMORY_BASIC_INFORMATION memory;
    DWORD protection;

    if (!address || !VirtualQuery((const void *)address, &memory, sizeof(memory)) ||
        memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
        return false;
    }

    protection = memory.Protect & 0xff;
    return protection == PAGE_EXECUTE ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

static void ninecraft_describe_address(const char *label, uintptr_t address) {
    android_Dl_info elf_info;
    MEMORY_BASIC_INFORMATION memory;
    char module_path[MAX_PATH];
    DWORD module_path_length;

    ninecraft_crash_write("%s0x%08lx", label ? label : "", (unsigned long)address);
    if (!address) {
        ninecraft_crash_write(" (NULL)\r\n");
        return;
    }

    memset(&elf_info, 0, sizeof(elf_info));
    if (android_dladdr((const void *)address, &elf_info) && elf_info.dli_fbase) {
        ninecraft_crash_write(
            " (%s+0x%lx",
            elf_info.dli_fname ? elf_info.dli_fname : "<ELF>",
            (unsigned long)(address - (uintptr_t)elf_info.dli_fbase));
        if (elf_info.dli_sname && elf_info.dli_saddr) {
            ninecraft_crash_write(
                ", %s+0x%lx",
                elf_info.dli_sname,
                (unsigned long)(address - (uintptr_t)elf_info.dli_saddr));
        }
        ninecraft_crash_write(")\r\n");
        return;
    }

    if (VirtualQuery((const void *)address, &memory, sizeof(memory))) {
        module_path_length = GetModuleFileNameA(
            (HMODULE)memory.AllocationBase,
            module_path,
            (DWORD)sizeof(module_path));
        if (module_path_length && module_path_length < sizeof(module_path)) {
            module_path[module_path_length] = '\0';
            ninecraft_crash_write(
                " (%s+0x%lx)\r\n",
                module_path,
                (unsigned long)(address - (uintptr_t)memory.AllocationBase));
            return;
        }
        ninecraft_crash_write(
            " (allocation=%p, protect=0x%08lx)\r\n",
            memory.AllocationBase,
            (unsigned long)memory.Protect);
        return;
    }

    ninecraft_crash_write(" (unmapped)\r\n");
}

static void ninecraft_dump_stack(CONTEXT *context) {
#if defined(_M_IX86) || defined(__i386__)
    DWORD words[NINECRAFT_CRASH_STACK_WORDS];
    uintptr_t seen[NINECRAFT_CRASH_CODE_CANDIDATES];
    SIZE_T bytes_read = 0;
    size_t word_count;
    size_t i;
    size_t seen_count = 0;

    memset(words, 0, sizeof(words));
    memset(seen, 0, sizeof(seen));
    if (!ReadProcessMemory(
            GetCurrentProcess(),
            (const void *)(uintptr_t)context->Esp,
            words,
            sizeof(words),
            &bytes_read)) {
        ninecraft_crash_write(
            "Unable to read stack at ESP=0x%08lx (error=%lu)\r\n",
            (unsigned long)context->Esp,
            (unsigned long)GetLastError());
        return;
    }

    word_count = bytes_read / sizeof(words[0]);
    ninecraft_crash_write("Raw stack (%lu DWORDs from ESP):\r\n", (unsigned long)word_count);
    for (i = 0; i < word_count; i += 4) {
        ninecraft_crash_write(
            "  ESP+0x%03lx: %08lx %08lx %08lx %08lx\r\n",
            (unsigned long)(i * sizeof(words[0])),
            (unsigned long)words[i],
            (unsigned long)(i + 1 < word_count ? words[i + 1] : 0),
            (unsigned long)(i + 2 < word_count ? words[i + 2] : 0),
            (unsigned long)(i + 3 < word_count ? words[i + 3] : 0));
    }

    if (word_count > 0) {
        ninecraft_describe_address(
            "Probable return address for a NULL call: ",
            (uintptr_t)words[0]);
    }

    ninecraft_crash_write("Executable-looking stack candidates:\r\n");
    for (i = 0; i < word_count && seen_count < NINECRAFT_CRASH_CODE_CANDIDATES; ++i) {
        uintptr_t candidate = (uintptr_t)words[i];
        size_t j;
        bool duplicate = false;

        if (!ninecraft_address_is_executable(candidate)) {
            continue;
        }
        for (j = 0; j < seen_count; ++j) {
            if (seen[j] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        seen[seen_count++] = candidate;
        ninecraft_crash_write("  [ESP+0x%03lx] ", (unsigned long)(i * sizeof(words[0])));
        ninecraft_describe_address(NULL, candidate);
    }
#else
    (void)context;
    ninecraft_crash_write("Register/stack decoding is only implemented for Win32 x86.\r\n");
#endif
}

static void ninecraft_write_minidump(EXCEPTION_POINTERS *exception) {
    HANDLE file;
    MINIDUMP_EXCEPTION_INFORMATION dump_exception;
    BOOL success;
    DWORD error;

    if (!ninecraft_debug_log_enabled || !ninecraft_mini_dump_write_dump ||
        !ninecraft_crash_dump_path[0]) {
        return;
    }

    file = CreateFileW(
        ninecraft_crash_dump_path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file == INVALID_HANDLE_VALUE) {
        ninecraft_crash_write("Unable to create crash.dmp (error=%lu)\r\n", (unsigned long)GetLastError());
        return;
    }

    dump_exception.ThreadId = GetCurrentThreadId();
    dump_exception.ExceptionPointers = exception;
    dump_exception.ClientPointers = FALSE;
    success = ninecraft_mini_dump_write_dump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        MiniDumpNormal,
        &dump_exception,
        NULL,
        NULL);
    error = success ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);

    if (success) {
        ninecraft_crash_write("Minidump written to crash.dmp beside ninecraft.exe.\r\n");
    } else {
        ninecraft_crash_write("MiniDumpWriteDump failed (error=%lu).\r\n", (unsigned long)error);
    }
}

static LONG WINAPI ninecraft_exception_filter(EXCEPTION_POINTERS *exception) {
    EXCEPTION_RECORD *record;
    CONTEXT *context;
    HANDLE output;

    if (InterlockedExchange(&ninecraft_handling_crash, 1) != 0) {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    if (!exception || !exception->ExceptionRecord || !exception->ContextRecord) {
        ninecraft_crash_write("Unhandled exception without a valid exception context.\r\n");
        return EXCEPTION_EXECUTE_HANDLER;
    }

    record = exception->ExceptionRecord;
    context = exception->ContextRecord;
    ninecraft_crash_write("\r\n========== Ninecraft crash diagnostics ==========\r\n");
    ninecraft_crash_write(
        "Exception: 0x%08lx (%s), flags=0x%08lx, address=%p, thread=%lu\r\n",
        (unsigned long)record->ExceptionCode,
        ninecraft_exception_name(record->ExceptionCode),
        (unsigned long)record->ExceptionFlags,
        record->ExceptionAddress,
        (unsigned long)GetCurrentThreadId());
    ninecraft_crash_write(
        "Runtime phase: %s\r\n",
        ninecraft_crash_phase ? (const char *)ninecraft_crash_phase : "<unknown>");

    if ((record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
         record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
        record->NumberParameters >= 2) {
        const char *operation = "unknown";
        if (record->ExceptionInformation[0] == 0) {
            operation = "read";
        } else if (record->ExceptionInformation[0] == 1) {
            operation = "write";
        } else if (record->ExceptionInformation[0] == 8) {
            operation = "execute";
        }
        ninecraft_crash_write(
            "Access violation: operation=%s (%lu), target=%p\r\n",
            operation,
            (unsigned long)record->ExceptionInformation[0],
            (void *)(uintptr_t)record->ExceptionInformation[1]);
    }

#if defined(_M_IX86) || defined(__i386__)
    ninecraft_crash_write(
        "Registers: EIP=%08lx ESP=%08lx EBP=%08lx EFLAGS=%08lx\r\n",
        (unsigned long)context->Eip,
        (unsigned long)context->Esp,
        (unsigned long)context->Ebp,
        (unsigned long)context->EFlags);
    ninecraft_crash_write(
        "           EAX=%08lx EBX=%08lx ECX=%08lx EDX=%08lx\r\n",
        (unsigned long)context->Eax,
        (unsigned long)context->Ebx,
        (unsigned long)context->Ecx,
        (unsigned long)context->Edx);
    ninecraft_crash_write(
        "           ESI=%08lx EDI=%08lx\r\n",
        (unsigned long)context->Esi,
        (unsigned long)context->Edi);
    ninecraft_describe_address("Fault instruction: ", (uintptr_t)context->Eip);
#endif

    ninecraft_dump_stack(context);
    ninecraft_write_minidump(exception);
    ninecraft_crash_write("===================================================\r\n");

    output = GetStdHandle(STD_ERROR_HANDLE);
    if (output && output != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(output);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

bool ninecraft_enable_debug_log(void) {
    static const wchar_t log_name[] = L"debug.log";
    wchar_t log_path[NINECRAFT_DEBUG_PATH_CAPACITY];
    FILE *redirected_stdout = NULL;
    FILE *redirected_stderr = NULL;
    int output_descriptor;
    int error_descriptor;
    intptr_t output_handle;
    intptr_t error_handle;

    if (!ninecraft_get_executable_sibling_path(
            log_path,
            sizeof(log_path) / sizeof(log_path[0]),
            log_name)) {
        return false;
    }

    if (_wfreopen_s(&redirected_stdout, log_path, L"w", stdout) != 0 ||
        !redirected_stdout) {
        return false;
    }
    if (_wfreopen_s(&redirected_stderr, L"NUL", L"w", stderr) != 0 ||
        !redirected_stderr) {
        return false;
    }
    output_descriptor = _fileno(redirected_stdout);
    error_descriptor = _fileno(redirected_stderr);
    if (output_descriptor < 0 || error_descriptor < 0 ||
        _dup2(output_descriptor, error_descriptor) != 0 ||
        _dup2(output_descriptor, 1) != 0 ||
        _dup2(output_descriptor, 2) != 0) {
        return false;
    }

    output_handle = _get_osfhandle(1);
    error_handle = _get_osfhandle(2);
    if (output_handle == (intptr_t)INVALID_HANDLE_VALUE ||
        error_handle == (intptr_t)INVALID_HANDLE_VALUE ||
        !SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)output_handle) ||
        !SetStdHandle(STD_ERROR_HANDLE, (HANDLE)error_handle)) {
        return false;
    }

    ninecraft_debug_log_enabled = true;
    return true;
}

void ninecraft_crash_set_phase(const char *phase) {
    ninecraft_crash_phase = phase ? phase : "<unknown>";
}

void ninecraft_install_crash_handler(void) {
    SetUnhandledExceptionFilter(ninecraft_exception_filter);

    if (!ninecraft_debug_log_enabled) {
        return;
    }

    ninecraft_crash_dump_path[0] = L'\0';
    ninecraft_get_executable_sibling_path(
        ninecraft_crash_dump_path,
        sizeof(ninecraft_crash_dump_path) / sizeof(ninecraft_crash_dump_path[0]),
        L"crash.dmp");

    ninecraft_dbghelp_module = LoadLibraryA("dbghelp.dll");
    if (ninecraft_dbghelp_module) {
        ninecraft_mini_dump_write_dump =
            (ninecraft_mini_dump_write_dump_t)GetProcAddress(
                ninecraft_dbghelp_module,
                "MiniDumpWriteDump");
    }
    ninecraft_crash_write(
        "Crash diagnostics enabled (MiniDumpWriteDump=%p).\r\n",
        (void *)ninecraft_mini_dump_write_dump);
}

#endif
