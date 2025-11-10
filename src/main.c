#include "majjen.h"
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h> // size_t, NULL
#include <stdint.h> // fixed-width integers
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LOG(fmt, ...) fprintf(stderr, "[%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)

// utility to make a random int in a range
int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

/*
    TASK CALLBACK
    This is run inside the scheduler. State is automatically freed by mj_scheduler_task_remove();
    Naming is not mandatory. Just think its easier if everything that goes into majjen has "mj_" prefix.
    Especially since we can pass in callbacks from anywhere in the code to it.
*/
void mj_cb_count_to_ten(mj_scheduler* scheduler, void* user_state) {
    // get  the state pointer
    int* task_state = (int*)user_state;
    // are we done?
    if (*task_state >= 10) {
        LOG("State: %d. TASK DONE. TASK REMOVED.", *task_state);
        // remove this task from scheduler
        mj_scheduler_task_remove(scheduler);
        return;
    }
    LOG("State: %d", *task_state);
    (*task_state)++;
}
/*
    TASK SETUP
    Inits and mallocs state then sends state and callback function to scheduler
*/
void mj_add_task_count_up(mj_scheduler* scheduler, int count) {
    // Instance state is created here
    int* state = malloc(sizeof(*state));
    *state = count;

    // State is sent with task to scheduler. Task MUST free state when done since instance can never return here again.
    mj_scheduler_task_add(scheduler, mj_cb_count_to_ten, state);
}

int main() {

    srand((unsigned)time(NULL));

    // Create scheduler
    mj_scheduler* scheduler = mj_scheduler_create();
    if (scheduler == NULL) {
        perror("mj_scheduler_create");
        return EXIT_FAILURE;
    }

    // add maximum amount of tasks
    for (int i = 0; i < MAX_TASKS; i++) { mj_add_task_count_up(scheduler, rand_range(-10, 10)); }

    // run tasks, blocks until task list is empty
    mj_scheduler_run(scheduler);

    // TODO move this to proper destroy funciton in majjen.c
    free(scheduler);
}
