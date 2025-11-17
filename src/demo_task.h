/* --------------------------------------------------------------------
 * demo_task.h
 *
 * Example usage:
 *   mj_task *t = demo_task_create_task();      // Create a task instance.
 *   mj_scheduler_task_add(sched, t);           // Scheduler takes ownership of `t`.
 *
 * Overview
 * --------
 * An `mj_task` is a lightweight "bundle" struct that the scheduler can execute. It contains:
 *
 *   • create  – Optional factory called once when the task is added.
 *   • run     – Mandatory; invoked repeatedly until the task removes itself.
 *   • cleanup – Optional; executed after the task has been removed.
 *   • ctx     – Opaque context that holds per‑task state.
 *
 * Design rules
 * -------------
 * 1. A task should perform only a small amount of work on each call, record its progress in `ctx`, and return
 * immediately so that other tasks can run.
 * 2. The `create` and `cleanup` callbacks are optional but useful for allocating or freeing resources that belong
 * exclusively to the task.
 *
 * Life‑cycle example
 * ------------------
 * 1. Allocate an `mj_task` via a helper function that wires up `create`, `run`, and (optionally) `cleanup`. The task
 * may be created in any manner; the scheduler only requires an `mj_task` instance.
 *
 * 2. Add it to the scheduler:
 * `mj_scheduler_task_add(sched, t);`.
 *
 * 3. The scheduler will invoke `create` once, then repeatedly call `run`. When the task has finished, it can request
 * removal via `mj_scheduler_task_remove_current()`.
 *
 * 4. After removal, the scheduler calls `cleanup` (if provided) before discarding the task structure.
 *
 * This header declares a simple demo task that counts from 1 to N and then removes itself. The implementation resides
 * in `demo_task.c`.
 * -------------------------------------------------------------------- */

#pragma once

#include "majjen.h"

/* Demo‑task data – stored in the context field of the mj_task. */
typedef struct demo_task_ctx {
    int count_to; /* When to stop counting */
    int count;
    void* unused_heap_ptr; // Any pointers here needs to be allocated and freed in the create / destroy functions
} demo_task_ctx;

mj_task* demo_task_counter_create_task(int count_to);
