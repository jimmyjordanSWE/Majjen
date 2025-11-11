#include "timer.h"
#include <stdio.h>
#include <string.h>

// Pick the most precise monotonic clock available.
// CLOCK_MONOTONIC_RAW avoids NTP adjustments on Linux.
// If RAW isn't available, fall back to the standard monotonic clock.
#if defined(CLOCK_MONOTONIC_RAW)
#define CLOCK_TIMER_CLOCK CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TIMER_CLOCK CLOCK_MONOTONIC
#endif

// Internal helper.
// Returns the "effective end" time of the timer:
//
// - If the timer is still running, we fetch the current timestamp.
// - If it has already been stopped, we return the stored end timestamp.
//
static struct timespec clock_timer_get_effective_end(const clock_timer_t* t) {
    struct timespec eff = {0, 0};
    if (!t) return eff;

    if (t->running) {
        // Timer still running: get current time
        clock_gettime(CLOCK_TIMER_CLOCK, &eff);
    } else {
        // Timer stopped: use last stop time
        eff = t->end;
    }
    return eff;
}

// Internal helper that computes a time difference in nanoseconds.
// Returns max(end - start, 0) to avoid negative results due to clock quirks.
static int64_t clock_timer_diff_ns(const struct timespec* end, const struct timespec* start) {
    int64_t s = (int64_t)end->tv_sec - (int64_t)start->tv_sec;
    int64_t ns = (int64_t)end->tv_nsec - (int64_t)start->tv_nsec;
    int64_t total = s * 1000000000LL + ns;

    // Clamp to zero to avoid negative durations
    return total < 0 ? 0 : total;
}

// Zero-initialize the timer struct.
void clock_timer_init(clock_timer_t* t) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
}

// Start the timer: record current time into t->start.
void clock_timer_start(clock_timer_t* t) {
    if (!t) return;
    clock_gettime(CLOCK_TIMER_CLOCK, &t->start);
    t->running = 1;
}

// Stop the timer: record current time into t->end.
void clock_timer_stop(clock_timer_t* t) {
    if (!t) return;
    clock_gettime(CLOCK_TIMER_CLOCK, &t->end);
    t->running = 0;
}

// Reset the timer fields to zero.
void clock_timer_reset(clock_timer_t* t) {
    if (!t) return;
    memset(&t->start, 0, sizeof(t->start));
    memset(&t->end, 0, sizeof(t->end));
    t->running = 0;
}

// Query whether the timer is running.
int clock_timer_is_running(const clock_timer_t* t) {
    return t ? t->running : 0;
}

// Returns elapsed time in nanoseconds.
// Works even if the timer is still running.
int64_t clock_timer_elapsed_ns(const clock_timer_t* t) {
    if (!t) return 0;
    struct timespec eff = clock_timer_get_effective_end(t);
    return clock_timer_diff_ns(&eff, &t->start);
}

// Microseconds = nanoseconds / 1,000
int64_t clock_timer_elapsed_us(const clock_timer_t* t) {
    return clock_timer_elapsed_ns(t) / 1000LL;
}

// Milliseconds = nanoseconds / 1e6
double clock_timer_elapsed_ms(const clock_timer_t* t) {
    return (double)clock_timer_elapsed_ns(t) / 1e6;
}

// Seconds = nanoseconds / 1e9
double clock_timer_elapsed_s(const clock_timer_t* t) {
    return (double)clock_timer_elapsed_ns(t) / 1e9;
}

// Format elapsed time into a readable string.
// Automatically chooses ns, us, ms, or s based on magnitude.
char* clock_timer_format_elapsed(const clock_timer_t* t, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return buf;

    int64_t ns = clock_timer_elapsed_ns(t);

    if (ns < 1000) {
        snprintf(buf, buflen, "%lldns", (long long)ns);
        return buf;
    }
    if (ns < 1000000) {
        double us = ns / 1000.0;
        snprintf(buf, buflen, "%.3fus", us);
        return buf;
    }
    if (ns < 1000000000) {
        double ms = ns / 1e6;
        snprintf(buf, buflen, "%.3fms", ms);
        return buf;
    }

    double s = ns / 1e9;
    snprintf(buf, buflen, "%.6fs", s);
    return buf;
}
