#include "majjen.h"
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h> // size_t, NULL
#include <stdint.h> // fixed-width integers
#include <stdio.h>

#define LOG(fmt, ...) fprintf(stderr, "[%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

void increment(mj_scheduler* scheduler, void* user_state) {
    int* task_state = (int*)user_state;
    if (*task_state >= 12) {
        LOG("State: %d. TASK DONE, REMOVING.", *task_state);
        mj_scheduler_task_remove(scheduler);
        return;
    }
    LOG("State: %d", *task_state);
    (*task_state)++;
}

void decrement(mj_scheduler* scheduler, void* user_state) {
    int* task_state = (int*)user_state;
    if (*task_state <= 96) {
        LOG("State: %d. TASK DONE, REMOVING.", *task_state);
        mj_scheduler_task_remove(scheduler);
        return;
    }
    LOG("State: %d", *task_state);
    (*task_state)--;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Creating scheduler
    mj_scheduler* scheduler = mj_scheduler_create();
    if (scheduler == NULL) {
        perror("mj_scheduler_create");
        return EXIT_FAILURE;
    }

    ////////////////
    // TODO Dont allocate in main, the task functions must allocate and free their state themselves. They "save" their state in the scheduler anyways
    /////////////

    // setup state for tasks 1
    int* task_1_state = malloc(sizeof(*task_1_state));
    *task_1_state = 10;

    // setup state for tasks 2
    int* task_2_state = malloc(sizeof(*task_2_state));
    *task_2_state = 100;

    // setup state for tasks 3
    int* task_3_state = malloc(sizeof(*task_3_state));
    *task_3_state = 1000;

    // add tasks
    mj_scheduler_task_add(scheduler, increment, task_1_state);
    mj_scheduler_task_add(scheduler, decrement, task_2_state);
    mj_scheduler_task_add(scheduler, increment, task_3_state);

    // run tasks, blocks until task list is empty
    mj_scheduler_run(scheduler);

    // Note that state is still here.
    // But we only ever end up here at app shutdown.
    // so we must free() state when we remove tasks.
    printf("\nremaning state 1: %d", *task_1_state);
    printf("\nremaning state 2: %d", *task_2_state);
    printf("\nremaning state 3: %d\n", *task_3_state);
}