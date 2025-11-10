#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_TASKS 5000       // sets the main task array size
#define ITERATION_SLEEP_MS 8 // just used when testing so printf isnt so fast

// Forward declarations of opaque structs
typedef struct mj_task mj_task;
typedef struct mj_scheduler mj_scheduler;

// Task callback prototype
typedef void(mj_task_fn)(mj_scheduler* scheduler, void* state);

/*
    Create / Destroy
*/
// Create a scheduler instance
mj_scheduler* mj_scheduler_create();
// int mj_scheduler_destroy(mj_scheduler* scheduler);

/*
    Scheduler management
*/
// Start scheduler (blocks). Returns when no tasks are left.
void mj_scheduler_run(mj_scheduler* scheduler);

/*
    Task management
*/
// Append a task to the end of the list (newer tasks run after older ones each loop).
int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task_fn, void* user_state);

// Usable from within a task callback; removes the current task.
int mj_scheduler_task_remove(mj_scheduler* scheduler);