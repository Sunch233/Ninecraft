#include "hooks.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <signal.h>
#include <locale.h>
#include <wctype.h>
#include <errno.h>
#include <time.h>
#include <wchar.h>
#include "android_fnmatch.h"
#include "android_pthread.h"
#include "android_math.h"
#include "android_io.h"
#include "android_ctype.h"
#include "android_stat.h"
#include "android_socket.h"
#include "android_dirent.h"
#include "android_posix_types.h"
#include "android_fcntl.h"
#include "android_ioctl.h"
#include "android_mmap.h"
#include "android_dlfcn.h"
#include "linker.h"
#include "android_sysconf.h"
#include "posix_funcs.h"
#include "android_cxa.h"
#include "string/android_string.h"
#include "android_errno.h"
#include "android_time.h"
#include "android_epoll.h"
#include "wchar/android_wchar.h"
#include "android_aeabi.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include "inet/android_inet.h"
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#else
#include <strings.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/select.h>
#include <netdb.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/uio.h>
#endif
#include <stdarg.h>
#include "android_strto.h"
#include "linker_format.h"
#include "tzcode/android_strftime.h"
#include "android_sprint.h"
#include "android_cxa_guard.h"
#include "android_operator_new.h"
#include "android_arc4random.h"
#include "android_semaphore.h"

typedef struct {
    char *name;
    void *addr;
} hook_t;


int android_stack_chk_guard_location[4] = {0xDEADCAFE, 0xDAC0FFEE, 0x8BADF00D, 0xBEEFFACE};
void *android_stack_chk_guard = (void *)android_stack_chk_guard_location;

#ifdef _WIN32

int android_mkdir(const char *pathname, int mode) {
    BOOL ret = CreateDirectory(pathname, NULL);
    return ret ? 0 : -1;
}

int android_pipe(int fds[2]) {
    HANDLE handles[2];
    if (!CreatePipe(&handles[0], &handles[1], NULL, 0)) {
        return -1;
    }
    fds[0] = _open_osfhandle((intptr_t)handles[0], _O_RDONLY);
    fds[1] = _open_osfhandle((intptr_t)handles[1], _O_WRONLY);
    if (fds[0] == -1 || fds[1] == -1) {
        CloseHandle(handles[0]);
        CloseHandle(handles[1]);
    }
    return 0;
}

int android_getsockname(int sockfd, struct sockaddr *addr, android_socklen_t *addrlen) {
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } local_addr;
    int len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &len) == 0) {
        local_addr.ipv4.sin_family = af_to_android(local_addr.ipv4.sin_family);
        if (local_addr.ipv4.sin_family == ANDROID_AF_INET) {
            memcpy(addr, &local_addr.ipv4, (*addrlen < sizeof(struct sockaddr_in)) ? *addrlen : sizeof(struct sockaddr_in));
            *addrlen = sizeof(struct sockaddr_in);
        } else if (local_addr.ipv4.sin_family == ANDROID_AF_INET6) {
            memcpy(addr, &local_addr.ipv6, (*addrlen < sizeof(struct sockaddr_in6)) ? *addrlen : sizeof(struct sockaddr_in6));
            *addrlen = sizeof(struct sockaddr_in6);
        } else {
            return -1;
        }
        return 0;
        
    }
    return -1;
}

int android_getpeername(int sockfd, struct sockaddr *addr, android_socklen_t *addrlen) {
    union {
        struct sockaddr_in ipv4;
        struct sockaddr_in6 ipv6;
    } peer_addr;
    int len = sizeof(peer_addr);

    if (addr == NULL || addrlen == NULL) {
        return -1;
    }
    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &len) != 0) {
        return -1;
    }

    peer_addr.ipv4.sin_family = af_to_android(peer_addr.ipv4.sin_family);
    if (peer_addr.ipv4.sin_family == ANDROID_AF_INET) {
        memcpy(addr, &peer_addr.ipv4,
               (*addrlen < sizeof(struct sockaddr_in)) ? *addrlen : sizeof(struct sockaddr_in));
        *addrlen = sizeof(struct sockaddr_in);
    } else if (peer_addr.ipv4.sin_family == ANDROID_AF_INET6) {
        memcpy(addr, &peer_addr.ipv6,
               (*addrlen < sizeof(struct sockaddr_in6)) ? *addrlen : sizeof(struct sockaddr_in6));
        *addrlen = sizeof(struct sockaddr_in6);
    } else {
        return -1;
    }
    return 0;
}

int android_select(int nfds, android_fd_set_t *readfds, android_fd_set_t *writefds, android_fd_set_t *exceptfds, struct timeval *timeout) {
    fd_set native_readfds, native_writefds, native_exceptfds;
    fd_set *native_preadfds = NULL, *native_pwritefds = NULL, *native_pexceptfds = NULL;
    if (readfds) {
        fd_set_to_native(readfds, &native_readfds);
        native_preadfds = &native_readfds;
    }
    if (writefds) {
        fd_set_to_native(writefds, &native_writefds);
        native_pwritefds = &native_writefds;
    }
    if (exceptfds) {
        fd_set_to_native(exceptfds, &native_exceptfds);
        native_pexceptfds = &native_exceptfds;
    }
    return select(nfds, native_preadfds, native_pwritefds, native_pexceptfds, timeout);
}

int android_gethostname(char *name, size_t size) {
    DWORD len = size - 1;
    if (GetComputerName(name, &len)) {
        name[size - 1] = '\0';
        return 0;
    }
    memset(name, 0, size);
    return -1;
}

char *android_inet_ntoa(uint32_t addr) {
    struct in_addr real_addr;
    memcpy(&real_addr, &addr, sizeof(uint32_t));
    return inet_ntoa(real_addr);
}

uint32_t android_inet_addr(char *addr) {
    return inet_addr(addr);
}

void android_div(div_t *ret, int numerator, int denominator) {
    ret->quot = numerator/denominator;
    ret->rem = numerator%denominator;
}
SYSV_WRAPPER(android_div, 3)

static uint64_t seed48[3];

static const uint64_t multiplier = 0x5DEECE66D;
static const uint64_t addend = 0xB;
static const uint64_t mask = ((uint64_t)1 << 48) - 1;

void android_srand48(long seedval) {
    seed48[0] = (seedval ^ multiplier) & mask;
    seed48[1] = 0x330E;
    seed48[2] = 0x0;
}

long android_lrand48(void) {
    seed48[0] = (multiplier * seed48[0] + addend) & mask;
    return (long)(seed48[0] >> 16) & 0x7FFFFFFF;
}

FLOAT_ABI_FIX double android_drand48(void) {
    return (double)android_lrand48() / (1 << 31);
}

long android_syscall(long number, ...) {
    return -1;
}

typedef struct {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
} android_iovec_t;

long android_writev(int fd, const android_iovec_t *iov, int iovcnt) {
    int cnt = 0;
    int i;
    for (i = 0; i < iovcnt; ++i) {
        long len = android_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (len == -1) {
            return -1;
        }
        cnt += len;
    }
    return cnt;
}

typedef struct {
    int fd;
    short events;
    short revents;
} android_pollfd_t;

typedef unsigned int  android_nfds_t;

int android_poll(android_pollfd_t *fds, android_nfds_t nfds, long timeout) {
#define ANDROID_POLLIN 0x0001
#define ANDROID_POLLPRI 0x0002
#define ANDROID_POLLOUT 0x0004
#define ANDROID_POLLERR 0x0008
#define ANDROID_POLLHUP 0x0010
#define ANDROID_POLLNVAL 0x0020
    DWORD start;

    if ((fds == NULL && nfds != 0) || timeout < -1) {
        errno = EINVAL;
        return -1;
    }
    start = GetTickCount();
    for (;;) {
        fd_set read_set;
        fd_set write_set;
        fd_set except_set;
        struct timeval select_timeout;
        int socket_count = 0;
        int ready_count = 0;
        android_nfds_t index;
        DWORD slice_ms = 10;

        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_ZERO(&except_set);
        for (index = 0; index < nfds; ++index) {
            SOCKET socket_fd;
            fds[index].revents = 0;
            if (fds[index].fd < 0 || !is_socket(fds[index].fd)) {
                continue;
            }
            socket_fd = (SOCKET)fds[index].fd;
            if (fds[index].events & (ANDROID_POLLIN | ANDROID_POLLPRI)) {
                FD_SET(socket_fd, &read_set);
            }
            if (fds[index].events & ANDROID_POLLOUT) {
                FD_SET(socket_fd, &write_set);
            }
            FD_SET(socket_fd, &except_set);
            ++socket_count;
        }

        if (timeout >= 0) {
            DWORD elapsed = GetTickCount() - start;
            DWORD remaining = elapsed >= (DWORD)timeout
                                  ? 0
                                  : (DWORD)timeout - elapsed;
            if (slice_ms > remaining) {
                slice_ms = remaining;
            }
        }
        select_timeout.tv_sec = (long)(slice_ms / 1000);
        select_timeout.tv_usec = (long)((slice_ms % 1000) * 1000);
        if (socket_count != 0) {
            if (select(
                    0,
                    &read_set,
                    &write_set,
                    &except_set,
                    &select_timeout) == SOCKET_ERROR) {
                errno = EIO;
                return -1;
            }
        } else if (slice_ms != 0) {
            Sleep(slice_ms);
        }

        for (index = 0; index < nfds; ++index) {
            short revents = 0;
            int fd = fds[index].fd;

            if (fd < 0) {
                continue;
            }
            if (is_socket(fd)) {
                SOCKET socket_fd = (SOCKET)fd;
                if (FD_ISSET(socket_fd, &read_set)) {
                    revents |= fds[index].events &
                               (ANDROID_POLLIN | ANDROID_POLLPRI);
                }
                if (FD_ISSET(socket_fd, &write_set)) {
                    revents |= fds[index].events & ANDROID_POLLOUT;
                }
                if (FD_ISSET(socket_fd, &except_set)) {
                    revents |= ANDROID_POLLERR;
                }
            } else if (android_random_device_is_fd(fd)) {
                revents |= fds[index].events & ANDROID_POLLIN;
            } else {
                intptr_t raw_handle = _get_osfhandle(fd);
                if (raw_handle == -1) {
                    revents |= ANDROID_POLLNVAL;
                } else {
                    HANDLE handle = (HANDLE)raw_handle;
                    DWORD file_type = GetFileType(handle);
                    if (file_type == FILE_TYPE_PIPE) {
                        if (fds[index].events &
                            (ANDROID_POLLIN | ANDROID_POLLPRI)) {
                            DWORD available = 0;
                            if (PeekNamedPipe(
                                    handle,
                                    NULL,
                                    0,
                                    NULL,
                                    &available,
                                    NULL)) {
                                if (available != 0) {
                                    revents |= fds[index].events &
                                               (ANDROID_POLLIN | ANDROID_POLLPRI);
                                }
                            } else if (GetLastError() == ERROR_BROKEN_PIPE) {
                                revents |= ANDROID_POLLHUP;
                            } else {
                                revents |= ANDROID_POLLERR;
                            }
                        }
                        if (fds[index].events & ANDROID_POLLOUT) {
                            revents |= ANDROID_POLLOUT;
                        }
                    } else if (file_type != FILE_TYPE_UNKNOWN ||
                               GetLastError() == NO_ERROR) {
                        revents |= fds[index].events &
                                   (ANDROID_POLLIN | ANDROID_POLLPRI |
                                    ANDROID_POLLOUT);
                    } else {
                        revents |= ANDROID_POLLNVAL;
                    }
                }
            }
            fds[index].revents = revents;
            if (revents != 0) {
                ++ready_count;
            }
        }
        if (ready_count != 0) {
            return ready_count;
        }
        if (timeout == 0 ||
            (timeout > 0 && GetTickCount() - start >= (DWORD)timeout)) {
            return 0;
        }
    }
}

int android_sigaction(int signum, void *act, void *oldact) {
    return -1;
}

int android_sched_yield(void) {
    Sleep(0);
    return 0;
}

int android_fsync(int fd) {
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!FlushFileBuffers(hFile)) {
        return -1;
    }
    return 0;
}

int android_fdatasync(int fd) {
    HANDLE hFile = (HANDLE)_get_osfhandle(fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }
    if (!FlushFileBuffers(hFile)) {
        return -1;
    }
    return 0;
}

int android_geteuid(void) {
    return 0;
}

int android_getuid(void) {
    return 0;
}

int android_getpid(void) {
    return GetCurrentProcessId();
}

int android_remove(const char *pathname) {
    DWORD attr = GetFileAttributes(pathname);
    BOOL ret = FALSE;
    if (attr != -1) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            ret = RemoveDirectory(pathname);
        } else {
            ret = DeleteFile(pathname);
        }
    }
    return ret ? 0 : -1;
}

int android_rename(const char *oldpath, const char *newpath) {
    BOOL ret = MoveFileEx(oldpath, newpath, MOVEFILE_REPLACE_EXISTING);
    return ret ? 0 : -1;
}

int android_unlink(const char *pathname) {
    BOOL ret = DeleteFile(pathname);
    return ret ? 0 : -1;
}

int android_rmdir(const char *pathname) {
    BOOL ret = RemoveDirectory(pathname);
    return ret ? 0 : -1;
}

int android_shutdown(int sockfd, int how) {
    return shutdown(sockfd, how);
}

#else
#define android_mkdir mkdir
#define android_pipe pipe
#define android_select select
#define android_gethostname gethostname
#define android_getsockname getsockname
#define android_getpeername getpeername
#define android_div div
#define android_inet_ntoa inet_ntoa
#define android_inet_addr inet_addr
#define android_lrand48 lrand48
#define android_srand48 srand48
#define android_syscall syscall
#define android_writev writev
#define android_poll poll
#define android_sigaction sigaction
#define android_sched_yield sched_yield
#define android_fsync fsync
#define android_fdatasync fdatasync
#define android_geteuid geteuid
#define android_getuid getuid
#define android_getpid getpid
#define android_remove remove
#define android_rename rename
#define android_unlink unlink
#define android_rmdir rmdir
#define android_shutdown shutdown
#endif
#ifdef _WIN32
#if (_WIN32_WINNT >= 0x0501)
#define HAS_GETADDRINFO
#endif
#else
#define HAS_GETADDRINFO
#endif

char* android_setlocale(int category, char const *locale) {
    return 0;
}

typedef struct {
    char    *h_name;
    char    **h_aliases;
    int     h_addrtype;
    int     h_length;
    char    **h_addr_list;
} android_hostent_t;

static char h_name[256];
static char h_addr_list_storage[35][16];
static char h_aliases_storage[35][256];
static char *h_addr_list[35];
static char *h_aliases[35];

static android_hostent_t android_host = {
    h_name,
    h_aliases,
    ANDROID_AF_INET,
    4,
    h_addr_list
};

typedef struct _android_addrinfo_t {
	int	ai_flags;
	int	ai_family;
	int	ai_socktype;
	int	ai_protocol;
	size_t ai_addrlen;
	char *ai_canonname;
	struct sockaddr *ai_addr;
	struct _android_addrinfo_t *ai_next;
} android_addrinfo_t;

#ifdef HAS_GETADDRINFO
android_hostent_t *android_gethostbyname(const char *name) {
    struct addrinfo *info;
    struct addrinfo hints;
    size_t cnl;

    if (!name) {
        return NULL;
    }

    memset(&hints, 0, sizeof(hints));
    
    hints.ai_flags = AI_CANONNAME;

    cnl = strlen(name) + 1;
    memcpy(h_name, name, cnl <= 256 ? cnl : 256);
    h_name[255] = '\0';
    
    if(getaddrinfo(name, NULL, &hints, &info) == 0) {
        int i;
        struct addrinfo *original_info = info;
        for (i = 0; (i < 34) && (info != NULL); (++i), (info = info->ai_next)) {
            for (;;) {
                if (info == NULL) {
                    break;
                }
                if (info->ai_family == AF_INET) {
                    break;
                }
                info = info->ai_next;
            }
            if (info == NULL) {
                break;
            }

            if (info->ai_canonname) {
                cnl = strlen(info->ai_canonname) + 1;
                memcpy(h_aliases_storage[i], info->ai_canonname, cnl <= 256 ? cnl : 256);
                h_aliases_storage[i][255] = '\0';
            } else {
                h_aliases_storage[i][0] = '\0';
            }
            h_aliases[i] = h_aliases_storage[i];

            memcpy(h_addr_list_storage[i], &((struct sockaddr_in *)info->ai_addr)->sin_addr, sizeof(struct in_addr));
            h_addr_list[i] = h_addr_list_storage[i];
        }
        freeaddrinfo(original_info);
        h_addr_list[i] = NULL;
        h_aliases[i] = NULL;
        return &android_host;
    }
    return NULL;
}

int android_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    struct addrinfo h;
    struct addrinfo *hp = NULL;
    int ret;
    struct addrinfo *addr;
    if (hints) {
        h = *hints;
        h.ai_family = af_to_native(h.ai_family);
        h.ai_socktype = sock_to_native(h.ai_socktype);
        h.ai_protocol = ipproto_to_native(h.ai_protocol);
        hp = &h;
    }

    if (res == NULL) {
        return EAI_FAIL;
    }
    ret = getaddrinfo(node, service, hp, res);

    if (ret != 0 || *res == NULL) {
        return ret;
    }

    for (addr = *res; addr != NULL; addr = addr->ai_next) {
        addr->ai_family = af_to_android(addr->ai_family);
        addr->ai_socktype = sock_to_android(addr->ai_socktype);
        addr->ai_protocol = ipproto_to_android(addr->ai_protocol);
        if (addr->ai_addr != NULL) {
            addr->ai_addr->sa_family = af_to_android(addr->ai_addr->sa_family);
        }
    }
    return ret;
}

void android_freeaddrinfo(struct addrinfo *res) {
    if (res) {
        struct addrinfo *addr;
        for (addr = res; addr != NULL; addr = addr->ai_next) {
            addr->ai_family = af_to_native(addr->ai_family);
            addr->ai_socktype = sock_to_native(addr->ai_socktype);
            addr->ai_protocol = ipproto_to_native(addr->ai_protocol);
            if (addr->ai_addr != NULL) {
                addr->ai_addr->sa_family = af_to_native(addr->ai_addr->sa_family);
            }
        }
        freeaddrinfo(res);
    }
}

int android_getnameinfo(const struct sockaddr *address,
                        android_socklen_t address_length,
                        char *host,
                        android_socklen_t host_length,
                        char *service,
                        android_socklen_t service_length,
                        int flags) {
    struct sockaddr_storage native_address;

    if (address == NULL || address_length > sizeof(native_address)) {
        return EAI_FAIL;
    }
    memset(&native_address, 0, sizeof(native_address));
    memcpy(&native_address, address, address_length);
    ((struct sockaddr *)&native_address)->sa_family =
        af_to_native(((const struct sockaddr *)address)->sa_family);
    return getnameinfo(
        (const struct sockaddr *)&native_address,
        address_length,
        host,
        host_length,
        service,
        service_length,
        flags);
}

#else
int android_getaddrinfo(const char *node, const char *service, const android_addrinfo_t *hints, android_addrinfo_t **res) {
    *res = (android_addrinfo_t *)NULL;
    return -1;
}

void android_freeaddrinfo(android_addrinfo_t *res) {

}

int android_getnameinfo(const struct sockaddr *address,
                        android_socklen_t address_length,
                        char *host,
                        android_socklen_t host_length,
                        char *service,
                        android_socklen_t service_length,
                        int flags) {
    (void)address;
    (void)address_length;
    (void)host;
    (void)host_length;
    (void)service;
    (void)service_length;
    (void)flags;
    return -1;
}

#define android_gethostbyname gethostbyname
#endif

const char *android_gai_strerror(int error_code) {
#ifdef _WIN32
    const char *message = gai_strerrorA(error_code);
    return message ? message : "Unknown address resolution error";
#else
    return gai_strerror(error_code);
#endif
}

int android_setpriority(int which, int who, int prio) {
    return 0;
}

static int android_setenv(
    const char *name,
    const char *value,
    int overwrite) {
    if (!name || !name[0] || strchr(name, '=') || !value) {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite && getenv(name)) {
        return 0;
    }
#ifdef _WIN32
    {
        int result = _putenv_s(name, value);
        if (result != 0) {
            errno = result;
            return -1;
        }
        return 0;
    }
#else
    return setenv(name, value, overwrite);
#endif
}

static int android_unsetenv(const char *name) {
    if (!name || !name[0] || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    {
        int result = _putenv_s(name, "");
        if (result != 0) {
            errno = result;
            return -1;
        }
        return 0;
    }
#else
    return unsetenv(name);
#endif
}

FLOAT_ABI_FIX double android_strtod(const char *nptr, char **endptr) {
    return strtod(nptr, endptr);
}

void android_stack_chk_fail(void) {
    puts("__stack_chk_fail");
}

size_t android_strlen_chk(const char *s, size_t s_len) {
    size_t ret = strlen(s);
    if (ret >= s_len) {
        abort();
    }
    return ret;
}

FLOAT_ABI_FIX int android_isinff(float x) {
    union {
        float f;
        unsigned int u;
    } value;
    unsigned int exp, frac;

    value.f = x;
    exp  = (value.u >> 23) & 0xFF;
    frac = value.u & 0x7FFFFF;

    return (exp == 0xFF) && (frac == 0);
}

FLOAT_ABI_FIX int android_isfinitef(float x) {
    union {
        float f;
        unsigned int u;
    } value;
    unsigned int exp;

    value.f = x;
    exp = (value.u >> 23) & 0xFF;

    return exp != 0xFF;
}
 
FLOAT_ABI_FIX int android_isfinite(double x) {
    union {
        double d;
        uint64_t u;
    } value;
    unsigned int exp;

    value.d = x;
    exp = (unsigned int)((value.u >> 52) & 0x7FF);

    return exp != 0x7FF;
}

FLOAT_ABI_FIX int android_isnanf(float x) {
    union {
        float f;
        unsigned int u;
    } value;
    unsigned int exp, frac;

    value.f = x;
    exp  = (value.u >> 23) & 0xFF;
    frac = value.u & 0x7FFFFF;

    return (exp == 0xFF) && (frac != 0);
}

FLOAT_ABI_FIX double android_nearbyint(double x) {
    double intpart;
    double fracpart = modf(x, &intpart);

    if (fabs(fracpart) < 0.5) {
        return intpart;
    } else if (fabs(fracpart) > 0.5) {
        return x > 0.0 ? ceil(x) : floor(x);
    } else {
        if (fmod(intpart, 2.0) == 0.0)
            return intpart;
        else
            return x > 0.0 ? intpart + 1.0 : intpart - 1.0;
    }
}

void android_assert2(const char *__file, int __line, const char *__function, const char *__msg) {
    printf("Assertion failed: %s\nFile: %s\nLine: %d\nFunction: %s\n", __msg, __file, __line, __function);
    abort();
}

static hook_t aeabi_hooks[] = {
#if !defined(_MSC_VER) && defined(ANDROID_ARM_LINKER)
    {
        "__aeabi_atexit",
        (void *)android_aeabi_atexit
    },
    {
        "__aeabi_memcpy8",
        (void *)android_aeabi_memcpy8
    },
    {
        "__aeabi_memcpy4",
        (void *)android_aeabi_memcpy4
    },
    {
        "__aeabi_memcpy",
        (void *)android_aeabi_memcpy
    },
    {
        "__aeabi_memmove8",
        (void *)android_aeabi_memmove8
    },
    {
        "__aeabi_memmove4",
        (void *)android_aeabi_memmove4
    },
    {
        "__aeabi_memmove",
        (void *)android_aeabi_memmove
    },
    {
        "__aeabi_memset8",
        (void *)android_aeabi_memset8
    },
    {
        "__aeabi_memset4",
        (void *)android_aeabi_memset4
    },
    {
        "__aeabi_memset",
        (void *)android_aeabi_memset
    },
    {
        "__aeabi_memclr8",
        (void *)android_aeabi_memclr8
    },
    {
        "__aeabi_memclr4",
        (void *)android_aeabi_memclr4
    },
    {
        "__aeabi_memclr",
        (void *)android_aeabi_memclr
    },
    {
        "__aeabi_ul2f",
        (void *)android_aeabi_ul2f
    },
    {
        "__aeabi_ul2d",
        (void *)android_aeabi_ul2d
    },
    {
        "__aeabi_d2ulz",
        (void *)android_aeabi_d2ulz
    },
    {
        "__aeabi_idiv",
        (void *)android_aeabi_idiv
    },
    {
        "__aeabi_uidiv",
        (void *)android_aeabi_uidiv
    },
    {
        "__aeabi_ldiv",
        (void *)android_aeabi_ldiv
    },
    {
        "__aeabi_uldiv",
        (void *)android_aeabi_uldiv
    },
    {
        "__aeabi_idivmod",
        (void *)android_aeabi_idivmod
    },
    {
        "__aeabi_uidivmod",
        (void *)android_aeabi_uidivmod
    },
    {
        "__aeabi_ldivmod",
        (void *)android_aeabi_ldivmod
    },
    {
        "__aeabi_uldivmod",
        (void *)android_aeabi_uldivmod
    },
#endif
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t semaphore_hooks[] = {
    {
        "sem_init",
        (void *)android_sem_init
    },
    {
        "sem_destroy",
        (void *)android_sem_destroy
    },
    {
        "sem_open",
        (void *)android_sem_open
    },
    {
        "sem_close",
        (void *)android_sem_close
    },
    {
        "sem_unlink",
        (void *)android_sem_unlink
    },
    {
        "sem_wait",
        (void *)android_sem_wait
    },
    {
        "sem_timedwait",
        (void *)android_sem_timedwait
    },
    {
        "sem_post",
        (void *)android_sem_post
    },
    {
        "sem_trywait",
        (void *)android_sem_trywait
    },
    {
        "sem_getvalue",
        (void *)android_sem_getvalue
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t pthread_hooks[] = {
    {
        "__pthread_cleanup_push",
        (void *)android_pthread_cleanup_push
    },
    {
        "__pthread_cleanup_pop",
        (void *)android_pthread_cleanup_pop
    },
    {
        "pthread_attr_init",
        (void *)android_pthread_attr_init
    },
    {
        "pthread_attr_destroy",
        (void *)android_pthread_attr_destroy
    },
    {
        "pthread_attr_setdetachstate",
        (void *)android_pthread_attr_setdetachstate
    },
    {
        "pthread_attr_getdetachstate",
        (void *)android_pthread_attr_getdetachstate
    },
    {
        "pthread_attr_setschedparam",
        (void *)android_pthread_attr_setschedparam
    },
    {
        "pthread_attr_setstacksize",
        (void *)android_pthread_attr_setstacksize
    },
    {
        "pthread_cond_broadcast",
        (void *)android_pthread_cond_broadcast
    },
    {
        "pthread_cond_destroy",
        (void *)android_pthread_cond_destroy
    },
    {
        "pthread_cond_init",
        (void *)android_pthread_cond_init
    },
    {
        "pthread_mutex_trylock",
        (void *)android_pthread_mutex_trylock
    },
    {
        "pthread_cond_timedwait",
        (void *)android_pthread_cond_timedwait
    },
    {
        "pthread_cond_wait",
        (void *)android_pthread_cond_wait
    },
    {
        "pthread_create",
        (void *)android_pthread_create
    },
    {
        "pthread_getspecific",
        (void *)android_pthread_getspecific
    },
    {
        "pthread_join",
        (void *)android_pthread_join
    },
    {
        "pthread_key_create",
        (void *)android_pthread_key_create
    },
    {
        "pthread_mutexattr_destroy",
        (void *)android_pthread_mutexattr_destroy
    },
    {
        "pthread_mutexattr_init",
        (void *)android_pthread_mutexattr_init
    },
    {
        "pthread_mutex_destroy",
        (void *)android_pthread_mutex_destroy
    },
    {
        "pthread_mutex_init",
        (void *)android_pthread_mutex_init
    },
    {
        "pthread_mutex_lock",
        (void *)android_pthread_mutex_lock
    },
    {
        "pthread_mutex_unlock",
        (void *)android_pthread_mutex_unlock
    },
    {
        "pthread_self",
        (void *)android_pthread_self
    },
    {
        "pthread_setspecific",
        (void *)android_pthread_setspecific
    },
    {
        "pthread_key_delete",
        (void *)android_pthread_key_delete
    },
    {
        "pthread_once",
        (void *)android_pthread_once
    },
    {
        "pthread_equal",
        (void *)android_pthread_equal
    },
    {
        "pthread_cond_signal",
        (void *)android_pthread_cond_signal
    },
    {
        "pthread_detach",
        (void *)android_pthread_detach
    },
    {
        "pthread_setname_np",
        (void *)android_pthread_setname_np
    },
    {
        "pthread_mutexattr_settype",
        (void *)android_pthread_mutexattr_settype
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t math_hooks[] = {
    {
        "atan2f",
        (void *)android_atan2f
    },
    {
        "ceilf",
        (void *)android_ceilf
    },
    {
        "cosf",
        (void *)android_cosf
    },
    {
        "floorf",
        (void *)android_floorf
    },
    {
        "fmodf",
        (void *)android_fmodf
    },
    {
        "logf",
        (void *)android_logf
    },
    {
        "powf",
        (void *)android_powf
    },
    {
        "sinf",
        (void *)android_sinf
    },
    {
        "sqrtf",
        (void *)android_sqrtf
    },
    {
        "atanf",
        (void *)android_atanf
    },
    {
        "fmod",
        (void *)android_fmod
    },
    {
        "floor",
        (void *)android_floor
    },
    {
        "ceil",
        (void *)android_ceil
    },
    {
        "sin",
        (void *)android_sin
    },
    {
        "cos",
        (void *)android_cos
    },
    {
        "sqrt",
        (void *)android_sqrt
    },
    {
        "pow",
        (void *)android_pow
    },
    {
        "atan2",
        (void *)android_atan2
    },
    {
        "atan",
        (void *)android_atan
    },
    {
        "modff",
        (void *)android_modff
    },
    {
        "modf",
        (void *)android_modf
    },
    {
        "ldexp",
        (void *)android_ldexp
    },
    {
        "ldexpf",
        (void *)android_ldexpf
    },
    {
        "tanf",
        (void *)android_tanf
    },
    {
        "cosh",
        (void *)android_cosh
    },
    {
        "tan",
        (void *)android_tan
    },
    {
        "tanh",
        (void *)android_tanh
    },
    {
        "asin",
        (void *)android_asin
    },
    {
        "log",
        (void *)android_log
    },
    {
        "sinh",
        (void *)android_sinh
    },
    {
        "acos",
        (void *)android_acos
    },
    {
        "exp",
        (void *)android_exp
    },
    {
        "frexp",
        (void *)android_frexp
    },
    {
        "log10",
        (void *)android_log10
    },
    {
        "log10f",
        (void *)android_log10f
    },
    {
        "acosf",
        (void *)android_acosf
    },
    {
        "expf",
        (void *)android_expf
    },
    {
        "asinf",
        (void *)android_asinf
    },
    {
        "exp2f",
        (void *)android_exp2f
    },
    {
        "exp2",
        (void *)android_exp2
    },
    {
        "fmaxf",
        (void *)android_fmaxf
    },
    {
        "nearbyintf",
        (void *)android_nearbyintf
    },
    {
        "roundf",
        (void *)android_roundf
    },
    {
        "truncf",
        (void *)android_truncf
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t string_hooks[] = {
    {
        "bcopy",
        (void *)android_bcopy
    },
    {
        "index",
        (void *)android_index
    },
    {
        "memccpy",
        (void *)android_memccpy
    },
    {
        "memchr",
        (void *)android_memchr
    },
    {
        "memcmp",
        (void *)android_memcmp
    },
    {
        "memcpy",
        (void *)android_memcpy
    },
    {
        "memmem",
        (void *)android_memmem
    },
    {
        "memmove",
        (void *)android_memmove
    },
    {
        "memrchr",
        (void *)android_memrchr
    },
    {
        "memset",
        (void *)android_memset
    },
    {
        "memswap",
        (void *)android_memswap
    },
    {
        "strcasecmp",
        (void *)android_strcasecmp
    },
    {
        "strncasecmp",
        (void *)android_strncasecmp
    },
    {
        "strcasestr",
        (void *)android_strcasestr
    },
    {
        "strcat",
        (void *)android_strcat
    },
    {
        "strchr",
        (void *)android_strchr
    },
    {
        "strcmp",
        (void *)android_strcmp
    },
    {
        "strcoll",
        (void *)android_strcoll
    },
    {
        "strcpy",
        (void *)android_strcpy
    },
    {
        "strcspn",
        (void *)android_strcspn
    },
    {
        "strdup",
        (void *)android_strdup
    },
    {
        "strlcat",
        (void *)android_strlcat
    },
    {
        "strlcpy",
        (void *)android_strlcpy
    },
    {
        "strlen",
        (void *)android_strlen
    },
    {
        "strncat",
        (void *)android_strncat
    },
    {
        "strncmp",
        (void *)android_strncmp
    },
    {
        "strncpy",
        (void *)android_strncpy
    },
    {
        "strndup",
        (void *)android_strndup
    },
    {
        "strnlen",
        (void *)android_strnlen
    },
    {
        "strpbrk",
        (void *)android_strpbrk
    },
    {
        "strrchr",
        (void *)android_strrchr
    },
    {
        "strsep",
        (void *)android_strsep
    },
    {
        "strspn",
        (void *)android_strspn
    },
    {
        "strstr",
        (void *)android_strstr
    },
    {
        "strtok",
        (void *)android_strtok
    },
    {
        "strtotimeval",
        (void *)android_strtotimeval
    },
    {
        "strxfrm",
        (void *)android_strxfrm
    },
    {
        "strerror",
        (void *)android_strerror
    },
    {
        "strerror_r",
        (void *)android_strerror_r
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t wchar_hooks[] = {
    {
        "wcschr",
        (void *)android_wcschr
    },
    {
        "wcsdup",
        (void *)android_wcsdup
    },
    {
        "wcsncat",
        (void *)android_wcsncat
    },
    {
        "wcsrchr",
        (void *)android_wcsrchr
    },
    {
        "wmemchr",
        (void *)android_wmemchr
    },
    {
        "wcpcpy",
        (void *)android_wcpcpy
    },
    {
        "wcscmp",
        (void *)android_wcscmp
    },
    {
        "wcslcat",
        (void *)android_wcslcat
    },
    {
        "wcsncmp",
        (void *)android_wcsncmp
    },
    {
        "wcsspn",
        (void *)android_wcsspn
    },
    {
        "wmemcmp",
        (void *)android_wmemcmp
    },
    {
        "wcpncpy",
        (void *)android_wcpncpy
    },
    {
        "wcscoll",
        (void *)android_wcscoll
    },
    {
        "wcslcpy",
        (void *)android_wcslcpy
    },
    {
        "wcsncpy",
        (void *)android_wcsncpy
    },
    {
        "wcsstr",
        (void *)android_wcsstr
    },
    {
        "wmemcpy",
        (void *)android_wmemcpy
    },
    {
        "wcscasecmp",
        (void *)android_wcscasecmp
    },
    {
        "wcscpy",
        (void *)android_wcscpy
    },
    {
        "wcslen",
        (void *)android_wcslen
    },
    {
        "wcsnlen",
        (void *)android_wcsnlen
    },
    {
        "wcstok",
        (void *)android_wcstok
    },
    {
        "wmemmove",
        (void *)android_wmemmove
    },
    {
        "wcscat",
        (void *)android_wcscat
    },
    {
        "wcscspn",
        (void *)android_wcscspn
    },
    {
        "wcsncasecmp",
        (void *)android_wcsncasecmp
    },
    {
        "wcspbrk",
        (void *)android_wcspbrk
    },
    {
        "wcsxfrm",
        (void *)android_wcsxfrm
    },
    {
        "wmemset",
        (void *)android_strerror_r
    },
    {
        "iswalnum",
        (void *)android_iswalnum
    },
    {
        "iswalpha",
        (void *)android_iswalpha
    },
    {
        "iswcntrl",
        (void *)android_iswcntrl
    },
    {
        "iswdigit",
        (void *)android_iswdigit
    },
    {
        "iswgraph",
        (void *)android_iswgraph
    },
    {
        "iswlower",
        (void *)android_iswlower
    },
    {
        "iswprint",
        (void *)android_iswprint
    },
    {
        "iswpunct",
        (void *)android_iswpunct
    },
    {
        "iswspace",
        (void *)android_iswspace
    },
    {
        "iswupper",
        (void *)android_iswupper
    },
    {
        "iswxdigit",
        (void *)android_iswxdigit
    },
    {
        "iswctype",
        (void *)android_iswctype
    },
    {
        "wctype",
        (void *)android_wctype
    },
    {
        "wcwidth",
        (void *)android_wcwidth
    },
    {
        "towlower",
        (void *)android_towlower
    },
    {
        "towupper",
        (void *)android_towupper
    },
    {
        "wctob",
        (void *)android_wctob
    },
    {
        "btowc",
        (void *)android_btowc
    },
    {
        "wcrtomb",
        (void *)android_wcrtomb
    },
    {
        "mbrtowc",
        (void *)android_mbrtowc
    },
    {
        "wcsftime",
        (void *)android_wcsftime
    },
    {
        "wcsrtombs",
        (void *)android_wcsrtombs
    },
    {
        "wcstombs",
        (void *)android_wcstombs
    },
    {
        "mbsrtowcs",
        (void *)android_mbsrtowcs
    },
    {
        "mbstowcs",
        (void *)android_mbstowcs
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t ctype_hooks[] = {
    {
        "_ctype_",
        (void *)&android_ctype
    },
    {
        "_tolower_tab_",
        (void *)&android_tolower_tab
    },
    {
        "_toupper_tab_",
        (void *)&android_toupper_tab
    },
    {
        "tolower",
        (void *)android_tolower
    },
    {
        "toupper",
        (void *)android_toupper
    },
    {
        "isalnum",
        (void *)android_isalnum
    },
    {
        "isalpha",
        (void *)android_isalpha
    },
    {
        "iscntrl",
        (void *)android_iscntrl
    },
    {
        "isdigit",
        (void *)android_isdigit
    },
    {
        "isgraph",
        (void *)android_isgraph
    },
    {
        "islower",
        (void *)android_islower
    },
    {
        "isprint",
        (void *)android_isprint
    },
    {
        "ispunct",
        (void *)android_ispunct
    },
    {
        "isspace",
        (void *)android_isspace
    },
    {
        "isupper",
        (void *)android_isupper
    },
    {
        "isxdigit",
        (void *)android_isxdigit
    },
    {
        "isblank",
        (void *)android_isblank
    },
    {
        "isascii",
        (void *)android_isascii
    },
    {
        "toascii",
        (void *)android_toascii
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t hooks[] = {
    {
        "__cxa_finalize",
        (void *)android_cxa_finalize
    },
    {
        "__cxa_atexit",
        (void *)android_cxa_atexit
    },
    {
        "__cxa_pure_virtual",
        (void *)android_cxa_pure_virtual
    },
    {
        "__stack_chk_fail",
        (void *)android_stack_chk_fail
    },
    {
        "__stack_chk_guard",
        (void *)&android_stack_chk_guard
    },
    {
        "__strlen_chk",
        (void *)android_strlen_chk
    },
    {
        "__isnanf",
        (void *)android_isnanf
    },
    {
        "__isinff",
        (void *)android_isinff
    },
    {
        "__isfinitef",
        (void *)android_isfinitef
    },
    {
        "__isfinite",
        (void *)android_isfinite
    },
    {
        "div",
#ifdef _WIN32
        (void *)GET_SYSV_WRAPPER(android_div)
#else
        (void *)android_div
#endif
    },
    {
        "nearbyint",
        (void *)android_nearbyint
    },
    {
        "__errno",
        (void *)android_errno
    },
    {
        "fflush",
        (void *)android_fflush
    },
    {
        "fseek",
        (void *)android_fseek
    },
    {
        "fgetpos",
        (void *)android_fgetpos
    },
    {
        "fputs",
        (void *)android_fputs
    },
    {
        "inet_ntoa",
        (void *)android_inet_ntoa
    },
    {
        "inet_addr",
        (void *)android_inet_addr
    },
    {
        "fsetpos",
        (void *)android_fsetpos
    },
    {
        "ftell",
        (void *)android_ftell
    },
    {
        "fwrite",
        (void *)android_fwrite
    },
    {
        "fread",
        (void *)android_fread
    },
    {
        "ungetc",
        (void *)android_ungetc
    },
    {
        "fopen",
        (void *)android_fopen
    },
    {
        "fclose",
        (void *)android_fclose
    },
    {
        "putc",
        (void *)android_putc
    },
    {
        "setvbuf",
        (void *)android_setvbuf
    },
    {
        "rename",
        (void *)android_rename
    },
    {
        "__sF",
        (void *)&android_sf
    },
    {
        "gettimeofday",
        (void *)android_gettimeofday
    },
    {
        "fstat",
        (void *)android_fstat
    },
    {
        "closedir",
        (void *)android_closedir
    },
    {
        "opendir",
        (void *)android_opendir
    },
    {
        "readdir",
        (void *)android_readdir
    },
    {
        "remove",
        (void *)android_remove
    },
    {
        "mkdir",
        (void *)android_mkdir
    },
    {
        "pipe",
        (void *)android_pipe
    },
    {
        "sysconf",
        (void *)android_sysconf
    },
    {
        "usleep",
        (void *)android_usleep
    },
    {
        "gethostname",
        (void *)android_gethostname
    },
    {
        "listen",
        (void *)android_listen
    },
    {
        "send",
        (void *)android_send
    },
    {
        "sendto",
        (void *)android_sendto
    },
    {
        "recv",
        (void *)android_recv
    },
    {
        "recvfrom",
        (void *)android_recvfrom
    },
    {
        "recvmsg",
        (void *)android_recvmsg
    },
    {
        "sendmsg",
        (void *)android_sendmsg
    },
    {
        "getsockopt",
        (void *)android_getsockopt
    },
    {
        "setsockopt",
        (void *)android_setsockopt
    },
    {
        "socket",
        (void *)android_socket
    },
    {
        "accept",
        (void *)android_accept
    },
    {
        "bind",
        (void *)android_bind
    },
    {
        "connect",
        (void *)android_connect
    },
    {
        "gethostbyname",
        (void *)android_gethostbyname
    },
    {
        "getsockname",
        (void *)android_getsockname
    },
    {
        "getpeername",
        (void *)android_getpeername
    },
    {
        "shutdown",
        (void *)android_shutdown
    },
    {
        "select",
        (void *)android_select
    },
    {
        "fcntl",
        (void *)android_fcntl
    },
    {
        "ioctl",
        (void *)android_ioctl
    },
    {
        "mmap",
        (void *)android_mmap
    },
    {
        "munmap",
        (void *)android_munmap
    },
    {
        "mprotect",
        (void *)android_mprotect
    },
    {
        "close",
        (void *)android_close
    },
    {
        "read",
        (void *)android_read
    },
    {
        "write",
        (void *)android_write
    },
    {
        "open",
        (void *)android_open
    },
    {
        "getc",
        (void *)android_getc
    },
    {
        "__assert2",
        (void *)android_assert2
    },
    {
        "fprintf",
        (void *)android_fprintf
    },
    {
        "vfprintf",
        (void *)android_vfprintf
    },
    {
        "fscanf",
        (void *)android_fscanf
    },
    {
        "fgets",
        (void *)android_fgets
    },
    {
        "lrand48",
        (void *)android_lrand48
    },
    {
        "srand48",
        (void *)android_srand48
    },
    {
        "getpid",
        (void *)android_getpid
    },
    {
        "getuid",
        (void *)android_getuid
    },
    {
        "nanosleep",
        (void *)android_nanosleep
    },
    {
        "syscall",
        (void *)android_syscall
    },
    {
        "strtod",
        (void *)android_strtod
    },
    {
        "fputc",
        (void *)android_fputc
    },
    {
        "putwc",
        (void *)android_putwc
    },
    {
        "ungetwc",
        (void *)android_ungetwc
    },
    {
        "getwc",
        (void *)android_getwc
    },
    {
        "fdopen",
        (void *)android_fdopen
    },
    {
        "writev",
        (void *)android_writev
    },
    {
        "poll",
        (void *)android_poll
    },
    {
        "epoll_create",
        (void *)android_epoll_create
    },
    {
        "epoll_ctl",
        (void *)android_epoll_ctl
    },
    {
        "epoll_wait",
        (void *)android_epoll_wait
    },
    {
        "ferror",
        (void *)android_ferror
    },
    {
        "feof",
        (void *)android_feof
    },
    {
        "stat",
        (void *)android_stat
    },
    {
        "sigaction",
        (void *)android_sigaction
    },
    {
        "sched_yield",
        (void *)android_sched_yield
    },
    {
        "localtime_r",
        (void *)android_localtime_r
    },
    {
        "localtime",
        (void *)android_localtime
    },
    {
        "fnmatch",
        (void *)android_fnmatch
    },
    {
        "fsync",
        (void *)android_fsync
    },
    {
        "fdatasync",
        (void *)android_fdatasync
    },
    {
        "unlink",
        (void *)android_unlink
    },
    {
        "geteuid",
        (void *)android_geteuid
    },
    {
        "vsnprintf",
        (void *)android_vsnprintf
    },
    {
        "snprintf",
        (void *)android_snprintf
    },
    {
        "vsprintf",
        (void *)android_vsprintf
    },
    {
        "sprintf",
        (void *)android_sprintf
    },
    {
        "clock_gettime",
        (void *)android_clock_gettime
    },
    {
        "ftime",
        (void *)android_ftime
    },
    {
        "time",
        (void *)android_time
    },
    {
        "ctime",
        (void *)android_ctime
    },
    {
        "clock",
        (void *)android_clock
    },
    {
        "sleep",
        (void *)android_sleep
    },
    {
        "gmtime",
        (void *)android_gmtime
    },
    {
        "gmtime_r",
        (void *)android_gmtime_r
    },
    {
        "mktime",
        (void *)android_mktime
    },
    {
        "strtoull",
        (void *)android_strtoull
    },
    {
        "setlocale",
        (void *)android_setlocale
    },
    {
        "strftime",
        (void *)android_strftime
    },
    {
        "strptime",
        (void *)android_strptime
    },
    {
        "rmdir",
        (void *)android_rmdir
    },
    {
        "__dso_handle",
        (void *)&android_dso_handle
    },
    {
        "__cxa_guard_acquire",
        (void *)android_cxa_guard_acquire
    },
    {
        "__cxa_guard_release",
        (void *)android_cxa_guard_release
    },
    {
        "__cxa_guard_abort",
        (void *)android_cxa_guard_abort
    },
    {
        "_Znwj",
        (void *)android_operator_new
    },
    {
        "_Znaj",
        (void *)android_operator_new_arr
    },
    {
        "_ZdlPv",
        (void *)android_operator_delete
    },
    {
        "_ZdaPv",
        (void *)android_operator_delete_arr
    },
    {
        "_ZnwjRKSt9nothrow_t",
        (void *)android_operator_new_nothrow
    },
    {
        "_ZnajRKSt9nothrow_t",
        (void *)android_operator_new_arr_nothrow
    },
    {
        "_ZdlPvRKSt9nothrow_t",
        (void *)android_operator_delete_nothrow
    },
    {
        "_ZdaPvRKSt9nothrow_t",
        (void *)android_operator_delete_arr_nothrow
    },
    {
        "__arc4_getbyte",
        (void *)android_arc4_getbyte
    },
    {
        "arc4random_stir",
        (void *)android_arc4random_stir
    },
    {
        "arc4random_addrandom",
        (void *)android_arc4random_addrandom
    },
    {
        "arc4random",
        (void *)android_arc4random
    },
    {
        "arc4random_buf",
        (void *)android_arc4random_buf
    },
    {
        "arc4random_uniform",
        (void *)android_arc4random_uniform
    },
    {
        "getaddrinfo",
        (void *)android_getaddrinfo
    },
    {
        "gai_strerror",
        (void *)android_gai_strerror
    },
    {
        "getnameinfo",
        (void *)android_getnameinfo
    },
    {
        "freeaddrinfo",
        (void *)android_freeaddrinfo
    },
    {
        "inet_ntop",
        (void *)android_inet_ntop
    },
    {
        "inet_pton",
        (void *)android_inet_pton
    },
    {
        "if_nametoindex",
        (void *)android_if_nametoindex
    },
    {
        "if_indextoname",
        (void *)android_if_indextoname
    },
    {
        "setpriority",
        (void *)android_setpriority
    },
    {
        "pread",
        (void *)pread
    },
    {
        "strtol",
        (void *)strtol
    },
    {
        "strtoul",
        (void *)strtoul
    },
    {
        "calloc",
        (void *)calloc
    },
    {
        "malloc",
        (void *)malloc
    },
    {
        "realloc",
        (void *)realloc
    },
    {
        "free",
        (void *)free
    },
    {
        "printf",
        (void *)printf
    },
    {
        "sscanf",
        (void *)sscanf
    },
    {
        "puts",
        (void *)puts
    },
    {
        "putchar",
        (void *)putchar
    },
    {
        "bsd_signal",
        (void *)signal
    },
    {
        "exit",
        (void *)exit
    },
    {
        "abort",
        (void *)abort
    },
    {
        "access",
        (void *)access
    },
    {
        "atoi",
        (void *)atoi
    },
    {
        "atol",
        (void *)atol
    },
    {
        "raise",
        (void *)raise
    },
    {
        "perror",
        (void *)perror
    },
    {
        "getenv",
        (void *)getenv
    },
    {
        "setenv",
        (void *)android_setenv
    },
    {
        "unsetenv",
        (void *)android_unsetenv
    },
    {
        "lseek",
        (void *)lseek
    },
    {
        "qsort",
        (void *)qsort
    },
    {
        (char *)NULL,
        (void *)NULL
    }
};

static hook_t *custom_hooks = NULL;
static int custom_hooks_size = 0;
static int custom_hooks_arr_size = 0;

static void custom_hooks_resize(void) {
    if (custom_hooks_arr_size == 0) {
        custom_hooks_arr_size = 512;
        custom_hooks = (hook_t *) malloc(custom_hooks_arr_size * sizeof(hook_t));
    } else {
        hook_t *new_array;

        custom_hooks_arr_size *= 2;
        new_array = (hook_t *) malloc(custom_hooks_arr_size * sizeof(hook_t));
        memcpy(&new_array[0], &custom_hooks[0], custom_hooks_size * sizeof(hook_t));
        free(custom_hooks);
        custom_hooks = new_array;
    }
}

void add_custom_hook(char *name, void *addr) {
    int i;

    if (custom_hooks_size + 1 >= custom_hooks_arr_size) {
        custom_hooks_resize();
    }
    for (i = 0; i < custom_hooks_size; ++i) {
        if (strcmp(custom_hooks[i].name, name) == 0) {
            custom_hooks[i].name = name;
            custom_hooks[i].addr = addr;
            return;
        }
    }
    custom_hooks[custom_hooks_size].name = name;
    custom_hooks[custom_hooks_size++].addr = addr;
}

#define ANCMP_HOOKS_CHECK_AND_RETURN(x, name, hook) \
    for (hook = &x[0]; hook->name != NULL; ++hook) { \
        if (strcmp(name, hook->name) == 0) { \
            return hook->addr; \
        } \
    }

void *get_hooked_symbol(char *name) {
    int i;
    hook_t *hook;

    for (i = 0; i < custom_hooks_size; ++i) {
        if (strcmp(name, custom_hooks[i].name) == 0) {
            return custom_hooks[i].addr;
        }
    }
    ANCMP_HOOKS_CHECK_AND_RETURN(hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(string_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(wchar_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(ctype_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(pthread_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(math_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(aeabi_hooks, name, hook);
    ANCMP_HOOKS_CHECK_AND_RETURN(semaphore_hooks, name, hook);
    return NULL;
}
