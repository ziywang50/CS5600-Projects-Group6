#define _XOPEN_SOURCE 700
#include "sync.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define MAX_CUSTOMERS 30
#define MAX_STEPS 13

typedef struct {
    int id;
    direction_t dir;
    int64_t turnaround_ms;
} customer_t;

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <num_customers<=30> <num_steps<=13> [step_time_ms] [seed] [max_batch]\n"
        "  Example: %s 20 10\n"
        "           %s 30 13 150 12345 5\n",
        prog, prog, prog);
}

static void* customer_thread(void *arg) {
    customer_t *c = (customer_t*)arg;

    int64_t start = sync_now_ms();

    sync_enter_gate(c->id, c->dir);
    sync_cross_steps(c->id, c->dir);
    sync_leave_gate(c->id);

    int64_t end = sync_now_ms();
    c->turnaround_ms = end - start;
    return NULL;
}

static void tiny_stagger_ns(long ns) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = ns;
    nanosleep(&req, NULL);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    int n_customers = atoi(argv[1]);
    int n_steps = atoi(argv[2]);
    int step_time_ms = (argc >= 4) ? atoi(argv[3]) : 200;
    unsigned int seed = (argc >= 5) ? (unsigned int)strtoul(argv[4], NULL, 10) : (unsigned int)time(NULL);
    int max_batch = (argc >= 6) ? atoi(argv[5]) : 5;

    if (n_customers < 1 || n_customers > MAX_CUSTOMERS) {
        fprintf(stderr, "num_customers must be 1..%d\n", MAX_CUSTOMERS);
        return 1;
    }
    if (n_steps < 1 || n_steps > MAX_STEPS) {
        fprintf(stderr, "num_steps must be 1..%d\n", MAX_STEPS);
        return 1;
    }

    srand(seed);

    if (sync_init(n_steps, step_time_ms, max_batch) != 0) {
        fprintf(stderr, "sync_init failed\n");
        return 2;
    }

    pthread_t th[MAX_CUSTOMERS];
    customer_t customers[MAX_CUSTOMERS];

    for (int i = 0; i < n_customers; i++) {
        customers[i].id = i;
        customers[i].dir = (rand() % 2 == 0) ? DIR_UP : DIR_DOWN;
        customers[i].turnaround_ms = 0;

        if (pthread_create(&th[i], NULL, customer_thread, &customers[i]) != 0) {
            perror("pthread_create");
            return 3;
        }

        tiny_stagger_ns((long)(rand() % 15) * 1000000L);
    }

    int64_t sum = 0;
    for (int i = 0; i < n_customers; i++) {
        pthread_join(th[i], NULL);
        sum += customers[i].turnaround_ms;
    }

    printf("\n--- Turnaround times ---\n");
    for (int i = 0; i < n_customers; i++) {
        printf("Customer %2d (%s): %lld ms\n", customers[i].id, sync_dir_str(customers[i].dir), (long long)customers[i].turnaround_ms);
    }
    printf("Average turnaround: %.2f ms\n", (double)sum / (double)n_customers);

    sync_destroy();
    return 0;
}
