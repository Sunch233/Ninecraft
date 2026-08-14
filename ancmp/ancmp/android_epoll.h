#ifndef ANCMP_ANDROID_EPOLL_H
#define ANCMP_ANDROID_EPOLL_H

#ifdef _WIN32

#include <stdint.h>

#define ANDROID_EPOLLIN 0x001u
#define ANDROID_EPOLLPRI 0x002u
#define ANDROID_EPOLLOUT 0x004u
#define ANDROID_EPOLLERR 0x008u
#define ANDROID_EPOLLHUP 0x010u
#define ANDROID_EPOLLRDHUP 0x2000u
#define ANDROID_EPOLLET 0x80000000u

#define ANDROID_EPOLL_CTL_ADD 1
#define ANDROID_EPOLL_CTL_DEL 2
#define ANDROID_EPOLL_CTL_MOD 3

typedef union android_epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} android_epoll_data_t;

#pragma pack(push, 1)
typedef struct android_epoll_event {
    uint32_t events;
    android_epoll_data_t data;
} android_epoll_event_t;
#pragma pack(pop)

int android_epoll_create(int size);
int android_epoll_ctl(
    int epfd,
    int operation,
    int fd,
    android_epoll_event_t *event);
int android_epoll_wait(
    int epfd,
    android_epoll_event_t *events,
    int max_events,
    int timeout_ms);

#else

#include <sys/epoll.h>

#define android_epoll_create epoll_create
#define android_epoll_ctl epoll_ctl
#define android_epoll_wait epoll_wait

#endif

#endif
