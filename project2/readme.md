***Project2--POSIX Thread Implementation***

Group6
Members: 
Ziyue Wang  Yuzhou Pan  Wanyu Zhang

Deadlock-free:
We enforce a single active direction on the staircase at any time (current_dir). Threads whose direction does not match current_dir block on a condition variable, so circular wait cannot occur.

Starvation-free (fairness):
When the opposite direction has waiting customers, we limit the number of admissions in the current direction using a batch counter (batch_count) and a batch limit (batch_limit = steps). After admitting a batch, no new threads in the current direction are allowed to enter; the staircase drains to empty and then direction switches to the opposite side. This guarantees bounded waiting for both directions.

Performance Model:
In our implementation, each customer sleeps for **1 second per step**
to simulate the traversal time of the staircase.

Therefore, the theoretical crossing time for a customer is:

    steps × 1 second

Since the workload is sleep-dominated (I/O-style simulation),
the wall-clock turnaround time is primarily determined by
this fixed traversal delay and scheduling effects,
rather than by synchronization overhead.

As a result, performance differences between optimization
strategies are not clearly visible in this time scale.

Efficiency:
We allow up to steps customers to be on the staircase simultaneously in the same direction (on_stairs < steps), modeling each step as capacity.

Turnaround time:
Each thread records CLOCK_MONOTONIC timestamps at start and end, and reports turnaround time in milliseconds. We also compute the average turnaround time across all customers.

Test Results:
| Customers | Steps | Observed Max on_stairs | Deadlock | Starvation | Avg_turnaround_time (ms)
| --------- | ----- | ---------------------- | -------- | ---------- | ------------------------
| 20        | 1     | 1                      | No       | No         | 10506.98
| 20        | 3     | 3                      | No       | No         | 12007.76
| 20        | 5     | 5                      | No       | No         | 13004.83
| 30        | 13    | 13                     | No       | No         | 22103.03


Compile and Run Code:
 gcc -Wall -O2 -o stairs_2 main.c sync.c -pthread
Run:
./stairs_2 <num_customers> <steps>

Optimization Decisions:
1. We set **batch_limit = steps**
2. We implemented selective wakeup to reduce unnecessary wakeups (thundering herd).
However, under the required workload where each thread sleeps for steps seconds to simulate traversal, wall-clock turnaround is dominated by this fixed delay and by scheduling randomness. Therefore, selective wakeup does not consistently reduce average turnaround in our runs; its primary benefit is reduced contention and fewer spurious wakeups, which improves scalability in CPU-bound scenarios.

This ensures that:
Up to steps customers can enter in the same direction simultaneously.
Direction switching happens only after a full batch finishes.
Excessive switching overhead is avoided.
Opposite direction customers are guaranteed bounded waiting time.


Contributions of each member:
Ziyue Wang - Design, testing, turnaround time calculation, readme.md
Yuzhou Pan - Design, documentation, optimization
Wanyu Zhang - Design, implementation of sync.c
