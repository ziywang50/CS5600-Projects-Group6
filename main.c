#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include "sync.h"

/*
 * Structure passed to each customer thread.
 */
typedef struct {
    int id;      // Customer ID
    int dir;     // Direction (DIR_UP or DIR_DOWN)
    int steps;   // Number of steps (used as sleep time)
    struct timespec start;
    struct timespec end;
} CustomerArgs;

/*
 * Thread function representing a customer.
 */
static void* customer_thread(void* arg) {
    CustomerArgs* a = (CustomerArgs*)arg;

    clock_gettime(CLOCK_MONOTONIC, &a->start);

    printf("customer %d direction=%s\n",
           a->id, (a->dir == DIR_UP ? "UP" : "DOWN"));

    /*
     * Request to enter the stairs.
     */
    arrive(a->dir, a->id);

    /*
     * Simulate crossing the stairs.
     * In this basic version, we simply sleep for "steps" seconds.
     */
    sleep(a->steps);

    /*
     * Leave the stairs.
     */
    leave(a->dir, a->id);

    clock_gettime(CLOCK_MONOTONIC, &a->end);

    return NULL;
}

int main(int argc, char* argv[]) {
    /*
     * Program usage:
     * ./stairs <num_customers<=30> <steps<=13>
     */
    if (argc != 3) {
        printf("Usage: %s <num_customers<=30> <steps<=13>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int steps = atoi(argv[2]);

    if (n <= 0 || n > 30 || steps <= 0 || steps > 13) {
        printf("Invalid input. num_customers<=30 and steps<=13\n");
        return 1;
    }

    srand((unsigned)time(NULL));

    /*
     * Initialize synchronization module.
     */
    sync_init(steps);

    pthread_t threads[30];
    CustomerArgs args[30];

    /*
     * Create customer threads.
     */
    for (int i = 0; i < n; i++) {
        args[i].id = i;
        args[i].dir = (rand() % 2) ? DIR_UP : DIR_DOWN;
        args[i].steps = steps;

        if (pthread_create(&threads[i], NULL, customer_thread, &args[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    /*
     * Wait for all threads to finish.
     */
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    /*Calculate average turnaround time*/
    double total_ms = 0;

    for (int i = 0; i < n; i++) {
        double start_ms =
            args[i].start.tv_sec * 1000.0 +
            args[i].start.tv_nsec / 1e6;

        double end_ms =
            args[i].end.tv_sec * 1000.0 +
            args[i].end.tv_nsec / 1e6;

        double turnaround = end_ms - start_ms;

        printf("Customer %d turnaround: %.2f ms\n",
                args[i].id, turnaround);

        total_ms += turnaround;
    }

    printf("Average turnaround: %.2f ms\n", total_ms / n);


    /*
     * Destroy synchronization resources.
     */
    sync_destroy();

    printf("All customers finished.\n");
    return 0;
}
