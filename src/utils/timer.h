#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define SPEED_OF_LIGHT 299792458.0

typedef struct {
    struct timespec start;
    struct timespec end;
    int running;
} clock_timer_t;

void clock_timer_init(clock_timer_t* t);
void clock_timer_start(clock_timer_t* t);
void clock_timer_stop(clock_timer_t* t);
void clock_timer_reset(clock_timer_t* t);
int clock_timer_is_running(const clock_timer_t* t);

int64_t clock_timer_elapsed_ns(const clock_timer_t* t);
int64_t clock_timer_elapsed_us(const clock_timer_t* t);
double clock_timer_elapsed_ms(const clock_timer_t* t);
double clock_timer_elapsed_s(const clock_timer_t* t);

char* clock_timer_format_elapsed(const clock_timer_t* t, char* buf, size_t buflen);
