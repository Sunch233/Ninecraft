#include "android_futex.h"

#ifdef _WIN32

#include <windows.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "android_errno.h"

typedef struct android_futex_waiter {
    HANDLE event;
    struct android_futex_waiter *next;
} android_futex_waiter_t;

typedef struct android_futex_queue {
    volatile void *addr;
    android_futex_waiter_t *waiters;
    struct android_futex_queue *next;
} android_futex_queue_t;

static HANDLE android_futex_mtx = NULL;
static android_futex_queue_t *futex_queues = NULL;

/* Called with android_futex_mtx held; the returned link also supports removal. */
static android_futex_queue_t **android_futex_find_queue(volatile void *addr) {
    android_futex_queue_t **link = &futex_queues;
    while (*link != NULL && (*link)->addr != addr) {
        link = &(*link)->next;
    }
    return link;
}

static DWORD android_futex_timeout_ms(const android_timespec_t *timeout) {
    ULONGLONG milliseconds;
    if (timeout == NULL) return INFINITE;
    milliseconds = (ULONGLONG)timeout->tv_sec * 1000;
    milliseconds += ((ULONGLONG)timeout->tv_nsec + 999999) / 1000000;
    return milliseconds < INFINITE ? (DWORD)milliseconds : INFINITE - 1;
}

int android_futex_init(void) {
    android_futex_mtx = CreateMutex(NULL, FALSE, NULL);
    if (android_futex_mtx == NULL) {
        return 0;
    }
    return 1;
}

int android_futex_wake_ex(volatile void *ftx, int pshared, int val) {
    android_futex_queue_t **link;
    android_futex_queue_t *queue;
    int woke = 0;

    (void)pshared;
    if (val <= 0) return val == 0 ? 0 : -1;
    WaitForSingleObject(android_futex_mtx, INFINITE);
    link = android_futex_find_queue(ftx);
    queue = *link;
    while (queue != NULL && queue->waiters != NULL && woke < val) {
        android_futex_waiter_t *waiter = queue->waiters;
        if (!SetEvent(waiter->event)) break;
        queue->waiters = waiter->next;
        ++woke;
    }
    if (queue != NULL && queue->waiters == NULL) {
        *link = queue->next;
        free(queue);
    }
    ReleaseMutex(android_futex_mtx);
    return woke;
}

int android_futex_wait_ex(volatile void *ftx, int pshared, int val, const android_timespec_t *timeout) {
    android_futex_queue_t **link;
    android_futex_queue_t *queue;
    android_futex_waiter_t **waiter_link;
    android_futex_waiter_t waiter;
    DWORD result;

    (void)pshared;
    if (timeout != NULL &&
        (timeout->tv_sec < 0 || timeout->tv_nsec < 0 || timeout->tv_nsec >= 1000000000)) {
        return -1;
    }
    waiter.event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (waiter.event == NULL) return -1;

    WaitForSingleObject(android_futex_mtx, INFINITE);
    if (InterlockedCompareExchange((long *)ftx, val, val) != val) {
        ReleaseMutex(android_futex_mtx);
        CloseHandle(waiter.event);
        errno = EAGAIN;
        return -1;
    }
    link = android_futex_find_queue(ftx);
    queue = *link;
    if (queue == NULL) {
        queue = (android_futex_queue_t *)malloc(sizeof(*queue));
        if (queue != NULL) {
            queue->addr = ftx;
            queue->waiters = NULL;
            queue->next = NULL;
            *link = queue;
        }
    }
    if (queue == NULL) {
        ReleaseMutex(android_futex_mtx);
        CloseHandle(waiter.event);
        return -1;
    }
    waiter.next = queue->waiters;
    queue->waiters = &waiter;
    ReleaseMutex(android_futex_mtx);

    result = WaitForSingleObject(waiter.event, android_futex_timeout_ms(timeout));

    WaitForSingleObject(android_futex_mtx, INFINITE);
    link = android_futex_find_queue(ftx);
    queue = *link;
    if (queue != NULL) {
        waiter_link = &queue->waiters;
        while (*waiter_link != NULL && *waiter_link != &waiter) {
            waiter_link = &(*waiter_link)->next;
        }
        if (*waiter_link == &waiter) *waiter_link = waiter.next;
        if (queue->waiters == NULL) {
            *link = queue->next;
            free(queue);
        }
    }
    ReleaseMutex(android_futex_mtx);
    CloseHandle(waiter.event);
    return result == WAIT_OBJECT_0 ? 0 :
        (result == WAIT_TIMEOUT ? -ANDROID_ETIMEDOUT : -1);
}

int android_futex_wait(volatile void *ftx, int val, const android_timespec_t *timeout) {
    return android_futex_wait_ex(ftx, 0, val, timeout);
}

int android_futex_wake(volatile void *ftx, int count) {
    return android_futex_wake_ex(ftx, 0, count);
}

#else

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Futex syscall wrapper */
static int futex(volatile void *addr, int op, int val, const android_timespec_t *timeout, volatile void *addr2, int val3) {
    return syscall(SYS_futex, addr, op, val, timeout, addr2, val3);
}

/* Wake `val` threads (with process-shared support) */
int android_futex_wake_ex(volatile void *ftx, int pshared, int val) {
    int op = pshared ? FUTEX_WAKE | FUTEX_PRIVATE_FLAG : FUTEX_WAKE;
    return futex(ftx, op, val, NULL, NULL, 0);
}

/* Wait with timeout (with process-shared support) */
int android_futex_wait_ex(volatile void *ftx, int pshared, int val, const android_timespec_t *timeout) {
    int op = pshared ? FUTEX_WAIT | FUTEX_PRIVATE_FLAG : FUTEX_WAIT;
    return futex(ftx, op, val, timeout, NULL, 0);
}

/* Standard futex wait (calls `__futex_wait_ex` with `pshared=0`) */
int android_futex_wait(volatile void *ftx, int val, const android_timespec_t *timeout) {
    return android_futex_wait_ex(ftx, 0, val, timeout);
}

/* Standard futex wake (calls `__futex_wake_ex` with `pshared=0`) */
int android_futex_wake(volatile void *ftx, int count) {
    return android_futex_wake_ex(ftx, 0, count);
}

#endif
