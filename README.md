# Majjen

**(readme out of date, will be updated soon. Check demo_task.c and .h for now)**

A tiny cooperative scheduler in C99. It lets you register simple task callbacks with user-provided state and runs them in a round‑robin loop until each task removes itself.

## Features
- Cooperative, single-threaded scheduler
- Simple task API: function + opaque state pointer
- Fixed maximum number of tasks (`MAX_TASKS`)
- Round‑robin execution until all tasks complete
- Clear ownership: scheduler frees each task’s state when the task exits itself

## Usage
- Create a scheduler with `mj_scheduler_create()`.
- Add tasks with `mj_scheduler_task_add(s, cb, state)`.
- Start the loop with `mj_scheduler_run(s)`; it iterates over task slots and calls each callback with its associated state.
- When a task is done, it must call `mj_scheduler_task_remove_current(s)`. That function frees the task’s state and the task record, clears the slot, and decrements `task_count`.
- When no tasks remain, `mj_scheduler_run` returns.
- Destroy the scheduler with `mj_scheduler_destroy(&s)`.

## Public API 

```c
#define MAX_TASKS 1000

typedef struct mj_task mj_task;          // opaque
typedef struct mj_scheduler mj_scheduler; // opaque

typedef void (mj_task_fn)(mj_scheduler* scheduler, void* state);

// Lifecycle
mj_scheduler* mj_scheduler_create(void);
int mj_scheduler_destroy(mj_scheduler** scheduler); // sets *scheduler = NULL on success

// Run (blocks until all tasks finished)
int mj_scheduler_run(mj_scheduler* scheduler); // returns 0 when all tasks are done

// Task management
int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task_fn, void* user_state);
int mj_scheduler_task_remove_current(mj_scheduler* scheduler); // only valid from inside a task
```

Return values follow simple conventions: `-1` on invalid arguments or out‑of‑capacity conditions (with `errno` set), `0` on success. Destroy returns `1` with `errno=EBUSY` if tasks are still present.

## Example
A simple counter task that removes itself at 10:

```c
// Callback: increments until reaching 10, then removes itself
void mj_cb_count_to_ten(mj_scheduler* scheduler, void* user_state) {
    int* task_state = (int*)user_state;
    if (*task_state >= 10) {
        mj_scheduler_task_remove_current(scheduler);
        return;
    }
    (*task_state)++;
}

// Helper to allocate and add the task
void mj_add_task_count_up(mj_scheduler* scheduler, int start) {
    int* state = malloc(sizeof(*state));
    *state = start;
    mj_scheduler_task_add(scheduler, mj_cb_count_to_ten, state);
}

int main(void) {
    mj_scheduler* scheduler = mj_scheduler_create();
    mj_add_task_count_up(scheduler, 5);
    mj_scheduler_run(scheduler);
    mj_scheduler_destroy(&scheduler);
    return 0;
}
```
Note: this example is just an illustration and does not check for errors.

## Ownership & safety
- Allocate task state on the heap (e.g., `malloc`). The scheduler will `free()` it when the task removes itself via `mj_scheduler_task_remove_current`.
- Do not pass pointers to stack variables as state.
- Do not keep aliases to the state outside the scheduler. After the task completes, the scheduler frees the state; any external alias becomes dangling.

Minimal anti-pattern example (use‑after‑free):
```c
int* add_task_and_leak_pointer(mj_scheduler* s) {
    int* state = malloc(sizeof(*state));
    *state = 99;
    mj_scheduler_task_add(s, mj_cb_count_to_ten, state);
    return state; // escaping pointer; do NOT use afterward
}

void demo(void) {
    mj_scheduler* s = mj_scheduler_create();
    int* leaked = add_task_and_leak_pointer(s);
    mj_scheduler_run(s);             // task will free 'state' on completion
    mj_scheduler_destroy(&s);
    printf("%d\n", *leaked);        // DANGEROUS: leaked is now dangling
}
```

## Build & run
The repo includes a Makefile that builds a single binary named `app`.

```bash
make        # builds ./app
make run    # runs ./app
make clean  # cleans build/ and app
```

## Dependencies & Platform Support

Majjen uses only the C standard library plus POSIX headers (`unistd.h`, potentially `time.h` with POSIX feature macros). The current code defines `_POSIX_C_SOURCE=200809L` via `CFLAGS` in the `Makefile`.

| Requirement            | Notes |
|------------------------|-------|
| C99 compiler           | Tested with GCC (should also work with Clang). |
| POSIX environment      | Relies on `_POSIX_C_SOURCE` features (e.g. `nanosleep` if added later). |
| Operating system       | Developed & tested on Linux. Should compile on other POSIX-like systems (e.g. BSD, macOS) since no Linux-specific syscalls are used yet. |
| Standard library only  | No external third-party dependencies. |

### Portability
Right now: the implementation is purely cooperative with no platform-specific I/O or timers, so porting should be straightforward wherever a C99 + POSIX environment exists. 

### Minimal build check (Linux)
If you have GCC installed, the provided `Makefile` should work out of the box:

```bash
make
./app
```

If using Clang:

```bash
CC=clang make
```

On macOS/BSD you may remove or adapt any future Linux-specific code once added (none present yet).

Tested on Linux with GCC and `-std=c99`.

## Limitations (current state)
- No timers/yield API; tasks run full-speed round‑robin until they finish.
- Fixed capacity `MAX_TASKS` array; adding beyond capacity fails with `errno=ENOMEM`.
- Single-threaded cooperative model; no preemption.

## Roadmap 
- Sleep/yield primitives and a timer wheel/min-heap for wakeups
- Pluggable wait function for idle efficiency (e.g., nanosleep/epoll)
- Logging and basic metrics
- Dynamic task storage structure (free list or vector) instead of fixed slots
