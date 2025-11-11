#include "majjen.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
        //  remove this task from scheduler
        mj_scheduler_task_remove_current(scheduler);
        return;
    }

    // keep counting
    (*task_state)++;
}

/*
    TASK SETUP
    Inits and mallocs state then sends state and callback function to scheduler.
    This can also be done in the callback itself by conditionally malloc:ing if state == NULL.
    WARNING:
    Risk of dangling pointer. Dont create the state pointer outside of a dedicated setup function or inside the task itself.
    We want to create the pointer in a scope that is destroyed after task is added.
*/
void mj_add_task_count_up(mj_scheduler* scheduler, int count) {
    // Instance state is created here
    int* state = malloc(sizeof(*state));
    *state = count;

    // State and task is sent to scheduler. State is automatically freed and NULL:ed when scheduler removes task.
    mj_scheduler_task_add(scheduler, mj_cb_count_to_ten, state);
}

int main() {
    srand((unsigned)time(NULL)); // seed random nr generator

    // Create scheduler
    mj_scheduler* scheduler = mj_scheduler_create();

    // check for errno
    if (scheduler == NULL) {
        perror("mj_scheduler_create");
        return EXIT_FAILURE;
    }

    // add maximum amount of tasks
    for (int i = 0; i < MAX_TASKS; i++) { mj_add_task_count_up(scheduler, rand_range(5, 10)); }

    // run tasks, blocks until task list is empty
    mj_scheduler_run(scheduler);

    // free resources and set pointer to NULL
    mj_scheduler_destroy(&scheduler);

    return 0;
}
