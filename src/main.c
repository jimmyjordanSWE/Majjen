#include "demo_task.h"
#include "majjen.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    // Create scheduler
    mj_scheduler* scheduler = mj_scheduler_create();

    mj_scheduler_task_add(scheduler, demo_task_counter_create_task(4));
    mj_scheduler_task_add(scheduler, demo_task_counter_create_task(3));
    mj_scheduler_task_add(scheduler, demo_task_counter_create_task(2));

    // run tasks, blocks until task list is empty
    mj_scheduler_run(scheduler);

    // free resources and set pointer to NULL
    mj_scheduler_destroy(&scheduler);

    return 0;
}
