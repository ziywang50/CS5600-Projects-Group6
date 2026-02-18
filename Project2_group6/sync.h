#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { DIR_NONE = 0, DIR_UP = 1, DIR_DOWN = -1 } direction_t;

// Initialize global synchronization state.
// steps: number of stair steps (1..13)
// step_time_ms: time a customer spends per step
// max_batch: fairness/throughput knob (>=1). If max_batch<=0, defaults to 5.
int sync_init(int steps, int step_time_ms, int max_batch);

// Release resources.
void sync_destroy(void);

// Time since init (ms), monotonic.
int64_t sync_now_ms(void);

// Direction string ("up"/"down").
const char* sync_dir_str(direction_t d);

// Gate: wait until allowed to enter the stairs in direction d.
void sync_enter_gate(int id, direction_t d);

// Walk across the stairs, enforcing per-step occupancy with semaphores.
void sync_cross_steps(int id, direction_t d);

// Leave the stairs gate (call after finishing the last step).
void sync_leave_gate(int id);

#ifdef __cplusplus
}
#endif

#endif // SYNC_H
