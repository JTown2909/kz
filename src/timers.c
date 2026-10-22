#include <startup.h>
#include "timers.h"
#include "kz.h"
#include "z2.h"

uint64_t timers_frame_time = 0;

void timers_shift(uint64_t offset){
    char *file = (char*)&z2_file;
    uint8_t *states = (uint8_t*)(file + Z2_FILE_TIMER_STATES);
    uint64_t *start_times = (uint64_t*)(file + Z2_FILE_TIMER_START_TIMES);
    for(int i = 0; i < Z2_TIMER_CNT; i++){
        if(states[i] != 0){
            start_times[i] += offset;
            /* the start of an "alt" counting timer, kept outside the save context */
            z2_timer_alt_times[i] += offset;
        }
    }
    if(states[0] != 0){
        *(uint64_t*)(file + Z2_FILE_POSTMAN_STOP_TIME) += offset;
    }
    /* when the game paused its timers (pause menu, text boxes, ...) */
    z2_timer_pause_time += offset;

    for(int i = 0; i < 6; i++){
        if(z2_file.hsw_timer_active[i]){
            z2_file.hsw_catch_start[i] += offset;
        }
    }
    /* when the game paused the bottle timers */
    z2_timer_change_val += offset;
}

/* osTime counts at 46.875MHz, a game frame is update_rate retraces of 1/60s */
#define TIMERS_RETRACE_TICKS (46875000 / 60)

void timers_frame(int frames){
    uint64_t now = osGetTime();
    if(frames >= 0 && timers_frame_time != 0){
        int rate = 3;
        if(z2_file.static_ctx && z2_file.static_ctx->update_rate > 0){
            rate = z2_file.static_ctx->update_rate;
        }
        uint64_t counted = (uint64_t)frames * rate * TIMERS_RETRACE_TICKS;
        /* may wrap below zero when a frame was quicker than a game frame, which moves the timers on instead */
        timers_shift(now - timers_frame_time - counted);
    }
    timers_frame_time = now;
}

/*
 * the game's timer code (Interface_*: drawing and updating the minigame,
 * postman and bottle timers, and the postman's A press callback) calls this
 * instead of osGetTime().  while kz is paused or advancing frames it reads the
 * time the frame started, so the timers show the same value every time no
 * matter how long into the frame they're looked at.
 */
HOOK uint64_t timers_os_time(void){
    if(kz.pending_frames < 0 || timers_frame_time == 0){
        return osGetTime();
    }
    return timers_frame_time;
}
