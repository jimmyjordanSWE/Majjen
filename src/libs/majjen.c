
/* expose nanosleep / struct timespec on POSIX */
#define _POSIX_C_SOURCE 199309L

#include "majjen.h"
#include <errno.h>
#include <stdio.h>
#include <time.h>

void mj_scheduler_run(mj_scheduler* scheduler) {
    if (scheduler == NULL) {
        printf("Sheduler is NULL\n");
        return;
    }
    if (scheduler->task_count == 0) {
        printf("Cant run with zero tasks.");
        return;
    }
    // there are tasks, run them
    while (scheduler->task_count > 0) {
        for (int i = scheduler->task_count - 1; i >= 0; i--) {
            // get the function and the state
            //// TODO segfault here
            mj_task_fn* task = scheduler->task_list[i]->task_func;
            void* user_data = scheduler->task_list[i]->user_data;

            if (task != NULL) {
                /* sleep 100 ms */
                struct timespec ts = {.tv_sec = 0, .tv_nsec = 100 * 1000 * 1000};
                nanosleep(&ts, NULL);

                // set current function
                scheduler->current_task = scheduler->task_list[i];

                // call user function with user state
                task(scheduler, user_data);
                printf("running function at index %d\n", i);
            }
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

int mj_sheduler_destroy(mj_scheduler* shed) {
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

int mj_sheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task_fn, void* user_state) {
    // Sheduler is full
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

int mj_sheduler_task_remove() {
    // TODO

    return 0;
}
