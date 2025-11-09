#pragma once

#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t, NULL
#include <stdint.h>  // fixed-width integer types (int32_t, uint8_t, etc.)
#include <stdlib.h>  // malloc, free, and general utilities

#define MAX_TASKS 5

// TODO make opaque
typedef struct mj_task mj_task;
typedef struct mj_scheduler mj_scheduler;

// task callback prototype
typedef void(mj_task_fn)(mj_scheduler* sheduler, void* user_data);

typedef struct mj_task {
    mj_task_fn* task_func;
    void* user_data;
} mj_task;

typedef struct mj_scheduler {
    mj_task* task_list[MAX_TASKS];
    mj_task** current_task; // must be pp so we dont have to search list to remove current tasks
    size_t task_count;
} mj_scheduler;

/*
    Create / destroy
 */

mj_scheduler* mj_scheduler_create();
// int mj_scheduler_destroy(mj_scheduler* shed);

/*
    task management
*/
// Start sheduler, blocks until no tasks left
void mj_scheduler_run(mj_scheduler* sheduler);

// appends a task to the end of the list ( so that newer tasks gets run after older on each loop )
int mj_scheduler_task_add(mj_scheduler* sheduler, mj_task_fn* task_fn, void* user_state);

// usable from withing task func, removes current task
int mj_scheduler_task_remove(mj_scheduler* scheduler);
