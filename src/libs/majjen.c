
/* expose nanosleep / struct timespec on POSIX */
#define _POSIX_C_SOURCE 199309L

#include "majjen.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>

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
    // there are tasks, run them
    while (scheduler->task_count > 0) {
        mj_loop_count++;
        printf("\nRUNNING TASKS LOOP NR: %zu\n", mj_loop_count);
        // note runs oldest task first, change for linked list
        for (int i = MAX_TASKS - 1; i >= 0; i--) {
            // Skipto next if no task in slot
            if (scheduler->task_list[i] == NULL) { continue; }

            // get the function and the state
            mj_task_fn* task = scheduler->task_list[i]->task_func;
            void* user_data = scheduler->task_list[i]->user_data;

            /* sleep 50 ms */
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
            nanosleep(&ts, NULL);

            // set current function
            scheduler->current_task = &scheduler->task_list[i];

            // call user function with user state
            printf("running function at index %d: ", i);
            fflush(stdout);
            task(scheduler, user_data);

            // reset current function, should only be availible from the task that just ran
            scheduler->current_task = NULL;
        }
    }
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

int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task_fn, void* user_state) {
    // Scheduler is full
    if (scheduler->task_count >= MAX_TASKS) {
        printf("Task list is full, can not add more tasks. (%zu)\n,", scheduler->task_count);
        return -1;
    }

    mj_task* new_task = malloc(sizeof(*new_task));
    new_task->task_func = task_fn;
    new_task->user_data = user_state;

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

    // Free user_data first
    free(task->user_data);
    task->user_data = NULL;

    // Free the task struct itself
    free(task);

    // Null out the slot in task_list
    *scheduler->current_task = NULL;

    // Null out the scheduler’s current_task pointer
    scheduler->current_task = NULL;

    // Decrement task count safely
    if (scheduler->task_count > 0) scheduler->task_count--;

    return 0;
}
