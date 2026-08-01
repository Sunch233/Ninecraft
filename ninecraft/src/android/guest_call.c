#include <ninecraft/android/guest_call.h>

#if defined(_WIN32) && (defined(__i386__) || defined(_M_IX86))

#include <stdarg.h>

uintptr_t ninecraft_call_guest(void *func, int argc, ...) {
    uintptr_t args[8] = {0};
    uintptr_t *args_begin = args;
    uintptr_t *args_end;
    uintptr_t result = 0;
    va_list ap;
    int i;

    if (!func || argc < 0 || argc > (int)(sizeof(args) / sizeof(args[0]))) {
        return 0;
    }

    va_start(ap, argc);
    for (i = 0; i < argc; ++i) {
        args[(argc - 1) - i] = va_arg(ap, uintptr_t);
    }
    va_end(ap);
    args_end = args_begin + argc;

#if defined(_MSC_VER)
    __asm {
        push eax
        push ecx
        push edx
        push ebx
        mov ebx, esp
        mov eax, args_end
        sub eax, args_begin
        sub esp, eax
        and esp, 0FFFFFFF0h
        add esp, eax
        mov eax, args_begin
        cmp eax, args_end
        je ninecraft_guest_call_loop_end
    ninecraft_guest_call_loop:
        push [eax]
        add eax, 4
        cmp eax, args_end
        jne ninecraft_guest_call_loop
    ninecraft_guest_call_loop_end:
        call func
        mov result, eax
        mov esp, ebx
        pop ebx
        pop edx
        pop ecx
        pop eax
    }
#else
    __asm__ volatile (
        "push %%eax \n"
        "push %%ecx \n"
        "push %%edx \n"
        "push %%ebx \n"
        "mov %%esp, %%ebx \n"
        "mov %[args_end], %%eax \n"
        "sub %[args_begin], %%eax \n"
        "sub %%eax, %%esp \n"
        "and $0xFFFFFFF0, %%esp \n"
        "add %%eax, %%esp \n"
        "mov %[args_begin], %%eax \n"
        "cmp %[args_end], %%eax \n"
        "je 2f \n"
        "1: \n"
        "push (%%eax) \n"
        "add $4, %%eax \n"
        "cmp %[args_end], %%eax \n"
        "jne 1b \n"
        "2: \n"
        "call *%[func] \n"
        "mov %%eax, %[result] \n"
        "mov %%ebx, %%esp \n"
        "pop %%ebx \n"
        "pop %%edx \n"
        "pop %%ecx \n"
        "pop %%eax \n"
        : [result] "=r" (result)
        : [args_begin] "r" (args_begin), [args_end] "r" (args_end), [func] "r" (func)
        : "eax", "ebx", "memory"
    );
#endif

    return result;
}

#endif
