/*
* timers.h
*
* keeps the game's minigame and bottle timers in step with kz pausing and
* save states.  The game counts a timer as osGetTime() minus the osTime it
* started (minus time spent paused), so time the game doesn't run for has to
* be taken out by moving those osTimes forward.
*/

#ifndef _TIMERS_H
#define _TIMERS_H

#include <stdint.h>

/* the osTime of the last game frame kz has accounted for */
extern uint64_t timers_frame_time;

/* moves every running timer's osTimes forward by offset */
void timers_shift(uint64_t offset);

/* called once per frame; while kz is paused, the frame's time is taken out */
void timers_frame(_Bool paused);

#endif
