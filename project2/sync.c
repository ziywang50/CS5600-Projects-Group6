#include "sync.h"
#include <stdio.h>

/*
 * Staircase synchronization (Version 1) - mutex + condition variable
 *
 * Requirements:
 * 1) Prevent deadlock: only one direction on the stairs at a time.
 * 2) Prevent starvation: if the opposite direction is waiting, limit how many
 *    customers can enter in the current direction (batch), then force a switch.
 * 3) Efficient: allow multiple customers on the stairs in the SAME direction.
 *    We model the staircase capacity as max_steps (e.g., 13).
 */

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv_up = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cv_down = PTHREAD_COND_INITIALIZER;


static int max_steps = 1;     // staircase capacity
static int current_dir = 0;   // 0 = NONE, DIR_UP, DIR_DOWN
static int on_stairs = 0;     // number of customers currently on stairs

static int waiting_up = 0;
static int waiting_down = 0;

/* Batch control to prevent starvation */
static int batch_count = 0;   // number entered since last direction change
static int batch_limit = 1;   // when opposite is waiting, stop admitting after this many

static int opposite_dir(int dir) {
    return (dir == DIR_UP) ? DIR_DOWN : DIR_UP;
}

static int waiting_count(int dir) {
    return (dir == DIR_UP) ? waiting_up : waiting_down;
}

static void waiting_inc(int dir) {
    if (dir == DIR_UP) waiting_up++;
    else waiting_down++;
}

static void waiting_dec(int dir) {
    if (dir == DIR_UP) waiting_up--;
    else waiting_down--;
}

/*
 * Decide if a customer can enter right now.
 * Rules:
 * - If idle (current_dir == 0), any direction can claim the stairs.
 * - Must match current_dir if not idle.
 * - Must have capacity (on_stairs < max_steps).
 * - Starvation prevention:
 *   If opposite direction is waiting AND batch_count >= batch_limit,
 *   stop admitting new customers in current_dir (let stairs drain, then switch).
 */
static int can_enter(int dir) {
    if (current_dir == 0) return 1;
    if (dir != current_dir) return 0;
    if (on_stairs >= max_steps) return 0;

    int opp = opposite_dir(dir);
    if (waiting_count(opp) > 0 && batch_count >= batch_limit) return 0;

    return 1;
}

void sync_init(int steps) {
    if (steps <= 0) steps = 1;
    max_steps = steps;

    /* A simple and effective choice: limit batch size to capacity */
    batch_limit = steps;

    current_dir = 0;
    on_stairs = 0;
    waiting_up = 0;
    waiting_down = 0;
    batch_count = 0;
}

void sync_destroy(void) {
    /* Nothing required for statically initialized lock/cv for this assignment. */
}

void arrive(int dir, int id) {
    pthread_mutex_lock(&lock);

    waiting_inc(dir);
    printf("[arrive] customer %d wants %s | waiting_up=%d waiting_down=%d\n",
           id, (dir == DIR_UP ? "UP" : "DOWN"), waiting_up, waiting_down);

    while (!can_enter(dir)) {
        printf("[wait  ] customer %d waiting | current_dir=%s on_stairs=%d batch=%d\n",
               id,
               (current_dir == 0 ? "NONE" : (current_dir == DIR_UP ? "UP" : "DOWN")),
               on_stairs, batch_count);
        if (dir == DIR_UP)
            pthread_cond_wait(&cv_up, &lock);
        else
            pthread_cond_wait(&cv_down, &lock);
    }

    /* If stairs were idle, claim direction and reset batch */
    if (current_dir == 0) {
        current_dir = dir;
        batch_count = 0;
        printf("[dir   ] direction set to %s\n", (current_dir == DIR_UP ? "UP" : "DOWN"));
    }

    waiting_dec(dir);
    on_stairs++;
    batch_count++;

    printf("[enter ] customer %d ENTER stairs | dir=%s on_stairs=%d batch=%d\n",
           id, (current_dir == DIR_UP ? "UP" : "DOWN"), on_stairs, batch_count);

    pthread_mutex_unlock(&lock);
}

void leave(int dir, int id) {
    (void)dir; /* current_dir is the active direction */

    pthread_mutex_lock(&lock);

    on_stairs--;
    printf("[leave ] customer %d LEAVE stairs | dir=%s on_stairs=%d\n",
           id,
           (current_dir == 0 ? "NONE" : (current_dir == DIR_UP ? "UP" : "DOWN")),
           on_stairs);

    /* If stairs become empty, decide next direction */
    if (on_stairs == 0) {
        int opp = opposite_dir(current_dir);

        if (current_dir != 0 && waiting_count(opp) > 0) {
            current_dir = opp;
            batch_count = 0;
            printf("[switch] stairs empty -> switch to %s\n",
                   (current_dir == DIR_UP ? "UP" : "DOWN"));
        } else if (current_dir != 0 && waiting_count(current_dir) > 0) {
            batch_count = 0;
            printf("[keep  ] stairs empty -> keep %s\n",
                   (current_dir == DIR_UP ? "UP" : "DOWN"));
        } else if (waiting_up > 0) {
            current_dir = DIR_UP;
            batch_count = 0;
            printf("[dir   ] stairs empty -> set to UP\n");
        } else if (waiting_down > 0) {
            current_dir = DIR_DOWN;
            batch_count = 0;
            printf("[dir   ] stairs empty -> set to DOWN\n");
        } else {
            current_dir = 0;
            batch_count = 0;
            printf("[idle  ] stairs empty -> direction NONE\n");
        }
    }

    if (on_stairs == 0) {
        if (current_dir == DIR_UP) pthread_cond_broadcast(&cv_up);
        else if (current_dir == DIR_DOWN) pthread_cond_broadcast(&cv_down);
        else { pthread_cond_broadcast(&cv_up); pthread_cond_broadcast(&cv_down); }
    } else {
        if (current_dir == DIR_UP) pthread_cond_signal(&cv_up);
        else if (current_dir == DIR_DOWN) pthread_cond_signal(&cv_down);
    }
    pthread_mutex_unlock(&lock);
}
