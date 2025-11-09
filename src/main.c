#include "majjen.h"
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h> // size_t, NULL
#include <stdint.h> // fixed-width integers
#include <stdio.h>

void task_1(mj_scheduler* sheduler, void* user_state) {
    int* task_state = (int*)user_state;
    (*task_state)++;
    printf("\t\tTask_1 state: %d\n", *task_state);

    if (*task_state == 15) {
        printf("\t\tTask 1 done. Removing task.\n\n");
        mj_sheduler_task_remove(sheduler);
    }
}

void task_2(mj_scheduler* sheduler, void* user_state) {
    int* task_state = (int*)user_state;
    (*task_state)++;
    printf("\t\t\t\tTask_2 state: %d\n", *task_state);

    if (*task_state == 95) {
        printf("\t\t\t\tTask 2 done. Removing task.\n\n");
        mj_sheduler_task_remove(sheduler);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Creating sheduler
    mj_scheduler* sheduler = mj_scheduler_create();
    if (sheduler == NULL) {
        perror("mj_scheduler_create");
        return EXIT_FAILURE;
    }

    // setup state for tasks 1
    int* task_1_state = malloc(sizeof(*task_1_state));
    *task_1_state = 10;

    // setup state for tasks 2
    int* task_2_state = malloc(sizeof(*task_2_state));
    *task_2_state = 100;

    // add tasks
    mj_sheduler_task_add(sheduler, task_1, task_1_state);
    mj_sheduler_task_add(sheduler, task_2, task_2_state);

    // run tasks, blocks until task list is empty
    mj_scheduler_run(sheduler);
    /*
        // destroy does not check if tasks are left
        int res = mj_scheduler_destroy(sheduler);
        if (res == 0) {
            return EXIT_SUCCESS;
        } else {
            perror("mj_scheduler_destroy");
            return EXIT_FAILURE;
        } */
}