/*  demo_task.c   */
#include "demo_task.h"
#include "sleep_ms.h"
#include <stdio.h>
#include <time.h>

// In a real function this would be private ( opaque pattern )
static void demo_task_run(mj_scheduler* scheduler, void* ctx) {
    demo_task_ctx* current_context = (demo_task_ctx*)ctx;

    // Base case. Check if done.
    if (current_context->count >= current_context->count_to) {
        printf("Counting to %d (%d) DONE, removing self\n", current_context->count_to, current_context->count);
        mj_scheduler_task_remove_current(scheduler);
        return;
    }

    current_context->count++;
    printf("Counting to %d (%d)\n", current_context->count_to, current_context->count);

    sleep_ms(250); // external helper function for sleep
}

/* ------------------------------------------------------------------
 * Public helper – builds a mj_task that the scheduler can use.
 *
 * The returned pointer is owned by the caller; it should be passed
 * to `mj_scheduler_task_add`. The Scheduler frees the memory when
 * the task is removed.
 * ------------------------------------------------------------------ */
mj_task* demo_task_counter_create_task(int count_to) {
    // Allocate the container that holds the function pointers
    // This gets freed by the scheduler after it runs the tasks destroy()
    mj_task* new_task = calloc(1, sizeof(*new_task));

    // Create the context
    demo_task_ctx* ctx = malloc(sizeof(*ctx));
    ctx->count_to = count_to;
    ctx->count = 0; // always start count at zero

    // This could get allocated here or in a create func.
    // But it must be destroyed in a cleanup func.
    ctx->unused_heap_ptr = NULL;

    // set the context
    new_task->ctx = ctx;

    // set up the tasks functions, only run is mandatory
    new_task->create = NULL;
    new_task->run = demo_task_run;
    new_task->cleanup = NULL;

    printf("ADDED COUNTER COUNTING TO: %d\n", ctx->count_to);

    return new_task;
}
