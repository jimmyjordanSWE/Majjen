# Majjen Design Document (Draft)

## Overview
- Majjen is a cooperative scheduler running tasks (`mj_task`) until they yield or exit.
- Tasks manage their own `user_state`; scheduler manages task lifecycle and timing.
- The scheduler loop (`mj_scheduler_run`) continues while runnable or waiting tasks exist.

## Event Loop Architecture
1. Run all runnable tasks sequentially.
2. Delegate waiting to `mj_wait_func` to block on external events or sleep until the next timer.
3. Wake tasks by updating their state to runnable when timers expire or external events arrive.

## Design choices
- Blocking in OS-level pollers like `epoll_wait`, `kqueue`, or IOCP is the standard way to achieve idle 0% CPU usage while remaining responsive.
- The `mj_wait_func` hook mirrors libuv’s poll phase, enabling integration with platform-specific event sources. 
- Defaulting to a `nanosleep` fallback keeps the scheduler functional without I/O integration but limits responsiveness. 

## Suggestions
- Implement a min-heap to optimize wake-up scheduling.
- Provide reference `mj_wait_func` implementations: one using `nanosleep`, another wrapping `epoll`.
- Add tracing hooks (start/end of task execution, wait transitions) for debugging and performance profiling.
- Document ownership rules clearly: scheduler owns `mj_task`, user owns `user_state`.

## about the timers
wait_func is the scheduler’s abstraction for “block until something happens (I/O or time).”
The timer structure (min-heap or wheel) answers “when is the next timer due?” cheaply so the scheduler can give wait_func an accurate timeout.
You cannot replace the timer structure with “just epoll” unless you also add extra primitives (timerfd/eventfd) and still track timer scheduling efficiently.