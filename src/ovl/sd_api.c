/* the sd overlay's api table, see ovl.h */
#include "ovl.h"

void sd_shutdown(void);

const struct ovl_sd_api ovl_api OVL_API = {
    open,       creat,      mkdir,      chdir,
    stat,       getcwd,     close,      read,
    write,      readdir,    closedir,   opendir,
    reset_disk, sd_shutdown,
};
