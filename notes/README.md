# Majjen: A Generic Cooperative Scheduler in C99 (heavy WIP, including this readme)

In a cooperative model, each task runs until it voluntarily yields control back to the scheduler. This makes Majjen well-suited for embedded systems, protocol handlers, and other applications where many tasks must be interleaved without the overhead of preemption.

Majjen is simple by design. The scheduler iterates over a list of tasks, each represented by an `mj_task` struct. The scheduler manages the memory for the `mj_task` structs, but the user retains full ownership of the memory provided via the `user_state` pointer.

```c
// Each task is a function with this signature
typedef void (*mj_task_func)(void* user_state);

typedef struct mj_task {
    mj_task_func task_function;   // The user-provided function to run
    void*        user_state;      // User-owned state for the task
    // Internal scheduler fields below
    TaskState    state_type;      // e.g. MJ_RUNNABLE, MJ_WAIT_TIMER
    uint64_t     expiry_ns;       // When the task should resume (for sleep)
    // Internal linkage pointers...
} mj_task;
```

## Initializing the Scheduler
Before creating tasks, you must initialize a scheduler:
```c
mj_scheduler* mj_scheduler_create(void);
void mj_scheduler_destroy(mj_scheduler* scheduler);
void mj_scheduler_run(mj_scheduler* scheduler);
```

Typical setup:
```c
mj_scheduler* my_scheduler = mj_scheduler_create();
mj_create_task(my_scheduler, my_task_function, my_state_pointer);

// mj_scheduler_run blocks until all tasks have exited
mj_scheduler_run(my_scheduler);

// Clean up all resources
mj_scheduler_destroy(my_scheduler);
```

`mj_scheduler_run` enters the main scheduling loop and will continue running until all tasks have called `mj_task_exit()`.

## Creating a Task

Add a new task to the scheduler:
```c
mj_task* mj_create_task(mj_scheduler* scheduler, mj_task_func func, void* state);
```
Each task is provided with a `user_state` pointer, which your application code controls entirely.

## Controlling a Task From Within

A running task can control its own execution by calling a number of control functions. The scheduler keeps track of the currently running `mj_task`, so any control function will automatically apply to the task that is currently executing.

To signal that a task has finished its work and should be removed from the scheduler, it must call `mj_task_exit()`. The scheduler will then remove the task and free its associated resources on the next scheduling cycle. (Note: the `user_state` pointer is owned by you and must be freed by your own code if it was dynamically allocated).

```c
// Signals to the scheduler that the currently running task is complete.
void mj_task_exit(void);
```

A task that is not finished can voluntarily yield control to the scheduler, either for a set amount of time or until the next scheduling cycle.

```c
// Yield control and request to be woken up after a delay.
void mj_task_sleep_ns(uint64_t ns);

// Yield control until the next scheduler cycle.
void mj_task_yield(void);
```

Example of a long-running task that yields:
```c
void my_periodic_task(void* user_state) {
    int* counter = (int*)user_state;
    (*counter)++;

    if (*counter >= 10) {
        printf("Periodic task has run 10 times, now exiting.\n");
        free(counter); // Clean up user_state if it was allocated
        mj_task_exit();
        return; // It's good practice to return after calling exit
    }

    printf("Doing some work... run #%d\n", *counter);

    // Sleep for 1 second before running again
    mj_task_sleep_ns(1000000000);
}
```
