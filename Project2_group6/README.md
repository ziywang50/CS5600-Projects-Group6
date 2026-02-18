
# CS 5600 Project 2 — One-way Stairs (POSIX threads + semaphores)

> **Fill these in before submission**
- Group number: **groupX**
- Team members: **(name1, name2, ...)**

## 1) Project summary
This program simulates a department store staircase connecting two floors. Each customer is a thread with a direction (`up` or `down`). The stairs has `S` steps (cells). **At most one customer can occupy any step at a time**, and **only one direction is allowed on the staircase at a time** (otherwise you can deadlock when customers refuse to back up).

Goals required by the prompt:
1. Prevent deadlock
2. Prevent starvation
3. Allow multiple customers to use the stairs in the same direction efficiently

## 2) Build & run

### Build
```bash
make
```

### Run
```bash
./Project2 <num_customers<=30> <num_steps<=13> [step_time_ms] [seed]
```

Examples:
```bash
./Project2 20 10
./Project2 30 13 150 12345
```

## 3) Implementation details

### Synchronization design (deadlock-free)
We separate the problem into two layers:

**A) Direction gate (mutex + condition variable)**  
The staircase is treated like a one-lane bridge:
- If the stairs is currently serving `UP`, only `UP` customers may enter.
- If the stairs is currently serving `DOWN`, only `DOWN` customers may enter.
- When the stairs becomes empty, the manager chooses which direction to serve next.

Because opposite directions are never admitted concurrently, the classic head-on deadlock cannot happen.

**B) Per-step occupancy (array of semaphores)**  
We keep `S` semaphores: `step_sem[0..S-1]`, each initialized to 1.
A customer:
- waits on the first step semaphore
- repeatedly waits on the next step semaphore, then releases the previous one
This guarantees **at most one customer per step**.

### Starvation prevention (fairness)
We use a bounded “batch” policy:

- While one direction is active, we admit up to `MAX_BATCH` customers in that direction.
- If the opposite direction is waiting and the current batch reaches `MAX_BATCH`, we stop admitting new customers in the current direction.
- Once the stairs empties, we switch to the opposite direction.

This ensures that if customers keep arriving from both floors, neither side can starve the other.

### Functions and purpose
- `enter_stairs_gate(...)`  
  Uses `pthread_mutex` + `pthread_cond` to wait until the caller may enter given the current direction & fairness rules.
- `cross_stairs_steps(...)`  
  Uses per-step semaphores so only one customer can be on each stair step at a time.
- `leave_stairs_gate(...)`  
  Decrements the number of customers on the stairs and switches/reset direction when the stairs becomes empty.
- `customer_thread(...)`  
  The thread routine: records turnaround time, enters gate, crosses, leaves, stores timing.

## 4) Testing & test cases

Suggested tests (all should complete with no deadlock and with both directions eventually served):

1. **Small**
```bash
./Project2 6 5 200 1
```

2. **Max size**
```bash
./Project2 30 13 100 2
```

3. **Stress, slower steps**
```bash
./Project2 30 13 300 3
```

4. **Repeat with different seeds**
```bash
for s in 1 2 3 4 5; do ./Project2 30 13 150 $s; done
```

What to look for:
- Program always terminates (deadlock-free).
- Both directions appear over time (no starvation).
- Logs show multiple customers crossing consecutively in the same direction (efficient batching).

## 5) Turnaround time & efficiency notes
Each thread measures turnaround time as:
- start: when thread begins (arrival)
- end: after fully exiting the stairs

The program prints each customer’s turnaround and the average.

Efficiency knobs:
- `step_time_ms` controls the time spent per step.
- `MAX_BATCH` in `project2.c` controls fairness vs throughput.
  - larger batch → higher throughput but longer waits for the opposite direction
  - smaller batch → more fairness but more switching

## 6) How to compile, run, test
- Compile: `make`
- Run: `./Project2 N S [step_time_ms] [seed]`
- Clean: `make clean`

## 7) Contributions
- (name) — design / implementation / testing / README
- (name) — ...
