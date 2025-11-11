# Majjen Implementation Guide
**Advanced Features: Timers, Logging, and Wait Functions**

---

## Table of Contents
1. [Timer System Implementation](#timer-system-implementation)
2. [Logging & Performance Metrics](#logging--performance-metrics)
3. [Wait Function Architecture](#wait-function-architecture)
4. [Integration Example](#integration-example)

---

## Timer System Implementation

### Overview

A timer system allows tasks to sleep for specific durations and wake up automatically when their time expires. This is essential for tasks that need to run periodically (e.g., every 100ms) or after a delay (e.g., retry after 5 seconds).

### Core Concepts

**The Challenge:** How do we efficiently answer: "What's the next task that needs to wake up, and when?"

**The Solution:** Use a **min-heap** data structure that keeps tasks ordered by wake time. The earliest expiry is always at the root.

### Data Structures

#### 1. Enhanced Task State

```c
typedef enum {
    MJ_TASK_RUNNABLE,    // Ready to run immediately
    MJ_TASK_SLEEPING,    // Waiting for timer to expire
    MJ_TASK_WAITING_IO,  // Waiting for I/O event (future)
    MJ_TASK_EXITED       // Task has finished
} mj_task_state;

typedef struct mj_task {
    mj_task_fn*    task_function;
    void*          user_state;
    
    // State management
    mj_task_state  state;
    uint64_t       wake_time_ns;   // When to wake (in nanoseconds since epoch or monotonic)
    
    // Heap management (for timer queue)
    size_t         heap_index;     // Position in the heap (for quick updates)
    
    // Internal linkage
    struct mj_task* next;          // For runnable queue or free list
} mj_task;
```

#### 2. Scheduler with Timer Queue

```c
typedef struct mj_scheduler {
    // Runnable queue (linked list for O(1) enqueue/dequeue)
    mj_task*   runnable_head;
    mj_task*   runnable_tail;
    size_t     runnable_count;
    
    // Timer heap (min-heap sorted by wake_time_ns)
    mj_task**  timer_heap;
    size_t     timer_count;
    size_t     timer_capacity;
    
    // Currently executing task (for context-aware APIs)
    mj_task*   current_task;
    
    // Wait function hook (explained later)
    mj_wait_fn* wait_func;
    void*       wait_data;
    
    // Metrics
    mj_metrics  metrics;
} mj_scheduler;
```

### Min-Heap Implementation

A min-heap ensures the task with the earliest wake time is always at index 0.

```c
// Min-heap operations for timer queue
// Parent of node at index i is at (i-1)/2
// Children of node at index i are at 2*i+1 and 2*i+2

static void heap_swap(mj_task** heap, size_t a, size_t b) {
    mj_task* temp = heap[a];
    heap[a] = heap[b];
    heap[b] = temp;
    
    // Update heap_index for quick lookups
    heap[a]->heap_index = a;
    heap[b]->heap_index = b;
}

static void heap_bubble_up(mj_task** heap, size_t index) {
    while (index > 0) {
        size_t parent = (index - 1) / 2;
        
        // If parent expires later, swap
        if (heap[parent]->wake_time_ns > heap[index]->wake_time_ns) {
            heap_swap(heap, parent, index);
            index = parent;
        } else {
            break;
        }
    }
}

static void heap_bubble_down(mj_task** heap, size_t count, size_t index) {
    while (true) {
        size_t smallest = index;
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;
        
        if (left < count && heap[left]->wake_time_ns < heap[smallest]->wake_time_ns) {
            smallest = left;
        }
        if (right < count && heap[right]->wake_time_ns < heap[smallest]->wake_time_ns) {
            smallest = right;
        }
        
        if (smallest != index) {
            heap_swap(heap, index, smallest);
            index = smallest;
        } else {
            break;
        }
    }
}

// Insert task into timer heap
static int timer_heap_insert(mj_scheduler* sched, mj_task* task) {
    // Resize if needed
    if (sched->timer_count >= sched->timer_capacity) {
        size_t new_cap = sched->timer_capacity * 2;
        mj_task** new_heap = realloc(sched->timer_heap, new_cap * sizeof(mj_task*));
        if (new_heap == NULL) {
            return -1;
        }
        sched->timer_heap = new_heap;
        sched->timer_capacity = new_cap;
    }
    
    // Add to end and bubble up
    size_t index = sched->timer_count++;
    sched->timer_heap[index] = task;
    task->heap_index = index;
    heap_bubble_up(sched->timer_heap, index);
    
    return 0;
}

// Remove minimum (earliest) task from heap
static mj_task* timer_heap_pop(mj_scheduler* sched) {
    if (sched->timer_count == 0) {
        return NULL;
    }
    
    mj_task* result = sched->timer_heap[0];
    
    // Move last element to root and bubble down
    sched->timer_count--;
    if (sched->timer_count > 0) {
        sched->timer_heap[0] = sched->timer_heap[sched->timer_count];
        sched->timer_heap[0]->heap_index = 0;
        heap_bubble_down(sched->timer_heap, sched->timer_count, 0);
    }
    
    return result;
}

// Peek at next timer without removing it
static mj_task* timer_heap_peek(mj_scheduler* sched) {
    return sched->timer_count > 0 ? sched->timer_heap[0] : NULL;
}
```

### Timer API Functions

#### Sleep for Nanoseconds

```c
// Put current task to sleep for a duration
void mj_task_sleep_ns(uint64_t nanoseconds) {
    // This must be called from within a task
    mj_scheduler* sched = get_current_scheduler(); // TLS or global
    if (sched == NULL || sched->current_task == NULL) {
        return; // Error: not called from task context
    }
    
    mj_task* task = sched->current_task;
    
    // Calculate wake time
    uint64_t now = get_monotonic_time_ns();
    task->wake_time_ns = now + nanoseconds;
    task->state = MJ_TASK_SLEEPING;
    
    // Insert into timer heap
    timer_heap_insert(sched, task);
}

// Convenience functions
void mj_task_sleep_ms(uint64_t milliseconds) {
    mj_task_sleep_ns(milliseconds * 1000000ULL);
}

void mj_task_sleep_s(uint64_t seconds) {
    mj_task_sleep_ns(seconds * 1000000000ULL);
}

// Yield control until next scheduler cycle (wake immediately)
void mj_task_yield(void) {
    mj_scheduler* sched = get_current_scheduler();
    if (sched == NULL || sched->current_task == NULL) {
        return;
    }
    
    mj_task* task = sched->current_task;
    task->state = MJ_TASK_RUNNABLE;
    
    // Add to back of runnable queue
    enqueue_runnable(sched, task);
}
```

#### Getting Monotonic Time

```c
#include <time.h>

// Get current time in nanoseconds (monotonic clock)
static uint64_t get_monotonic_time_ns(void) {
    struct timespec ts;
    
#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts); // Fallback
#endif
    
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
```

### Scheduler Event Loop with Timers

```c
void mj_scheduler_run(mj_scheduler* sched) {
    while (sched->runnable_count > 0 || sched->timer_count > 0) {
        
        // Phase 1: Run all immediately runnable tasks
        while (sched->runnable_count > 0) {
            mj_task* task = dequeue_runnable(sched);
            sched->current_task = task;
            
            // Execute task
            uint64_t start = get_monotonic_time_ns();
            task->task_function(sched, task->user_state);
            uint64_t end = get_monotonic_time_ns();
            
            // Update metrics
            sched->metrics.task_executions++;
            sched->metrics.total_execution_time_ns += (end - start);
            
            sched->current_task = NULL;
            
            // If task exited, free it
            if (task->state == MJ_TASK_EXITED) {
                free(task->user_state);  // User data
                free(task);              // Task struct
            }
        }
        
        // Phase 2: Check for expired timers
        uint64_t now = get_monotonic_time_ns();
        while (sched->timer_count > 0) {
            mj_task* next_timer = timer_heap_peek(sched);
            
            if (next_timer->wake_time_ns <= now) {
                // Timer expired, wake the task
                timer_heap_pop(sched);
                next_timer->state = MJ_TASK_RUNNABLE;
                enqueue_runnable(sched, next_timer);
            } else {
                // No more expired timers
                break;
            }
        }
        
        // Phase 3: Wait for next event or timer
        if (sched->runnable_count == 0 && sched->timer_count > 0) {
            // Calculate timeout until next timer
            mj_task* next_timer = timer_heap_peek(sched);
            now = get_monotonic_time_ns();
            
            uint64_t timeout_ns = 0;
            if (next_timer->wake_time_ns > now) {
                timeout_ns = next_timer->wake_time_ns - now;
            }
            
            // Call wait function (blocks until timeout or I/O event)
            if (sched->wait_func != NULL) {
                sched->wait_func(sched, timeout_ns, sched->wait_data);
            } else {
                // Default: just sleep
                struct timespec ts = {
                    .tv_sec = timeout_ns / 1000000000ULL,
                    .tv_nsec = timeout_ns % 1000000000ULL
                };
                nanosleep(&ts, NULL);
            }
        }
    }
}
```

---

## Logging & Performance Metrics

### Logging System

A good logging system should be:
- **Lightweight:** Minimal overhead when disabled
- **Configurable:** Different verbosity levels
- **Extensible:** Pluggable backends (file, syslog, custom)

#### Log Levels

```c
typedef enum {
    MJ_LOG_TRACE   = 0,  // Very detailed, for debugging internal operations
    MJ_LOG_DEBUG   = 1,  // Debugging information
    MJ_LOG_INFO    = 2,  // Informational messages
    MJ_LOG_WARN    = 3,  // Warnings
    MJ_LOG_ERROR   = 4,  // Errors
    MJ_LOG_FATAL   = 5,  // Fatal errors
    MJ_LOG_SILENT  = 6   // No logging
} mj_log_level;
```

#### Logger Configuration

```c
typedef void (*mj_log_callback)(mj_log_level level, const char* message, void* user_data);

typedef struct mj_logger {
    mj_log_level     min_level;      // Minimum level to log
    mj_log_callback  callback;       // Custom log handler
    void*            user_data;      // Passed to callback
    bool             include_time;   // Include timestamps
    bool             include_level;  // Include level string
} mj_logger;

// Global logger (or per-scheduler)
static mj_logger global_logger = {
    .min_level = MJ_LOG_INFO,
    .callback = NULL,
    .user_data = NULL,
    .include_time = true,
    .include_level = true
};

// Configure logging
void mj_log_set_level(mj_log_level level) {
    global_logger.min_level = level;
}

void mj_log_set_callback(mj_log_callback callback, void* user_data) {
    global_logger.callback = callback;
    global_logger.user_data = user_data;
}
```

#### Logging Implementation

```c
static const char* level_strings[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static void default_log_handler(mj_log_level level, const char* message, void* user_data) {
    (void)user_data;
    
    FILE* out = (level >= MJ_LOG_ERROR) ? stderr : stdout;
    
    if (global_logger.include_time) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(out, "[%s] ", time_buf);
    }
    
    if (global_logger.include_level) {
        fprintf(out, "[%s] ", level_strings[level]);
    }
    
    fprintf(out, "%s\n", message);
    fflush(out);
}

void mj_log(mj_log_level level, const char* fmt, ...) {
    if (level < global_logger.min_level) {
        return; // Below threshold
    }
    
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    mj_log_callback handler = global_logger.callback;
    if (handler == NULL) {
        handler = default_log_handler;
    }
    
    handler(level, buffer, global_logger.user_data);
}

// Convenience macros
#define MJ_TRACE(...) mj_log(MJ_LOG_TRACE, __VA_ARGS__)
#define MJ_DEBUG(...) mj_log(MJ_LOG_DEBUG, __VA_ARGS__)
#define MJ_INFO(...)  mj_log(MJ_LOG_INFO, __VA_ARGS__)
#define MJ_WARN(...)  mj_log(MJ_LOG_WARN, __VA_ARGS__)
#define MJ_ERROR(...) mj_log(MJ_LOG_ERROR, __VA_ARGS__)
#define MJ_FATAL(...) mj_log(MJ_LOG_FATAL, __VA_ARGS__)
```

#### Usage in Scheduler

```c
void mj_scheduler_run(mj_scheduler* sched) {
    MJ_INFO("Scheduler starting with %zu runnable tasks, %zu sleeping",
            sched->runnable_count, sched->timer_count);
    
    while (sched->runnable_count > 0 || sched->timer_count > 0) {
        MJ_TRACE("Loop iteration: runnable=%zu, sleeping=%zu",
                 sched->runnable_count, sched->timer_count);
        
        // ... run tasks ...
        
        if (task->state == MJ_TASK_EXITED) {
            MJ_DEBUG("Task %p exited", (void*)task);
        }
    }
    
    MJ_INFO("Scheduler stopped: all tasks completed");
}
```

### Performance Metrics

Track important runtime statistics to identify bottlenecks and monitor health.

#### Metrics Structure

```c
typedef struct mj_metrics {
    // Task execution
    uint64_t task_executions;           // Total times tasks have run
    uint64_t total_execution_time_ns;   // Cumulative task execution time
    uint64_t min_execution_time_ns;     // Fastest task execution
    uint64_t max_execution_time_ns;     // Slowest task execution
    
    // Scheduler cycles
    uint64_t scheduler_loops;           // Main loop iterations
    uint64_t total_loop_time_ns;        // Time spent in scheduler
    
    // Timers
    uint64_t timer_expirations;         // Times timers woke tasks
    uint64_t timer_cancellations;       // Times timers were cancelled
    
    // Wait function
    uint64_t wait_calls;                // Times wait function was called
    uint64_t total_wait_time_ns;        // Time spent in wait function
    uint64_t io_events;                 // I/O events received (if applicable)
    
    // Task lifecycle
    uint64_t tasks_created;             // Total tasks created
    uint64_t tasks_exited;              // Total tasks completed
    uint64_t current_tasks;             // Currently active tasks
    uint64_t peak_tasks;                // Maximum concurrent tasks
    
    // Memory
    uint64_t heap_operations;           // Timer heap operations count
    size_t   memory_allocated_bytes;    // Tracked memory usage
} mj_metrics;
```

#### Metrics API

```c
// Get current metrics snapshot
const mj_metrics* mj_scheduler_get_metrics(mj_scheduler* sched) {
    return &sched->metrics;
}

// Reset all metrics
void mj_scheduler_reset_metrics(mj_scheduler* sched) {
    memset(&sched->metrics, 0, sizeof(mj_metrics));
}

// Print metrics in human-readable format
void mj_scheduler_print_metrics(mj_scheduler* sched, FILE* out) {
    const mj_metrics* m = &sched->metrics;
    
    fprintf(out, "\n=== Majjen Scheduler Metrics ===\n");
    fprintf(out, "Tasks:\n");
    fprintf(out, "  Created:       %lu\n", m->tasks_created);
    fprintf(out, "  Exited:        %lu\n", m->tasks_exited);
    fprintf(out, "  Current:       %lu\n", m->current_tasks);
    fprintf(out, "  Peak:          %lu\n", m->peak_tasks);
    
    fprintf(out, "\nExecution:\n");
    fprintf(out, "  Total runs:    %lu\n", m->task_executions);
    
    if (m->task_executions > 0) {
        uint64_t avg_ns = m->total_execution_time_ns / m->task_executions;
        fprintf(out, "  Avg time:      %lu ns (%.3f ms)\n", avg_ns, avg_ns / 1e6);
        fprintf(out, "  Min time:      %lu ns (%.3f ms)\n", 
                m->min_execution_time_ns, m->min_execution_time_ns / 1e6);
        fprintf(out, "  Max time:      %lu ns (%.3f ms)\n", 
                m->max_execution_time_ns, m->max_execution_time_ns / 1e6);
    }
    
    fprintf(out, "\nScheduler:\n");
    fprintf(out, "  Loop cycles:   %lu\n", m->scheduler_loops);
    fprintf(out, "  Wait calls:    %lu\n", m->wait_calls);
    fprintf(out, "  I/O events:    %lu\n", m->io_events);
    
    fprintf(out, "\nTimers:\n");
    fprintf(out, "  Expirations:   %lu\n", m->timer_expirations);
    fprintf(out, "  Cancellations: %lu\n", m->timer_cancellations);
    
    fprintf(out, "================================\n\n");
}
```

#### Metrics Collection Points

```c
void mj_scheduler_run(mj_scheduler* sched) {
    uint64_t loop_start = get_monotonic_time_ns();
    
    while (sched->runnable_count > 0 || sched->timer_count > 0) {
        sched->metrics.scheduler_loops++;
        
        while (sched->runnable_count > 0) {
            mj_task* task = dequeue_runnable(sched);
            
            uint64_t task_start = get_monotonic_time_ns();
            task->task_function(sched, task->user_state);
            uint64_t task_end = get_monotonic_time_ns();
            
            uint64_t duration = task_end - task_start;
            
            // Update metrics
            sched->metrics.task_executions++;
            sched->metrics.total_execution_time_ns += duration;
            
            if (duration < sched->metrics.min_execution_time_ns || 
                sched->metrics.min_execution_time_ns == 0) {
                sched->metrics.min_execution_time_ns = duration;
            }
            
            if (duration > sched->metrics.max_execution_time_ns) {
                sched->metrics.max_execution_time_ns = duration;
            }
            
            if (task->state == MJ_TASK_EXITED) {
                sched->metrics.tasks_exited++;
                sched->metrics.current_tasks--;
            }
        }
        
        // Check timers
        while (sched->timer_count > 0) {
            mj_task* next = timer_heap_peek(sched);
            if (next->wake_time_ns <= get_monotonic_time_ns()) {
                timer_heap_pop(sched);
                sched->metrics.timer_expirations++;
                // ...
            } else {
                break;
            }
        }
        
        // Wait phase
        if (sched->runnable_count == 0 && sched->timer_count > 0) {
            uint64_t wait_start = get_monotonic_time_ns();
            sched->metrics.wait_calls++;
            
            // ... wait logic ...
            
            uint64_t wait_end = get_monotonic_time_ns();
            sched->metrics.total_wait_time_ns += (wait_end - wait_start);
        }
    }
    
    uint64_t loop_end = get_monotonic_time_ns();
    sched->metrics.total_loop_time_ns = loop_end - loop_start;
}
```

---

## Wait Function Architecture

### The Problem

When the scheduler has no runnable tasks but has sleeping tasks, it needs to **idle efficiently**:
- Don't spin (wastes CPU)
- Don't sleep blindly (might miss events)
- Wake up when: (1) a timer expires, or (2) external event occurs

### The Solution: Pluggable Wait Function

Allow users to provide a custom "wait" function that knows how to block efficiently on their I/O sources.

#### Wait Function Signature

```c
// Called when scheduler needs to wait
// timeout_ns: Maximum time to wait (0 = return immediately, UINT64_MAX = wait forever)
// user_data: Custom data provided during registration
// Returns: Number of events that occurred (0 on timeout)
typedef int (*mj_wait_fn)(mj_scheduler* scheduler, uint64_t timeout_ns, void* user_data);
```

#### Registering a Wait Function

```c
void mj_scheduler_set_wait_func(mj_scheduler* sched, mj_wait_fn func, void* user_data) {
    sched->wait_func = func;
    sched->wait_data = user_data;
}
```

### Default Wait Function (nanosleep)

Simple fallback that just sleeps - no I/O integration.

```c
static int default_wait_nanosleep(mj_scheduler* sched, uint64_t timeout_ns, void* user_data) {
    (void)sched;
    (void)user_data;
    
    if (timeout_ns == 0) {
        return 0; // Don't wait
    }
    
    struct timespec ts = {
        .tv_sec = timeout_ns / 1000000000ULL,
        .tv_nsec = timeout_ns % 1000000000ULL
    };
    
    nanosleep(&ts, NULL);
    return 0; // No events, just timeout
}
```

### Advanced: epoll Integration

For Linux systems that want network I/O integration.

#### epoll Wait Function

```c
#include <sys/epoll.h>

typedef struct mj_epoll_data {
    int epoll_fd;
    
    // Map file descriptors to tasks
    // When fd becomes ready, task gets added to runnable queue
    struct {
        int fd;
        mj_task* task;
    } fd_map[MAX_FDS];
    
    size_t fd_count;
} mj_epoll_data;

static int wait_epoll(mj_scheduler* sched, uint64_t timeout_ns, void* user_data) {
    mj_epoll_data* epoll_data = (mj_epoll_data*)user_data;
    
    // Convert nanoseconds to milliseconds for epoll_wait
    int timeout_ms;
    if (timeout_ns == UINT64_MAX) {
        timeout_ms = -1; // Wait forever
    } else if (timeout_ns == 0) {
        timeout_ms = 0;  // Return immediately
    } else {
        timeout_ms = (int)(timeout_ns / 1000000ULL);
        if (timeout_ms == 0 && timeout_ns > 0) {
            timeout_ms = 1; // Round up to at least 1ms
        }
    }
    
    struct epoll_event events[32];
    int nfds = epoll_wait(epoll_data->epoll_fd, events, 32, timeout_ms);
    
    if (nfds < 0) {
        if (errno == EINTR) {
            return 0; // Interrupted, try again
        }
        MJ_ERROR("epoll_wait failed: %s", strerror(errno));
        return -1;
    }
    
    // Wake up tasks whose fds are ready
    for (int i = 0; i < nfds; i++) {
        mj_task* task = (mj_task*)events[i].data.ptr;
        if (task != NULL && task->state == MJ_TASK_WAITING_IO) {
            task->state = MJ_TASK_RUNNABLE;
            enqueue_runnable(sched, task);
            sched->metrics.io_events++;
        }
    }
    
    return nfds;
}
```

#### Setting up epoll Integration

```c
mj_scheduler* setup_scheduler_with_epoll(void) {
    mj_scheduler* sched = mj_scheduler_create();
    
    // Create epoll instance
    mj_epoll_data* epoll_data = calloc(1, sizeof(mj_epoll_data));
    epoll_data->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    
    if (epoll_data->epoll_fd < 0) {
        MJ_ERROR("Failed to create epoll fd");
        free(epoll_data);
        mj_scheduler_destroy(sched);
        return NULL;
    }
    
    // Register the wait function
    mj_scheduler_set_wait_func(sched, wait_epoll, epoll_data);
    
    MJ_INFO("Scheduler created with epoll integration");
    return sched;
}
```

#### Task Waiting for I/O

```c
// User API to wait for a file descriptor to become readable
void mj_task_wait_readable(int fd) {
    mj_scheduler* sched = get_current_scheduler();
    mj_task* task = sched->current_task;
    mj_epoll_data* epoll_data = (mj_epoll_data*)sched->wait_data;
    
    // Add fd to epoll
    struct epoll_event ev = {
        .events = EPOLLIN | EPOLLET,  // Edge-triggered, readable
        .data.ptr = task
    };
    
    if (epoll_ctl(epoll_data->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        MJ_ERROR("Failed to add fd %d to epoll: %s", fd, strerror(errno));
        return;
    }
    
    // Mark task as waiting for I/O
    task->state = MJ_TASK_WAITING_IO;
    MJ_DEBUG("Task %p waiting for fd %d to become readable", (void*)task, fd);
}
```

### Custom Wait Functions

You can create specialized wait functions for different use cases:

#### 1. Hybrid Timer + I/O

```c
// Waits for either timer expiry OR I/O events
int wait_hybrid(mj_scheduler* sched, uint64_t timeout_ns, void* user_data) {
    // Use timerfd on Linux to integrate timers into epoll
    // Or use pselect/ppoll which support both timeout and fd monitoring
}
```

#### 2. Message Queue Integration

```c
// For inter-process communication
int wait_message_queue(mj_scheduler* sched, uint64_t timeout_ns, void* user_data) {
    // Block on message queue (e.g., POSIX mq_timedreceive)
    // Wake tasks when messages arrive
}
```

#### 3. Database Events

```c
// For PostgreSQL LISTEN/NOTIFY
int wait_postgresql(mj_scheduler* sched, uint64_t timeout_ns, void* user_data) {
    // Use PQconsumeInput / PQnotifies
    // Block with select() on connection socket
}
```

### Why This Architecture Works

1. **Scheduler stays generic**: Core code doesn't know about epoll, kqueue, or any I/O
2. **User chooses integration**: Pick nanosleep (simple), epoll (network), or custom (database)
3. **0% CPU when idle**: Blocking in wait function prevents busy loops
4. **Responsive**: Returns immediately when event occurs, no polling delay
5. **Testable**: Can provide mock wait function for unit tests

---

## Integration Example

### Complete Example: HTTP Server with Timers

```c
#include "majjen.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// Task state for HTTP connection
typedef struct http_connection {
    int socket_fd;
    char buffer[4096];
    size_t bytes_read;
    time_t last_activity;
} http_connection;

// Periodic cleanup task
void cleanup_task(mj_scheduler* sched, void* user_state) {
    int* iteration = (int*)user_state;
    (*iteration)++;
    
    MJ_INFO("Cleanup task iteration %d", *iteration);
    
    // Run cleanup every 30 seconds
    if (*iteration < 100) {
        mj_task_sleep_s(30);
    } else {
        MJ_INFO("Cleanup task exiting after 100 iterations");
        mj_task_exit();
    }
}

// HTTP connection handler
void handle_connection(mj_scheduler* sched, void* user_state) {
    http_connection* conn = (http_connection*)user_state;
    
    // Wait for data to be readable
    mj_task_wait_readable(conn->socket_fd);
    
    // Read data
    ssize_t n = read(conn->socket_fd, 
                     conn->buffer + conn->bytes_read,
                     sizeof(conn->buffer) - conn->bytes_read);
    
    if (n <= 0) {
        // Connection closed or error
        MJ_DEBUG("Connection closed on fd %d", conn->socket_fd);
        close(conn->socket_fd);
        mj_task_exit();
        return;
    }
    
    conn->bytes_read += n;
    conn->last_activity = time(NULL);
    
    // Simple HTTP response
    if (strstr(conn->buffer, "\r\n\r\n") != NULL) {
        const char* response = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "Hello, World!";
        
        write(conn->socket_fd, response, strlen(response));
        close(conn->socket_fd);
        mj_task_exit();
    }
    
    // Check for timeout (30 seconds)
    if (time(NULL) - conn->last_activity > 30) {
        MJ_WARN("Connection timeout on fd %d", conn->socket_fd);
        close(conn->socket_fd);
        mj_task_exit();
    }
}

// Accept task - spawns connection handlers
void accept_task(mj_scheduler* sched, void* user_state) {
    int listen_fd = *(int*)user_state;
    
    while (1) {
        // Wait for new connection
        mj_task_wait_readable(listen_fd);
        
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, 
                               (struct sockaddr*)&client_addr, 
                               &addr_len);
        
        if (client_fd < 0) {
            MJ_ERROR("Accept failed: %s", strerror(errno));
            continue;
        }
        
        MJ_INFO("New connection accepted on fd %d", client_fd);
        
        // Create task to handle this connection
        http_connection* conn = calloc(1, sizeof(http_connection));
        conn->socket_fd = client_fd;
        conn->last_activity = time(NULL);
        
        mj_create_task(sched, handle_connection, conn);
    }
}

int main(int argc, char** argv) {
    // Set up logging
    mj_log_set_level(MJ_LOG_INFO);
    
    // Create scheduler with epoll
    mj_scheduler* sched = setup_scheduler_with_epoll();
    if (sched == NULL) {
        return 1;
    }
    
    // Create listening socket
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        MJ_ERROR("Bind failed");
        return 1;
    }
    
    if (listen(listen_fd, 128) < 0) {
        MJ_ERROR("Listen failed");
        return 1;
    }
    
    MJ_INFO("HTTP server listening on port 8080");
    
    // Create accept task
    int* listen_fd_ptr = malloc(sizeof(int));
    *listen_fd_ptr = listen_fd;
    mj_create_task(sched, accept_task, listen_fd_ptr);
    
    // Create periodic cleanup task
    int* cleanup_state = malloc(sizeof(int));
    *cleanup_state = 0;
    mj_create_task(sched, cleanup_task, cleanup_state);
    
    // Run scheduler
    MJ_INFO("Starting scheduler...");
    mj_scheduler_run(sched);
    
    // Print final metrics
    mj_scheduler_print_metrics(sched, stdout);
    
    // Cleanup
    close(listen_fd);
    mj_scheduler_destroy(sched);
    
    MJ_INFO("Server stopped");
    return 0;
}
```

### Running the Example

```bash
# Compile
gcc -std=c99 -O2 -o http_server http_server.c majjen.c -I.

# Run
./http_server

# Test from another terminal
curl http://localhost:8080/

# Monitor metrics
# Press Ctrl+C to stop and see final metrics
```

### Expected Output

```
[2025-11-10 10:30:00] [INFO] Scheduler created with epoll integration
[2025-11-10 10:30:00] [INFO] HTTP server listening on port 8080
[2025-11-10 10:30:00] [INFO] Starting scheduler...
[2025-11-10 10:30:00] [INFO] Scheduler starting with 2 runnable tasks, 0 sleeping
[2025-11-10 10:30:05] [INFO] New connection accepted on fd 7
[2025-11-10 10:30:05] [DEBUG] Task 0x123456 waiting for fd 7 to become readable
[2025-11-10 10:30:05] [DEBUG] Connection closed on fd 7
[2025-11-10 10:30:30] [INFO] Cleanup task iteration 1

=== Majjen Scheduler Metrics ===
Tasks:
  Created:       3
  Exited:        1
  Current:       2
  Peak:          3

Execution:
  Total runs:    15
  Avg time:      50234 ns (0.050 ms)
  Min time:      12451 ns (0.012 ms)
  Max time:      234567 ns (0.235 ms)

Scheduler:
  Loop cycles:   120
  Wait calls:    45
  I/O events:    3

Timers:
  Expirations:   1
  Cancellations: 0
================================
```

---

## Summary

### Implementation Checklist

**Timers:**
- [ ] Add `mj_task_state` enum to track RUNNABLE/SLEEPING/WAITING_IO
- [ ] Implement min-heap for timer queue
- [ ] Add `wake_time_ns` field to `mj_task`
- [ ] Implement `mj_task_sleep_ns()`, `mj_task_sleep_ms()`, `mj_task_yield()`
- [ ] Add timer expiration checking in scheduler loop
- [ ] Implement `get_monotonic_time_ns()` helper

**Logging:**
- [ ] Define `mj_log_level` enum
- [ ] Create `mj_logger` configuration struct
- [ ] Implement `mj_log()` function with formatting
- [ ] Add convenience macros (MJ_INFO, MJ_DEBUG, etc.)
- [ ] Allow custom log callbacks
- [ ] Add log calls at key points in scheduler

**Metrics:**
- [ ] Create `mj_metrics` struct
- [ ] Track task executions, min/max/avg times
- [ ] Track scheduler loops and wait calls
- [ ] Track timer events
- [ ] Implement `mj_scheduler_get_metrics()` and `mj_scheduler_print_metrics()`

**Wait Function:**
- [ ] Define `mj_wait_fn` typedef
- [ ] Add `wait_func` and `wait_data` to `mj_scheduler`
- [ ] Implement `mj_scheduler_set_wait_func()`
- [ ] Create default `nanosleep`-based wait function
- [ ] Implement epoll integration (Linux)
- [ ] Add timeout calculation before wait
- [ ] Document wait function contract

### Next Steps

1. **Start with timers**: They're the foundation for everything else
2. **Add basic logging**: Makes debugging much easier
3. **Implement default wait function**: Get the architecture in place
4. **Add metrics**: Visibility into performance
5. **Implement epoll**: Real-world I/O integration

### Key Takeaways

- **Timers use a min-heap** to efficiently find the next wake time
- **Logging should be lightweight** with pluggable backends
- **Metrics track everything** for performance analysis and debugging
- **Wait functions decouple** the scheduler from specific I/O mechanisms
- **This architecture mirrors libuv/Node.js** - you're building the right thing!

Good luck with implementation! 🚀
