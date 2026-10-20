/*
* ovl_stubs.c
*
* in kz-lite, the sd card (sys.h) and state functions are overlays; these
* load the overlay and call it.
*/
#if defined(LITE) && !defined(KZ_OVERLAY)
#include <stdarg.h>
#include <errno.h>
#include "ovl.h"
#include "kz.h"

/* open files and directories, the sd overlay is kept while there are any */
static int sd_open_cnt;
int ovl_sd_busy(void){
    return sd_open_cnt > 0;
}

void ovl_sd_released(void){
    sd_open_cnt = 0;
}

static struct ovl_sd_api *sd(void){
    struct ovl_sd_api *api = ovl_get(OVL_SD);
    if(api == NULL){
        errno = ENOMEM;
    }
    return api;
}

int open(const char *path, int open_flags, ...){
    va_list va;
    va_start(va, open_flags);
    mode_t mode = va_arg(va, mode_t);
    va_end(va);
    struct ovl_sd_api *api = sd();
    if(api == NULL){
        return -1;
    }
    int ret = api->open(path, open_flags, mode);
    if(ret >= 0){
        sd_open_cnt++;
    }
    return ret;
}

int creat(const char *path, mode_t mode){
    return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int close(int file){
    struct ovl_sd_api *api = sd();
    if(api == NULL){
        return -1;
    }
    int ret = api->close(file);
    if(ret == 0 && sd_open_cnt > 0){
        sd_open_cnt--;
    }
    return ret;
}

DIR *opendir(const char *dir){
    struct ovl_sd_api *api = sd();
    if(api == NULL){
        return NULL;
    }
    DIR *ret = api->opendir(dir);
    if(ret != NULL){
        sd_open_cnt++;
    }
    return ret;
}

int closedir(DIR *dir){
    struct ovl_sd_api *api = sd();
    if(api == NULL){
        return -1;
    }
    int ret = api->closedir(dir);
    if(sd_open_cnt > 0){
        sd_open_cnt--;
    }
    return ret;
}

dirent_t *readdir(DIR *dir){
    struct ovl_sd_api *api = sd();
    return api ? api->readdir(dir) : NULL;
}

int read(int file, void *buf, uint32_t byte_cnt){
    struct ovl_sd_api *api = sd();
    return api ? api->read(file, buf, byte_cnt) : -1;
}

int write(int file, void *buf, uint32_t byte_cnt){
    struct ovl_sd_api *api = sd();
    return api ? api->write(file, buf, byte_cnt) : -1;
}

int mkdir(const char *path, mode_t mode){
    struct ovl_sd_api *api = sd();
    return api ? api->mkdir(path, mode) : -1;
}

int chdir(const char *path){
    struct ovl_sd_api *api = sd();
    return api ? api->chdir(path) : -1;
}

int stat(const char *path, struct stat *buf){
    struct ovl_sd_api *api = sd();
    return api ? api->stat(path, buf) : -1;
}

char *getcwd(char *buf, size_t size){
    struct ovl_sd_api *api = sd();
    return api ? api->getcwd(buf, size) : NULL;
}

int reset_disk(void){
    struct ovl_sd_api *api = sd();
    return api ? api->reset_disk() : -1;
}

/* state.c */

static struct ovl_state_api *state(void){
    struct ovl_state_api *api = ovl_get(OVL_STATE);
    if(api == NULL){
        kz_log("not enough memory for state code");
    }
    return api;
}

kz_state_hdr_t *save_state_alloc(void){
    struct ovl_state_api *api = state();
    return api ? api->save_state_alloc() : NULL;
}

void load_state(void *state_data){
    struct ovl_state_api *api = state();
    if(api){
        api->load_state(state_data);
    }
}

int state_is_valid(kz_state_hdr_t *hdr){
    struct ovl_state_api *api = state();
    return api ? api->state_is_valid(hdr) : -2;
}
#endif
