#include "majjen.h"
#include "timer.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

typedef struct mj_task {
    mj_task_fn* task;
    void* state;
} mj_task;

typedef struct mj_scheduler {
    mj_task* task_list[MAX_TASKS];
    mj_task** current_task; // must be pp so we dont have to search list to remove current task
    size_t task_count;      // How many tasks are in the list
} mj_scheduler;

/* hidden loop counter (file scope) */
static size_t mj_loop_count = 0;

void mj_scheduler_run(mj_scheduler* scheduler) {

    if (scheduler == NULL) {
        printf("scheduler is NULL\n");
        return;
    }
    if (scheduler->task_count == 0) {
        printf("Cant run with zero tasks.\n");
        return;
    }

    // setup timers
    clock_timer_t sched_timer;
    clock_timer_init(&sched_timer);
    int64_t start_time = 0;
    int64_t end_time = 0;
    int64_t elapsed = 0;
    int64_t total = 0;

    // there are tasks, run them
    clock_timer_start(&sched_timer);
    printf("Running with %d tasks:\n", MAX_TASKS);
    while (scheduler->task_count > 0) {
        start_time = clock_timer_elapsed_ns(&sched_timer);

        mj_loop_count++;
        // printf("\nRUNNING TASKS LOOP NR: %zu\n", mj_loop_count);
        //  note runs oldest task first, change for linked list
        for (int i = MAX_TASKS - 1; i >= 0; i--) {
            // Skipto next if no task in slot
            if (scheduler->task_list[i] == NULL) { continue; }

            // get the function and the state
            mj_task_fn* task = scheduler->task_list[i]->task;
            void* state = scheduler->task_list[i]->state;

            // set current function
            scheduler->current_task = &scheduler->task_list[i];

            // call user function with user state
            // printf("running function at index %d: ", i);
            fflush(stdout);
            task(scheduler, state);

            // reset current function, should only be availible from the task that just ran
            scheduler->current_task = NULL;
        }

        end_time = clock_timer_elapsed_ns(&sched_timer);
        elapsed = end_time;
        total += elapsed;

        // Convert nanoseconds to seconds and multiply by speed of light
        double light_traveled_meters = SPEED_OF_LIGHT * elapsed * 1e-9;
        printf("LOOP: %5.0ld, ELAPSED TIME: %6.3f ms, light traveled %6.1f km\n", mj_loop_count, (double)elapsed * 1e-6, light_traveled_meters / 1000);
        fflush(stdout);
    }
    printf("TOTAL SCHEDULER LOOPS: %ld\nTOTAL TIME: %6.3f ms\n", mj_loop_count, (double)total * 1e-6);
    printf("AVERAGE LOOP TIME: %6.6ld ns\n", total / mj_loop_count);
}

/*
    CREATE / DESTROY
*/

mj_scheduler* mj_scheduler_create() {
    mj_scheduler* shed = calloc(1, sizeof(*shed));
    if (shed == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    return shed;
}

int mj_scheduler_destroy(mj_scheduler* shed) {
    if (shed == NULL) {
        errno = EINVAL;
        return -1;
    }

    // TODO uncomment when task creation works
    // null all task pointers
    // while (shed->task_count) { mj_scheduler_remove_task(shed->task_list[shed->task_count - 1]); }

    free(shed);
    return 0;
}

/*
    TASK MANAGMENT
*/
// add task to scheduler. Mallocs a new task to contain task_fn and state pointers.
int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task, void* state) {
    // Scheduler is full
    if (scheduler->task_count >= MAX_TASKS) {
        printf("Task list is full, can not add more tasks. (%zu)\n,", scheduler->task_count);
        return -1;
    }

    // Create the new task
    mj_task* new_task = malloc(sizeof(*new_task));
    new_task->task = task;
    new_task->state = state;

    // Find empty spot in task list
    for (int i = 0; i < MAX_TASKS; i++) {
        if (scheduler->task_list[i] == NULL) {
            scheduler->task_list[i] = new_task;
            scheduler->task_count++;
            return 0;
        }
    }
    printf("SHOULD NEVER GET HERE\n");
    return -1;
}

int mj_scheduler_task_remove(mj_scheduler* scheduler) {
    if (scheduler == NULL || scheduler->current_task == NULL || *scheduler->current_task == NULL) return -1;

    mj_task* task = *scheduler->current_task;

    // Free state first ( this comes from the callbacks instance )
    free(task->state);
    task->state = NULL;

    // Free the task struct itself ( this was created by majjen when task was added to scheduler )
    free(task);

    // Null out the slot in task_list
    *scheduler->current_task = NULL;

    // Null out the scheduler’s current_task pointer
    scheduler->current_task = NULL;

    // Decrement task count safely
    if (scheduler->task_count > 0) scheduler->task_count--;

    return 0;
}
