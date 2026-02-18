#define _XOPEN_SOURCE 700
#include "sync.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#define MAX_STEPS 13

static int g_steps = 0;
static int g_step_time_ms = 200;
static int g_max_batch = 5;

static sem_t g_step_sem[MAX_STEPS];

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv   = PTHREAD_COND_INITIALIZER;

static direction_t g_dir = DIR_NONE;
static int g_on_stairs = 0; 
static int g_waiting_up = 0;
static int g_waiting_down = 0;

static int g_batch_count = 0;
static int g_block_current_dir = 0;
static direction_t g_last_dir = DIR_DOWN;

static struct timespec g_t0;

// time helpers
static inline void msleep(int ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
}

int64_t sync_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t ms = (int64_t)(ts.tv_sec - g_t0.tv_sec) * 1000
               + (ts.tv_nsec - g_t0.tv_nsec) / 1000000;
    return ms;
}

const char* sync_dir_str(direction_t d) {
    return (d == DIR_UP) ? "up" : "down";
}

//Logging
static void log_msg(int id, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%6lld ms] ", (long long)sync_now_ms());
    if (id >= 0) fprintf(stdout, "Customer %2d: ", id);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

// admission control
static int opposite_waiting(direction_t d) {
    return (d == DIR_UP) ? (g_waiting_down > 0) : (g_waiting_up > 0);
}

static direction_t choose_dir_when_empty(void) {
    if (g_waiting_up > 0 && g_waiting_down == 0) return DIR_UP;
    if (g_waiting_down > 0 && g_waiting_up == 0) return DIR_DOWN;
    if (g_waiting_up == 0 && g_waiting_down == 0) return DIR_NONE;

    return (g_last_dir == DIR_UP) ? DIR_DOWN : DIR_UP;
}

int sync_init(int steps, int step_time_ms, int max_batch) {
    if (steps < 1 || steps > MAX_STEPS) return -1;
    if (step_time_ms <= 0) step_time_ms = 200;
    if (max_batch <= 0) max_batch = 5;

    g_steps = steps;
    g_step_time_ms = step_time_ms;
    g_max_batch = max_batch;

    // Reset manager state
    g_dir = DIR_NONE;
    g_on_stairs = 0;
    g_waiting_up = 0;
    g_waiting_down = 0;
    g_batch_count = 0;
    g_block_current_dir = 0;
    g_last_dir = DIR_DOWN;

    clock_gettime(CLOCK_MONOTONIC, &g_t0);

    for (int i = 0; i < g_steps; i++) {
        if (sem_init(&g_step_sem[i], 0, 1) != 0) {
            // best-effort cleanup
            for (int j = 0; j < i; j++) sem_destroy(&g_step_sem[j]);
            return -2;
        }
    }
    return 0;
}

void sync_destroy(void) {
    for (int i = 0; i < g_steps; i++) {
        sem_destroy(&g_step_sem[i]);
    }
}

void sync_enter_gate(int id, direction_t mydir) {
    pthread_mutex_lock(&g_lock);
    log_msg(id, "waiting for mutex");

    if (mydir == DIR_UP) g_waiting_up++;
    else g_waiting_down++;

    for (;;) {
        // direction
        if (g_on_stairs == 0) {
            direction_t chosen = choose_dir_when_empty();
            if (chosen != DIR_NONE) {
                g_dir = chosen;
                g_batch_count = 0;
                g_block_current_dir = 0;
                log_msg(-1, "Crossing direction reset -> %s", sync_dir_str(g_dir));
            } else {
                g_dir = DIR_NONE;
            }
        }

        int dir_ok = (g_dir == mydir);
        int blocked = (g_block_current_dir && g_dir == mydir);

        if (g_dir != DIR_NONE && dir_ok && !blocked) {
            g_on_stairs++;
            g_batch_count++;

            if (mydir == DIR_UP) g_waiting_up--; else g_waiting_down--;

            // If opposite is waiting and we've admitted enough in this batch, stop admitting more.
            if (opposite_waiting(g_dir) && g_batch_count >= g_max_batch) {
                g_block_current_dir = 1;
            }

            pthread_mutex_unlock(&g_lock);
            return;
        }

        log_msg(id, "going %s should wait", sync_dir_str(mydir));
        pthread_cond_wait(&g_cv, &g_lock);
    }
}

void sync_leave_gate(int id) {
    (void)id;
    pthread_mutex_lock(&g_lock);

    g_on_stairs--;
    if (g_on_stairs < 0) g_on_stairs = 0;

    if (g_on_stairs == 0) {
        g_last_dir = g_dir;

        direction_t next = choose_dir_when_empty();
        if (next == DIR_NONE) {
            g_dir = DIR_NONE;
            g_batch_count = 0;
            g_block_current_dir = 0;
            log_msg(-1, "Crossing direction reset -> none");
        } else if (next != g_dir) {
            g_dir = next;
            g_batch_count = 0;
            g_block_current_dir = 0;
            log_msg(-1, "Crossing direction reset -> %s", sync_dir_str(g_dir));
        } else {
            // Same direction continues; if opposite isn't waiting, we can just reset the batch
            g_batch_count = 0;
            g_block_current_dir = 0;
            log_msg(-1, "Crossing direction continues -> %s", sync_dir_str(g_dir));
        }
    }

    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_lock);
}

void sync_cross_steps(int id, direction_t d) {
    // from
    int start = (d == DIR_UP) ? 0 : (g_steps - 1);
    int end   = (d == DIR_UP) ? (g_steps - 1) : 0;
    int step  = (d == DIR_UP) ? 1 : -1;

    // first step
    sem_wait(&g_step_sem[start]);

    log_msg(id, "crossing the stairs now (%s)", sync_dir_str(d));

    int cur = start;
    while (cur != end) {
        int next = cur + step;
        sem_wait(&g_step_sem[next]);
        msleep(g_step_time_ms);
        sem_post(&g_step_sem[cur]);
        cur = next;
    }

    // release
    msleep(g_step_time_ms);
    sem_post(&g_step_sem[cur]);

    log_msg(id, "finished stairs (%s)", sync_dir_str(d));
}
