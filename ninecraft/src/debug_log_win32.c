#include <ninecraft/debug_log.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <io.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define NINECRAFT_DEBUG_PATH_CAPACITY 32768

bool ninecraft_enable_debug_log(void) {
    static const wchar_t log_name[] = L"debug.log";
    wchar_t executable_path[NINECRAFT_DEBUG_PATH_CAPACITY];
    wchar_t *separator;
    DWORD path_length;
    size_t directory_length;
    FILE *redirected_stdout = NULL;
    FILE *redirected_stderr = NULL;
    int output_descriptor;
    int error_descriptor;
    intptr_t output_handle;
    intptr_t error_handle;

    path_length = GetModuleFileNameW(
        NULL,
        executable_path,
        (DWORD)(sizeof(executable_path) / sizeof(executable_path[0])));
    if (!path_length ||
        path_length >= sizeof(executable_path) / sizeof(executable_path[0])) {
        return false;
    }

    separator = wcsrchr(executable_path, L'\\');
    if (!separator) {
        separator = wcsrchr(executable_path, L'/');
    }
    if (!separator) {
        return false;
    }
    directory_length = (size_t)(separator - executable_path) + 1;
    if (directory_length +
            sizeof(log_name) / sizeof(log_name[0]) >
        sizeof(executable_path) / sizeof(executable_path[0])) {
        return false;
    }
    memcpy(
        executable_path + directory_length,
        log_name,
        sizeof(log_name));

    if (_wfreopen_s(
            &redirected_stdout,
            executable_path,
            L"w",
            stdout) != 0 ||
        !redirected_stdout) {
        return false;
    }
    if (_wfreopen_s(
            &redirected_stderr,
            L"NUL",
            L"w",
            stderr) != 0 ||
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
    return true;
}

#endif
