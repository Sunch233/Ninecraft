#include "android_epoll.h"

#ifdef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define ANDROID_EPOLL_MAX_INSTANCES 8
#define ANDROID_EPOLL_MAX_WATCHES 256
#define ANDROID_EPOLL_SELECT_SLICE_MS 10

typedef struct android_epoll_watch {
    int fd;
    uint32_t events;
    uint64_t data;
} android_epoll_watch_t;

typedef struct android_epoll_instance {
    int in_use;
    int fd;
    int watch_count;
    android_epoll_watch_t watches[ANDROID_EPOLL_MAX_WATCHES];
} android_epoll_instance_t;

static android_epoll_instance_t
    android_epoll_instances[ANDROID_EPOLL_MAX_INSTANCES];
static CRITICAL_SECTION android_epoll_lock;
static volatile LONG android_epoll_lock_state;

static void android_epoll_initialize(void) {
    if (InterlockedCompareExchange(&android_epoll_lock_state, 1, 0) == 0) {
        InitializeCriticalSection(&android_epoll_lock);
        InterlockedExchange(&android_epoll_lock_state, 2);
        return;
    }
    while (InterlockedCompareExchange(&android_epoll_lock_state, 2, 2) != 2) {
        Sleep(0);
    }
}

static android_epoll_instance_t *android_epoll_find_instance(int fd) {
    int i;
    for (i = 0; i < ANDROID_EPOLL_MAX_INSTANCES; ++i) {
        if (android_epoll_instances[i].in_use &&
            android_epoll_instances[i].fd == fd) {
            return &android_epoll_instances[i];
        }
    }
    return NULL;
}

static int android_epoll_find_watch(
    const android_epoll_instance_t *instance,
    int fd) {
    int i;
    for (i = 0; i < instance->watch_count; ++i) {
        if (instance->watches[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

static int android_epoll_is_socket(int fd) {
    int socket_type = 0;
    int size = sizeof(socket_type);
    return getsockopt(
               (SOCKET)fd,
               SOL_SOCKET,
               SO_TYPE,
               (char *)&socket_type,
               &size) == 0;
}

static uint32_t android_epoll_handle_events(
    const android_epoll_watch_t *watch) {
    intptr_t raw_handle = _get_osfhandle(watch->fd);
    HANDLE handle;
    DWORD file_type;
    uint32_t ready = 0;

    if (raw_handle == -1) {
        return ANDROID_EPOLLERR;
    }
    handle = (HANDLE)raw_handle;
    file_type = GetFileType(handle);
    if (file_type == FILE_TYPE_PIPE) {
        if (watch->events & (ANDROID_EPOLLIN | ANDROID_EPOLLPRI)) {
            DWORD available = 0;
            if (PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
                if (available > 0) {
                    ready |= watch->events &
                             (ANDROID_EPOLLIN | ANDROID_EPOLLPRI);
                }
            } else if (GetLastError() == ERROR_BROKEN_PIPE) {
                ready |= ANDROID_EPOLLHUP;
            }
        }
        if (watch->events & ANDROID_EPOLLOUT) {
            ready |= ANDROID_EPOLLOUT;
        }
        return ready;
    }
    if (file_type != FILE_TYPE_UNKNOWN) {
        ready |= watch->events &
                 (ANDROID_EPOLLIN | ANDROID_EPOLLPRI | ANDROID_EPOLLOUT);
        return ready;
    }
    return ANDROID_EPOLLERR;
}

int android_epoll_create(int size) {
    HANDLE handle;
    int fd;
    int i;

    if (size <= 0) {
        errno = EINVAL;
        return -1;
    }
    handle = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!handle) {
        errno = EMFILE;
        return -1;
    }
    fd = _open_osfhandle((intptr_t)handle, _O_RDONLY);
    if (fd < 0) {
        CloseHandle(handle);
        return -1;
    }

    android_epoll_initialize();
    EnterCriticalSection(&android_epoll_lock);
    for (i = 0; i < ANDROID_EPOLL_MAX_INSTANCES; ++i) {
        if (!android_epoll_instances[i].in_use) {
            memset(&android_epoll_instances[i], 0, sizeof(android_epoll_instances[i]));
            android_epoll_instances[i].in_use = 1;
            android_epoll_instances[i].fd = fd;
            LeaveCriticalSection(&android_epoll_lock);
            return fd;
        }
    }
    LeaveCriticalSection(&android_epoll_lock);
    _close(fd);
    errno = EMFILE;
    return -1;
}

int android_epoll_ctl(
    int epfd,
    int operation,
    int fd,
    android_epoll_event_t *event) {
    android_epoll_instance_t *instance;
    int index;

    android_epoll_initialize();
    EnterCriticalSection(&android_epoll_lock);
    instance = android_epoll_find_instance(epfd);
    if (!instance) {
        LeaveCriticalSection(&android_epoll_lock);
        errno = EBADF;
        return -1;
    }
    index = android_epoll_find_watch(instance, fd);
    if (operation == ANDROID_EPOLL_CTL_ADD) {
        if (!event) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = EFAULT;
            return -1;
        }
        if (index >= 0) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = EEXIST;
            return -1;
        }
        if (instance->watch_count >= ANDROID_EPOLL_MAX_WATCHES) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = ENOSPC;
            return -1;
        }
        index = instance->watch_count++;
        instance->watches[index].fd = fd;
        instance->watches[index].events = event->events;
        instance->watches[index].data = event->data.u64;
    } else if (operation == ANDROID_EPOLL_CTL_MOD) {
        if (!event) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = EFAULT;
            return -1;
        }
        if (index < 0) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = ENOENT;
            return -1;
        }
        instance->watches[index].events = event->events;
        instance->watches[index].data = event->data.u64;
    } else if (operation == ANDROID_EPOLL_CTL_DEL) {
        if (index < 0) {
            LeaveCriticalSection(&android_epoll_lock);
            errno = ENOENT;
            return -1;
        }
        --instance->watch_count;
        if (index != instance->watch_count) {
            instance->watches[index] =
                instance->watches[instance->watch_count];
        }
    } else {
        LeaveCriticalSection(&android_epoll_lock);
        errno = EINVAL;
        return -1;
    }
    LeaveCriticalSection(&android_epoll_lock);
    return 0;
}

int android_epoll_wait(
    int epfd,
    android_epoll_event_t *events,
    int max_events,
    int timeout_ms) {
    android_epoll_watch_t watches[ANDROID_EPOLL_MAX_WATCHES];
    android_epoll_instance_t *instance;
    int watch_count;
    DWORD start;

    if (!events || max_events <= 0 || timeout_ms < -1) {
        errno = EINVAL;
        return -1;
    }
    android_epoll_initialize();
    EnterCriticalSection(&android_epoll_lock);
    instance = android_epoll_find_instance(epfd);
    if (!instance) {
        LeaveCriticalSection(&android_epoll_lock);
        errno = EBADF;
        return -1;
    }
    watch_count = instance->watch_count;
    memcpy(watches, instance->watches, watch_count * sizeof(watches[0]));
    LeaveCriticalSection(&android_epoll_lock);

    start = GetTickCount();
    for (;;) {
        fd_set read_set;
        fd_set write_set;
        fd_set except_set;
        struct timeval select_timeout;
        int socket_count = 0;
        int output_count = 0;
        int i;
        DWORD slice_ms = ANDROID_EPOLL_SELECT_SLICE_MS;

        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_ZERO(&except_set);
        for (i = 0; i < watch_count; ++i) {
            SOCKET socket_fd;
            if (!android_epoll_is_socket(watches[i].fd)) {
                continue;
            }
            socket_fd = (SOCKET)watches[i].fd;
            if (watches[i].events & (ANDROID_EPOLLIN | ANDROID_EPOLLPRI)) {
                FD_SET(socket_fd, &read_set);
            }
            if (watches[i].events & ANDROID_EPOLLOUT) {
                FD_SET(socket_fd, &write_set);
            }
            FD_SET(socket_fd, &except_set);
            ++socket_count;
        }

        if (timeout_ms >= 0) {
            DWORD elapsed = GetTickCount() - start;
            DWORD remaining = elapsed >= (DWORD)timeout_ms
                                  ? 0
                                  : (DWORD)timeout_ms - elapsed;
            if (slice_ms > remaining) {
                slice_ms = remaining;
            }
        }
        select_timeout.tv_sec = (long)(slice_ms / 1000);
        select_timeout.tv_usec = (long)((slice_ms % 1000) * 1000);
        if (socket_count > 0) {
            if (select(
                    0,
                    &read_set,
                    &write_set,
                    &except_set,
                    &select_timeout) == SOCKET_ERROR) {
                errno = EIO;
                return -1;
            }
        } else if (slice_ms > 0) {
            Sleep(slice_ms);
        }

        for (i = 0; i < watch_count && output_count < max_events; ++i) {
            uint32_t ready = 0;
            if (android_epoll_is_socket(watches[i].fd)) {
                SOCKET socket_fd = (SOCKET)watches[i].fd;
                if (FD_ISSET(socket_fd, &read_set)) {
                    ready |= watches[i].events &
                             (ANDROID_EPOLLIN | ANDROID_EPOLLPRI);
                }
                if (FD_ISSET(socket_fd, &write_set)) {
                    ready |= watches[i].events & ANDROID_EPOLLOUT;
                }
                if (FD_ISSET(socket_fd, &except_set)) {
                    ready |= ANDROID_EPOLLERR;
                }
            } else {
                ready = android_epoll_handle_events(&watches[i]);
            }
            if (ready) {
                events[output_count].events = ready;
                events[output_count].data.u64 = watches[i].data;
                ++output_count;
            }
        }
        if (output_count > 0) {
            return output_count;
        }
        if (timeout_ms == 0 ||
            (timeout_ms > 0 && GetTickCount() - start >= (DWORD)timeout_ms)) {
            return 0;
        }
    }
}

#endif
