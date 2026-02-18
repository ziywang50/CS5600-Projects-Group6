#pragma once
#include <pthread.h>
#include <semaphore.h>

#define DIR_UP 1
#define DIR_DOWN 2

void sync_init(int max_steps);
void sync_destroy(void);

void arrive(int dir, int id);
void leave(int dir, int id);
