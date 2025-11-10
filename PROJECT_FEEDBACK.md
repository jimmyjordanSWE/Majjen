# Majjen Project Feedback
**Date:** November 10, 2025  
**Project:** Majjen - A Generic Cooperative Scheduler in C99

---

## Executive Summary

Majjen is a well-conceived cooperative scheduler with a clear vision inspired by production systems like libuv. The project demonstrates solid understanding of event loop architecture and task scheduling fundamentals. The current implementation is functional and serves as a good proof-of-concept, but there are significant discrepancies between the documented API (in README) and the actual implementation that need to be addressed.

**Status:** Early development (Heavy WIP as stated)  
**Build Status:** ✅ Compiles cleanly with gcc/C99  
**Runtime Status:** ✅ Executes successfully

---

## Critical Issues

### 1. **API Mismatch Between Documentation and Implementation** ⚠️ HIGH PRIORITY

The README.md describes an API that doesn't exist in the current codebase:

**Documented API (README.md):**
```c
mj_task* mj_create_task(mj_scheduler* scheduler, mj_task_func func, void* state);
void mj_task_exit(void);
void mj_task_sleep_ns(uint64_t ns);
void mj_task_yield(void);
```

**Actual API (majjen.h):**
```c
typedef void(mj_task_fn)(mj_scheduler* scheduler, void* state);
int mj_scheduler_task_add(mj_scheduler* scheduler, mj_task_fn* task_fn, void* user_state);
int mj_scheduler_task_remove(mj_scheduler* scheduler);
```

**Impact:** This creates confusion for potential users. The documented API is actually more intuitive and aligned with the design goals.

**Recommendation:** Either:
- Update README.md to match the current implementation, OR
- Implement the API as documented (preferred, as it's cleaner)

### 2. **Missing Core Features from Design Document**

The design document (`notes/design_document.md`) outlines several features not yet implemented:

- ❌ `mj_wait_func` hook for custom I/O polling
- ❌ Timer-based task sleeping (only busy-wait loop exists)
- ❌ Task state types (MJ_RUNNABLE, MJ_WAIT_TIMER, etc.)
- ❌ Min-heap for efficient timer scheduling
- ❌ Tracing/debugging hooks

**Current State:** The implementation uses a fixed `ITERATION_SLEEP_MS` delay between iterations, which is functional but doesn't provide per-task sleep capabilities.

### 3. **Incomplete Resource Management**

**In `main.c` line 58:**
```c
// TODO move this to proper destroy function in majjen.c
free(scheduler);
```

**In `majjen.h` line 16:**
```c
// int mj_scheduler_destroy(mj_scheduler* shed);  // Commented out
```

**Issue:** The destroy function is implemented but not exposed in the header. This creates:
- Memory leak potential if tasks are still allocated
- Incomplete cleanup path
- Commented-out API in header file

**Fix Required:**
```c
// In majjen.h - uncomment this
int mj_scheduler_destroy(mj_scheduler* shed);

// In main.c - replace free(scheduler) with
mj_scheduler_destroy(scheduler);
```

---

## Code Quality & Design

### Strengths ✅

1. **Clean Separation of Concerns**
   - Scheduler manages `mj_task` lifecycle
   - User maintains ownership of `user_state`
   - Clear boundary between library and user code

2. **Good Coding Practices**
   - Consistent naming conventions (`mj_` prefix)
   - Useful debug logging with `LOG()` macro
   - Memory safety checks (NULL pointer validation)
   - Proper use of `calloc()` for initialization

3. **Build System**
   - Well-structured Makefile with automatic dependency generation
   - Clean output with progress messages
   - Proper PHONY targets
   - Debug symbols enabled (-g flag)

4. **Documentation Mindset**
   - Comprehensive design document
   - Thoughtful README with usage examples
   - Inline comments explaining design decisions

5. **Development Environment**
   - Proper `.gitignore` for C projects
   - VS Code configuration set up correctly
   - Clang-format configured for consistent style

### Areas for Improvement 🔧

#### 1. **Memory Safety & Error Handling**

**Current Issue in `majjen.c:85`:**
```c
// Create the new task
mj_task* new_task = malloc(sizeof(*new_task));
new_task->task = task;  // No NULL check!
```

**Risk:** If `malloc()` fails, dereferencing `new_task` causes undefined behavior.

**Recommended Fix:**
```c
mj_task* new_task = malloc(sizeof(*new_task));
if (new_task == NULL) {
    errno = ENOMEM;
    return -1;
}
new_task->task = task;
new_task->state = state;
```

#### 2. **Type Safety & Consistency**

**Issue:** Loop counter type mismatch in `majjen.c:31-34`
```c
for (int i = MAX_TASKS - 1; i >= 0; i--) {  // 'int i'
```

But `task_count` is `size_t`. This creates potential signed/unsigned comparison issues.

**Better Approach:**
```c
for (size_t i = 0; i < MAX_TASKS; i++) {
    if (scheduler->task_list[i] == NULL) continue;
    // ... run task
}
```

Or if reverse iteration is intentional:
```c
for (int i = (int)MAX_TASKS - 1; i >= 0; i--) {
```

#### 3. **Hard-Coded Limits**

**In `majjen.h`:**
```c
#define MAX_TASKS 10
```

**Concerns:**
- Arbitrary limit without justification
- No dynamic resizing capability
- Could easily become a bottleneck

**Recommendations:**
- Document why 10 is chosen (embedded system constraint?)
- Consider making this configurable at scheduler creation
- Or implement dynamic array resizing (though this contradicts embedded use-case)

#### 4. **Unused/Premature Code**

**In `main.c:8-10`:**
```c
// utility to make a random int in a range
int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}
```

This is only used for testing. Consider:
- Moving to a separate `test/` or `examples/` directory
- Or clearly marking test-only code

#### 5. **Platform-Specific Code Without Guards**

**In `majjen.c:5`:**
```c
#define _POSIX_C_SOURCE 199309L
```

This limits portability to POSIX systems. Consider:
```c
#if defined(__unix__) || defined(__APPLE__)
    #define _POSIX_C_SOURCE 199309L
#endif
```

And provide Windows alternatives for `nanosleep()`.

---

## Architectural Observations

### Current Architecture vs. Documented Vision

**Current State (v0.1):**
- Simple round-robin task execution
- Fixed-interval iteration sleep
- Direct task removal from within task callback
- Flat array-based task storage

**Documented Vision (design_document.md):**
- Event-driven architecture with `mj_wait_func` hook
- Per-task timer support with efficient wake scheduling
- Integration points for I/O systems (epoll, kqueue)
- State machine for tasks (RUNNABLE, WAITING, etc.)

### The Good News

Your architecture is **fundamentally sound**. The `mj_wait_func` design pattern you've outlined in the notes is exactly how production schedulers work. You've correctly identified:

1. The scheduler should be I/O-agnostic
2. Blocking on OS-level pollers achieves true idle (0% CPU)
3. Timer queues need efficient data structures (min-heap)
4. The event loop pattern mirrors Node.js/libuv

### The Gap

There's a significant implementation gap between the current code and the documented architecture. This is normal for early-stage projects, but it creates confusion.

**Suggestion:** Consider versioning your documentation:
- `README_v0.1.md` - Documents current simple implementation
- `README_future.md` or keep design_document.md - Documents target architecture

---

## Specific Code Review

### majjen.c

**Lines 18-27: Scheduler Run Loop**
```c
if (scheduler == NULL) {
    printf("scheduler is NULL\n");
    return;
}
if (scheduler->task_count == 0) {
    printf("Cant run with zero tasks.\n");
    return;
}
```

**Issues:**
- Uses `printf()` instead of `fprintf(stderr, ...)` for error messages
- "Cant" should be "Can't" (minor)
- No distinction between error types for caller

**Better Approach:**
```c
if (scheduler == NULL) {
    fprintf(stderr, "Error: scheduler is NULL\n");
    errno = EINVAL;
    return;
}
if (scheduler->task_count == 0) {
    fprintf(stderr, "Warning: Can't run with zero tasks\n");
    return;
}
```

**Lines 37-38: Sleep Between Iterations**
```c
struct timespec ts = {.tv_sec = 0, .tv_nsec = ITERATION_SLEEP_MS * 1000 * 1000};
nanosleep(&ts, NULL);
```

**Observations:**
- This creates a 200ms delay between each task execution
- Good: Prevents CPU spinning
- Bad: Adds unnecessary latency (all tasks wait 200ms even if they want to run immediately)

**Future Improvement:** Implement per-task sleep with `mj_task_sleep_ns()` as documented.

**Lines 65-68: Destroy Function**
```c
int mj_scheduler_destroy(mj_scheduler* shed) {
    if (shed == NULL) {
        errno = EINVAL;
        return -1;
    }
```

**Issue:** Parameter named `shed` instead of `scheduler`. While functionally fine, inconsistency with rest of codebase reduces readability.

**Line 72: TODO Comment**
```c
// TODO uncomment when task creation works
// null all task pointers
// while (shed->task_count) { mj_scheduler_remove_task(shed->task_list[shed->task_count - 1]); }
```

**Critical Issue:** This TODO suggests task cleanup doesn't work, but the function is called. This creates a memory leak if tasks exist at shutdown.

**Action Required:** Either implement proper cleanup or document that users must remove all tasks before calling destroy.

### majjen.h

**Line 12:**
```c
#define ITERATION_SLEEP_MS 200
```

This is an implementation detail exposed in the public header. Consider moving to `.c` file or making it configurable.

**Lines 10-11:**
```c
typedef struct mj_task mj_task;
typedef struct mj_scheduler mj_scheduler;
```

Good use of opaque pointers! This properly encapsulates internal structure.

### main.c

**Lines 15-37: Task Callback**
```c
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
```

**Observations:**
- Clean, simple demonstration
- Properly frees state before exit
- Good use of LOG macro

**Note:** The naming convention (`mj_cb_` for callbacks) is good but might not need the `mj_` prefix since it's user code.

**Lines 53-54:**
```c
// add maximum amount of tasks
for (int i = 0; i < MAX_TASKS; i++) { mj_add_task_count_up(scheduler, rand_range(-10, 10)); }
```

**Readability Issue:** Single-line loop is hard to scan. Consider:
```c
// Add maximum amount of tasks
for (int i = 0; i < MAX_TASKS; i++) {
    mj_add_task_count_up(scheduler, rand_range(-10, 10));
}
```

---

## Testing & Validation

### Current Test Coverage

**What Works:**
- ✅ Scheduler creation
- ✅ Task addition (up to MAX_TASKS limit)
- ✅ Task execution in sequence
- ✅ Task removal from within callback
- ✅ Proper loop termination when all tasks exit
- ✅ Memory cleanup (partial)

**What's Not Tested:**
- ❌ Error conditions (malloc failure, NULL pointers)
- ❌ Edge cases (removing tasks from empty scheduler, adding beyond limit)
- ❌ Concurrent task interactions
- ❌ Timer/sleep functionality (not implemented)
- ❌ Proper memory cleanup on destroy

### Recommendations

1. **Add a test suite:**
   ```
   tests/
   ├── test_basic_scheduler.c
   ├── test_task_lifecycle.c
   ├── test_error_conditions.c
   └── test_memory_leaks.c
   ```

2. **Use assertions:**
   ```c
   #include <assert.h>
   assert(scheduler != NULL);
   assert(mj_scheduler_task_add(scheduler, task, state) == 0);
   ```

3. **Memory leak detection:**
   - Run with `valgrind ./app`
   - Add sanitizers: `-fsanitize=address -fsanitize=undefined`

---

## Documentation Issues

### README.md

**Strengths:**
- Clear explanation of cooperative scheduling concept
- Good code examples
- Explains ownership model well

**Issues:**

1. **API Examples Don't Match Implementation**
   - Example shows `mj_create_task()` which doesn't exist
   - Shows `mj_task_exit()` which isn't implemented
   - Shows `mj_task_sleep_ns()` which isn't available

2. **Missing Information:**
   - No build instructions
   - No dependencies listed (currently none, which is good!)
   - No platform compatibility info
   - No license information

3. **Incomplete Example:**
   ```c
   void my_periodic_task(void* user_state) {  // Wrong signature!
   ```
   
   Should be:
   ```c
   void my_periodic_task(mj_scheduler* scheduler, void* user_state) {
   ```

**Recommendations:**
- Add "Current Status" section explaining what's implemented vs. planned
- Include actual working example from `main.c`
- Add build instructions: `make && ./app`

### design_document.md

**Strengths:**
- Excellent architectural thinking
- Shows understanding of production systems
- References to libuv are helpful

**Issues:**
- Not clear this is "future vision" vs. current state
- Some suggestions contradict each other (blocking vs. nanosleep)

**Suggestion:** Add section headers:
```markdown
## Current Architecture (v0.1)
## Planned Architecture (v1.0)
## Future Enhancements (v2.0+)
```

---

## Performance Considerations

### Current Performance Profile

**Strengths:**
- Simple, predictable execution model
- Low overhead (no complex data structures)
- Suitable for small task counts

**Limitations:**
- O(n) task iteration every cycle
- Fixed 200ms sleep between cycles adds latency
- No prioritization or load balancing
- Array-based storage limits scalability

### Scaling Concerns

At current MAX_TASKS = 10:
- **Acceptable** for embedded systems
- **Acceptable** for simple applications

If increased to 1000+ tasks:
- Array iteration becomes expensive
- Consider linked list or priority queue
- NULL slot checking adds overhead

### Future Optimization Opportunities

1. **Skip NULL slots more efficiently:**
   ```c
   // Instead of iterating all MAX_TASKS, maintain a count
   size_t active_tasks = 0;
   for (size_t i = 0; i < MAX_TASKS && active_tasks < scheduler->task_count; i++) {
       if (scheduler->task_list[i] != NULL) {
           active_tasks++;
           // run task
       }
   }
   ```

2. **Implement timer min-heap** (as planned in design doc)

3. **Batch task additions/removals** to reduce array manipulation

---

## Build System & Tooling

### Makefile Analysis

**Strengths:**
- Clean, well-organized
- Automatic dependency generation with `-MMD -MP`
- Proper directory creation
- Good use of PHONY targets

**Observations:**

**Line 2: Compiler Flags**
```makefile
CFLAGS := -g -Wall -Wextra -std=c99 -Iinclude -Isrc/libs -I. -MMD -MP -Wno-unused-parameter -Wno-unused-function -Wno-format-truncation
```

**Good:**
- `-Wall -Wextra` enables comprehensive warnings
- `-std=c99` ensures standard compliance
- `-g` includes debug symbols

**Concerning:**
- `-Wno-unused-parameter` and `-Wno-unused-function` suppress potentially useful warnings
- These suppressions might hide actual issues

**Suggestion:** Remove these suppressions and fix the warnings instead. Unused parameters can be marked:
```c
void my_func(int used, int unused __attribute__((unused))) {
    // ...
}
```

Or in C:
```c
void my_func(int used, int unused) {
    (void)unused;  // Explicitly mark as intentionally unused
    // ...
}
```

**Line 2: Include Paths**
```makefile
-Iinclude -Isrc/libs -I.
```

**Issue:** References `-Iinclude` but no `include/` directory exists in the project.

**Fix:** Either remove `-Iinclude` or create the directory for future use.

### Development Tools

**Clang-format Configuration:**
- ✅ Well-configured
- ✅ Consistent style (LLVM-based)
- ✅ Appropriate for C99
- Note: `ColumnLimit: 160` is quite wide; 100-120 is more common

**VS Code Configuration:**
- ✅ Format on save enabled
- ✅ C/C++ IntelliSense configured
- ⚠️ No launch configuration for debugging visible

**Git Configuration:**
- ✅ Comprehensive `.gitignore`
- ✅ Properly excludes build artifacts
- ✅ Includes VS Code debug files (*.dwo)

---

## Security & Safety

### Memory Safety

**Current State:**
- Mostly safe with some gaps (missing malloc checks)
- User responsible for `user_state` lifetime
- Clear ownership boundaries

**Potential Issues:**

1. **Use-After-Free Risk:**
   If a task callback tries to access scheduler state after calling `mj_scheduler_task_remove()`, behavior is undefined.
   
   **Current Code (main.c:30):**
   ```c
   mj_scheduler_task_remove(scheduler);
   return;  // Good! Returns immediately
   ```
   
   This is handled correctly in the example.

2. **Dangling Pointer in `current_task`:**
   The `current_task` double pointer is set to NULL after task removal, which is good. But there's a window where it points to freed memory.

3. **No Protection Against:**
   - Tasks removing other tasks
   - Tasks modifying scheduler state directly
   - Recursive scheduler_run calls

### Integer Overflow

**In `main.c:10`:**
```c
int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}
```

**Issue:** If `max < min`, this causes integer underflow.

**Better:**
```c
int rand_range(int min, int max) {
    if (max < min) {
        int temp = min;
        min = max;
        max = temp;
    }
    return min + rand() % (max - min + 1);
}
```

### Thread Safety

**Current State:** ⚠️ Not thread-safe

**Issues if used in multi-threaded context:**
- `current_task` is a shared mutable pointer
- `task_list` modifications not atomic
- `task_count` updates not synchronized

**Recommendation:** Document that scheduler is **single-threaded only** or add mutex protection.

---

## Portability

### Current Portability

**Platform Dependencies:**
- `_POSIX_C_SOURCE 199309L` - Requires POSIX.1b (1993)
- `nanosleep()` - POSIX-only, not available on Windows
- `<time.h>` structures - POSIX timespec

**Compiler Assumptions:**
- GCC-specific warning flags
- Assumes 64-bit target (though code should work on 32-bit)

### Portability Recommendations

1. **Add Windows Support:**
   ```c
   #ifdef _WIN32
       #include <windows.h>
       #define nanosleep(ts, rem) Sleep((ts)->tv_sec * 1000 + (ts)->tv_nsec / 1000000)
   #else
       #include <time.h>
   #endif
   ```

2. **Add Compiler Detection:**
   ```c
   #if defined(__GNUC__) || defined(__clang__)
       #define UNUSED __attribute__((unused))
   #else
       #define UNUSED
   #endif
   ```

3. **Document Supported Platforms:**
   ```markdown
   ## Platform Support
   - ✅ Linux (tested)
   - ✅ macOS (should work, untested)
   - ❌ Windows (requires porting nanosleep)
   - ✅ Any POSIX.1b compliant system
   ```

---

## Licensing & Legal

**Current State:** ⚠️ No license file present

**Impact:**
- By default, code is "all rights reserved"
- Others cannot legally use, modify, or distribute
- Unclear legal status for contributions

**Recommendations:**

Choose an appropriate license:
- **MIT/BSD:** Permissive, allows commercial use
- **GPL:** Copyleft, requires derivative works to be open source
- **Apache 2.0:** Permissive with patent grant
- **Public Domain (Unlicense):** Maximum freedom

Add `LICENSE` file to project root.

---

## Project Organization

### Current Structure
```
Majjen/
├── src/
│   ├── main.c          (Example/test code)
│   └── libs/
│       ├── majjen.c    (Implementation)
│       └── majjen.h    (Public API)
├── build/              (Build artifacts)
├── notes/              (Design docs)
├── Makefile
└── README.md
```

### Suggested Improvements

1. **Separate Examples from Core:**
   ```
   examples/
   ├── basic_counter.c       (current main.c)
   ├── timer_example.c
   └── io_integration.c
   ```

2. **Add Tests Directory:**
   ```
   tests/
   ├── test_runner.c
   ├── unit_tests.c
   └── integration_tests.c
   ```

3. **Include Directory for Public Headers:**
   ```
   include/
   └── majjen/
       └── majjen.h      (Public API only)
   
   src/
   └── majjen_internal.h  (Private/internal definitions)
   ```

4. **Documentation Structure:**
   ```
   docs/
   ├── API.md            (Public API reference)
   ├── DESIGN.md         (Move design_document.md here)
   ├── CONTRIBUTING.md   (If accepting contributions)
   └── CHANGELOG.md      (Version history)
   ```

---

## Roadmap Suggestions

Based on your design document and current code, here's a suggested implementation roadmap:

### Phase 1: Foundation (Current → v0.2)
- [ ] Fix API documentation mismatch
- [ ] Implement proper `mj_scheduler_destroy()`
- [ ] Add error checking for all malloc calls
- [ ] Fix signed/unsigned type issues
- [ ] Add basic unit tests
- [ ] Document current API accurately

### Phase 2: Core Features (v0.2 → v0.5)
- [ ] Implement task state machine (RUNNABLE, WAITING, etc.)
- [ ] Add `mj_task_sleep_ns()` functionality
- [ ] Implement `mj_task_yield()`
- [ ] Add `mj_task_exit()` convenience function
- [ ] Remove hardcoded ITERATION_SLEEP_MS
- [ ] Implement min-heap for timer scheduling

### Phase 3: Advanced Features (v0.5 → v1.0)
- [ ] Implement `mj_wait_func` hook
- [ ] Provide reference epoll integration
- [ ] Provide reference nanosleep fallback
- [ ] Add task priority support
- [ ] Implement dynamic task limit (remove MAX_TASKS)
- [ ] Add comprehensive error codes

### Phase 4: Production Ready (v1.0+)
- [ ] Full test coverage (unit + integration)
- [ ] Performance benchmarks
- [ ] Memory leak auditing (valgrind clean)
- [ ] Thread-safety options
- [ ] Cross-platform support (Windows, macOS, Linux)
- [ ] API stability guarantees

---

## Positive Highlights

It's important to recognize what's going well:

1. **✅ Clear Vision:** Your design documents show you understand the problem space deeply. The comparison to libuv/Node.js demonstrates research and good architectural thinking.

2. **✅ Clean C Code:** The code is readable, follows conventions, and demonstrates good C practices (opaque types, proper error codes, etc.).

3. **✅ Functional Prototype:** The current implementation works and successfully demonstrates the core concept of cooperative scheduling.

4. **✅ Good Development Environment:** Build system, formatting, and tooling are all properly configured.

5. **✅ Educational Value:** The project structure and documentation make it easy to understand cooperative scheduling concepts.

6. **✅ Realistic Scope:** You've correctly identified this as "heavy WIP" and haven't over-promised features.

---

## Conclusion

Majjen is a promising project with solid foundations and clear direction. The main challenges are:

1. **Closing the gap** between documented vision and current implementation
2. **Completing resource management** (destroy function, cleanup)
3. **Adding missing features** from the design document (timers, wait hooks)
4. **Improving error handling** and edge case coverage

The architecture you've outlined in the design document is sound and mirrors production systems. The current implementation serves as a good learning tool and prototype. With focused effort on the roadmap above, this could become a useful library for embedded systems and educational purposes.

### Priority Actions (Immediate)

1. Fix `mj_scheduler_destroy()` exposure and usage
2. Add malloc error checking
3. Update README.md to match current API OR implement the documented API
4. Run valgrind to check for memory leaks
5. Add tests for basic functionality

### Long-term Direction

Continue towards the `mj_wait_func` architecture outlined in your design document. This is the right path and will make Majjen truly generic and useful for real-world applications.

---

## Additional Resources

If you're continuing development, consider studying:

- **libuv source code:** Your notes already reference this - excellent resource
- **FreeRTOS:** Another cooperative scheduler for embedded systems
- **Coroutines in C:** Protothreads, Boost.Context for advanced scheduling
- **Event-driven servers:** nginx, Redis for real-world event loop patterns

---

**Overall Assessment:** 7/10
- Strong concept and direction
- Functional core implementation
- Needs work on completeness and documentation accuracy
- High potential for becoming a useful library

Good luck with continued development! 🚀
