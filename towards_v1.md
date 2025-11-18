# Majjen Code Review and Towards v1
*(GPT 5.1: loo through this project, all files, and redo the readme.md so that it is up to date and matches the project. The comments in demo_task.h are accurate.)*

This document captures a high-level code review of the current Majjen prototype and a prioritized set of improvements to move towards a stable v1.

## 1. Overall Architecture

**What works well**
- Clear separation between the scheduler core (`src/libs/majjen.c` / `majjen.h`) and demo usage (`src/demo_task.*`, `src/main.c`).
- The cooperative model and ownership rules are simple and easy to reason about: scheduler owns `mj_task` and its `ctx`; tasks remove themselves.
- The design notes under `notes/` demonstrate a solid understanding of modern event-loop architectures (libuv-style wait function, timers, metrics).
- The `Makefile` is minimal but robust: recursive source discovery, out-of-tree `build/` objects, dependency tracking with `-MMD -MP`.

**Gaps vs. the intended design**
- The production-like architecture described in `notes/IMPLEMENTATION_GUIDE.md` (timers, wait functions, state machine, metrics, logging) is not yet reflected in the implementation.
- The current scheduler uses a fixed-size array (`MAX_TASKS`) and only supports simple runnable tasks with synchronous blocking (e.g., `sleep_ms` inside the task logic).

**Suggested direction towards v1**
- Treat the current code as a minimal “v0” and start incrementally matching the design docs: add task state, a timer structure, and a wait hook while keeping the core small.

## 2. Scheduler Core (`src/libs/majjen.*`)

### 2.1 API shape and semantics

**Strengths**
- `mj_task` clearly bundles callbacks + context; this matches the intent documented in `demo_task.h`.
- Double-pointer pattern in `mj_scheduler_destroy(mj_scheduler** scheduler)` is good: prevents dangling scheduler pointers.
- Error handling uses `errno` with sensible values (`EINVAL`, `ENOMEM`, `EBUSY`).

**Issues / limitations**
- `mj_scheduler_task_add` takes an already-allocated `mj_task*` rather than exposing a helper to construct tasks. This is fine, but it puts more burden on users to follow ownership rules correctly.
- `mj_scheduler_task_remove_current` assumes that `ctx` is always heap-allocated and safe to `free`. This is correct for the demo, but the API doesn’t clearly communicate this contract.
- `MAX_TASKS` is a global compile-time limit with no way to query capacity or current number of tasks from outside.
- Scheduler state is very minimal:
  - No concept of task state (runnable/sleeping/waiting I/O).
  - No integration point for custom wait functions.
  - No metrics or logging hooks (even as no-ops).

**Suggested improvements**
- Clarify API contracts in `majjen.h`:
  - Document that `ctx` must be heap-allocated or otherwise valid to `free` by the scheduler.
  - Explicitly state that `mj_scheduler_task_remove_current` must only be called from a `run`/`cleanup` context.
- Add small introspection helpers:
  - `size_t mj_scheduler_task_count(const mj_scheduler*);`
  - `size_t mj_scheduler_capacity(const mj_scheduler*); // returns MAX_TASKS for now`
- Prepare for future state machine without breaking API:
  - Introduce an internal `mj_task_state` enum inside `majjen.c` only.
  - Keep the external API the same but let internal code start tracking more information (runnable/removed, etc.).

### 2.2 `mj_scheduler_run` loop

**Strengths**
- Simple round-robin loop is easy to follow; good for a teaching/demo implementation.
- Uses `scheduler->current_task` as a double pointer to enable O(1) removal of the currently running task.

**Issues / edge cases**
- If a task sets `run = NULL`, it is just silently skipped forever; the scheduler doesn’t remove it or treat it as an error.
- The loop is fully busy when tasks do not sleep or yield. This is expected in a minimal model, but it is worth documenting clearly.
- There is no protection against user code calling `mj_scheduler_task_remove_current` outside of `run` (you handle it with `EINVAL`, but documentation could be stronger).

**Suggested improvements**
- Consider treating `run == NULL` tasks as invalid and either:
  - Reject them in `mj_scheduler_task_add`, or
  - Remove them immediately with an error code or debug log.
- Optional but helpful: add a small assertion-style macro (compiled in debug builds) to validate invariants inside the run loop (e.g., `current_task != NULL` when expected).
- When adding timers / wait functions in the future, refactor the run loop into explicit phases as described in the notes (run runnable tasks → service timers → wait).

### 2.3 `mj_scheduler_task_remove_current`

**Strengths**
- Correctly uses `current_task` as a double pointer to update the scheduler’s array in O(1) time.
- Calls `cleanup` before freeing `ctx` and the task struct itself.

**Issues / risks**
- `cleanup` currently receives `ctx` but has no documented guarantee about what it may do (e.g., whether it can free `ctx` itself, which would double-free). This is mostly a documentation problem.
- If a `cleanup` function throws (via abort/assert) or otherwise misbehaves, the scheduler could be left in an inconsistent state. That’s acceptable for a low-level library but worth calling out.

**Suggested improvements**
- Define a clear contract in comments for `cleanup`:
  - It must not free `task` or `ctx` (that’s the scheduler’s job).
  - It may adjust external resources but should not modify scheduler internals.
- Consider supporting a mode where the scheduler does **not** free `ctx` automatically, for users who want complete control (e.g., a flag or a different add function). This can be a v1+ feature, not mandatory now.

## 3. Demo Task and Main (`src/demo_task.*`, `src/main.c`)

**What’s good**
- `demo_task.h` provides an excellent narrative description of what a task is and how the scheduler interacts with it.
- `demo_task_counter_create_task` is a clean example of how to allocate a task and its context.
- `main.c` is minimal and readable: create scheduler → add demo tasks → run → destroy.

**Minor issues / improvement ideas**
- The demo uses `printf` and `sleep_ms(250)` inside the task body, which is perfect for demonstration but not representative of a “fully non-blocking” cooperative world. This is fine as long as the README and comments keep framing it as a simple example.
- `unused_heap_ptr` is a bit confusing for readers; adding a short comment in the header explaining the idea ("example of more internal resources that should be allocated/freed in create/cleanup") would help.

**Suggested improvements**
- Add a second demo task that does not use `sleep_ms` but instead just runs a fixed number of steps and then exits. This demonstrates that the scheduler works even without blocking calls.
- Optionally, add a simple test-style program (or just a second `main` under `examples/`) that stresses the scheduler with `MAX_TASKS` simultaneous tasks.

## 4. Utilities (`src/utils/*.h`, `src/utils/*.c`)

### 4.1 `sleep_ms.h`

**Good points**
- Correctly uses `nanosleep` with a loop to handle `EINTR` and resume sleeping.
- Declared `static inline` in a header, which is appropriate for this small utility.

**Possible tweaks**
- Consider adding a short comment warning that using `sleep_ms` inside tasks blocks the entire scheduler thread.
- If you later add timer-based sleeping in the scheduler itself, clarify when to use `sleep_ms` vs. `mj_task_sleep_*`.

### 4.2 `timer.[ch]`

**Good points**
- Uses `CLOCK_MONOTONIC_RAW` when available, which is ideal for timing.
- Clean API (`clock_timer_*` functions) with multiple time units and a formatting helper.

**Integration opportunity**
- Timer is currently unused by the scheduler. For v1, consider:
  - Using `clock_timer_t` for internal metrics (measure each task’s run time or the scheduler loop duration).
  - Or using it to drive a simple “soft” timer system as a stepping stone towards the full min-heap-based timer queue described in `notes/IMPLEMENTATION_GUIDE.md`.

## 5. Build System and Layout

**Strengths**
- Recursive `find src -name '*.c'` is simple and avoids manually maintaining a source list.
- `-MMD -MP` and inclusion of `.d` files give you correct incremental rebuilds.
- `build/` directory keeps object files out of the repo root.

**Potential improvements**
- Consider adding a minimal `test` target (even if initially just re-running `app` with different environment variables or input). This would make it easier to bolt on testing later.
- Add a `format` or `lint` target if you adopt a consistent formatting tool (`clang-format` or similar). You already have `.clang-format` in the repo.

## 6. Documentation and Notes

**Positives**
- The design documents under `notes/` are unusually detailed and show good awareness of how mature event-loop systems are structured.
- The new `README.md` correctly reflects the current state and cleanly separates "implemented" vs. "planned" behavior.

**Suggestions**
- In `majjen.h`, add a short “High-level overview” comment (3–5 lines) at the top that matches the README’s wording. This helps users who start from the header instead of the README.
- In `notes/README.md`, add a short section titled “Implementation status” that points to this `towards_v1.md` as the place to check what has been built vs. what remains.

## 7. Prioritized Roadmap Towards v1

This list focuses on incremental steps that keep the project small but significantly more capable.

1. **Document and harden current API**
   - Finalize the contracts for `ctx`, `create`, `run`, and `cleanup` in `majjen.h`.
   - Add small introspection helpers for capacity and task count.

2. **Introduce basic task state (internal-only)**
   - Add an internal `enum mj_task_state { MJ_TASK_RUNNABLE, MJ_TASK_REMOVED /* ... */ };` in `majjen.c`.
   - Use it to avoid running tasks that are already logically removed (future-proofing for timers / I/O).

3. **Integrate `clock_timer_t` for metrics (optional but valuable)**
   - Start collecting simple metrics: total number of task runs, and total time spent running tasks.
   - Expose a minimal `mj_scheduler_get_metrics(const mj_scheduler*)` API.

4. **Add a minimal sleep/yield API**
   - Implement a very simple cooperative sleep/yield mechanism without full min-heap:
     - `mj_task_yield()` could just re-enqueue the task to the next scheduler cycle.
     - `mj_task_sleep_ms()` could be implemented in terms of `sleep_ms` initially, then later moved to a timer-based approach.
   - This keeps the API stable while you improve the implementation internally.

5. **Add a pluggable wait function hook**
   - Introduce `mj_wait_fn` and `mj_scheduler_set_wait_func` as in your notes, but start with a default `nanosleep`-based implementation.
   - Don’t integrate I/O yet; just prove the structure works.

6. **Implement a simple timer queue**
   - Start with a straightforward min-heap for sleeping tasks as described in `notes/IMPLEMENTATION_GUIDE.md`.
   - Wire `mj_task_sleep_*` to move tasks into the timer heap and wake them when they expire.

7. **Add examples and tests**
   - Create an `examples/` directory with:
     - The current counter demo.
     - A periodic timer demo using the new `mj_task_sleep_ms`.
   - Add basic unit tests or integration tests (even simple assert-based C programs) to guard against regressions.

## 8. Summary

Majjen’s current code is a clean, minimal cooperative scheduler with a well-explained demo and strong design notes for future expansion. The main gap is not code quality but the distance between the implemented v0 and the rich architecture described in `notes/`.

The suggestions above aim to close that gap incrementally: solidify the current API, introduce internal task state and metrics, then layer in timers and a wait function hook. Following this path will move Majjen from a teaching prototype to a small but credible v1 runtime.
