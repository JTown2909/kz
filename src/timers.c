#include "timers.h"
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

void timers_frame(_Bool paused){
    uint64_t now = osGetTime();
    if(paused && timers_frame_time != 0){
        timers_shift(now - timers_frame_time);
    }
    timers_frame_time = now;
}
