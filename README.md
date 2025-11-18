# Majjen

Majjen is a tiny round‑robin cooperative scheduler written in C99.

It lets you register small tasks that perform a bit of work and then voluntarily remove themselves from the scheduler.

It’s designed for well‑behaved, non‑blocking state machines and simple one‑shot tasks, where each task runs quickly and never blocks — but you’re free to put any logic you want in a task’s `run` function, as long as it doesn’t block for too long.

## Features

- Single-threaded, cooperative scheduler
- Simple task abstraction (`mj_task`) with callbacks and user-defined context
- Fixed-size task array (`MAX_TASKS`)
- Clear ownership model: scheduler owns tasks and their context once added

## How it works

The scheduler core lives in `src/libs/majjen.c` / `src/libs/majjen.h`.

Tasks are represented by the `mj_task` struct:

```c
typedef struct mj_scheduler mj_scheduler;

typedef void (*mj_task_fn)(mj_scheduler* scheduler, void* ctx);

typedef struct mj_task {
    mj_task_fn create;   // optional: called once when added (unused in demo)
    mj_task_fn run;      // required: called repeatedly while the task is alive
    mj_task_fn cleanup;  // optional: called when the task is removed
    void* ctx;           // opaque task state
} mj_task;
```

The main API looks like this:

```c
// Scheduler lifecycle
mj_scheduler* mj_scheduler_create(void);
int mj_scheduler_destroy(mj_scheduler** scheduler); // sets *scheduler = NULL on success

// Run loop (blocks until all tasks have finished and removed themselves)
int mj_scheduler_run(mj_scheduler* scheduler);

// Task management
int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task* task);  // takes ownership of `task`
int mj_scheduler_task_remove_current(mj_scheduler* scheduler);      // only valid from inside a task
```

Internally, the scheduler keeps a fixed-size array of `mj_task*` of length `MAX_TASKS` and cycles through it in a simple round-robin loop, calling each task’s `run` callback while there are tasks left.

---

## Demo program

The demo code lives in `src/demo_task.c`, `src/demo_task.h`, and `src/main.c`.

The demo task counts from 1 up to a configured value and then removes itself from the scheduler.

### Demo task

`src/demo_task.h` defines the per-task context and a helper to create a counting task:

```c
typedef struct demo_task_ctx {
    int count_to;        // When to stop counting
    int count;           // Current value
    void* unused_heap_ptr; // Placeholder for extra resources
} demo_task_ctx;

mj_task* demo_task_counter_create_task(int count_to);
```

`src/demo_task.c` implements `demo_task_counter_create_task` and the task’s `run` function. The `run` function:

- Increments the counter on each call
- Prints `"Counting to N (current)"`
- When `count >= count_to`, prints a final message and calls `mj_scheduler_task_remove_current(scheduler)`
- Sleeps for 250 ms between iterations using a small helper `sleep_ms(250)`

### Main

`src/main.c` wires everything together:

```c
int main(void) {
    mj_scheduler* scheduler = mj_scheduler_create();

    // Create a task with your own helper function and add it to the scheduler
    mj_scheduler_task_add(scheduler, demo_task_counter_create_task(2));

    mj_scheduler_run(scheduler);      // blocks until all tasks are done
    mj_scheduler_destroy(&scheduler); // frees scheduler and sets pointer to NULL

    return 0;
}
```

Running this program will interleave the output from all three tasks as the scheduler cycles through its task list.

---

## Ownership and memory model

- You allocate `mj_task` instances (and their `ctx`) on the heap.
- After calling `mj_scheduler_task_add`, the scheduler owns that `mj_task` and its `ctx`.
- When a task is finished, it must call `mj_scheduler_task_remove_current(scheduler)` from inside its `run` function. That function:
  - Calls the task’s `cleanup` callback if it is non-`NULL`.
  - Frees `task->ctx`.
  - Frees the `mj_task` itself.
  - Clears the task’s slot in the scheduler’s array and decrements the internal task count.
- Do not keep external pointers to a task’s `ctx` after the task has removed itself; they will be dangling.

The demo code in `src/demo_task.c` is a good reference for how to structure your own tasks to fit this model.

---

## Utilities

### `sleep_ms`

`src/utils/sleep_ms.h` provides a small helper:

```c
static inline void sleep_ms(unsigned int ms);
```

It uses `nanosleep` to sleep for a given number of milliseconds, correctly handling `EINTR` by resuming the remaining sleep.

### `clock_timer`

`src/utils/timer.h` / `timer.c` implement a simple monotonic timer utility (`clock_timer_t`) with helpers to measure elapsed time in ns/µs/ms/s and format it as a string. This is useful for benchmarking or experimenting with timing but is not required to use the scheduler.

---

## Requirements

- C99-compatible compiler (`gcc` or `clang`)
- POSIX-like system (developed and tested on Linux)
- Standard C library and POSIX time functions (`time.h`, `nanosleep`, `clock_gettime`)

No third-party libraries are required.
