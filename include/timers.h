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

/*
* takes out of the timers the time since the last call that the game didn't
* run for.  frames < 0 counts all of it (kz isn't paused), otherwise exactly
* that many game frames are counted, so frame advance moves the timers by the
* same amount every time, however long each frame really took.
*/
void timers_frame(int frames);

/* osGetTime() for the game's timer code, stopped while kz is paused */
uint64_t timers_os_time(void);

#endif
