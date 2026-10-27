/*
* ovl.h
*
* kz-lite overlays: parts of kz that are kept in rom and only loaded into
* memory while they are used.
*/

#ifndef _OVL_H
#define _OVL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include "sys.h"
#include "state.h"

#define OVL_SD      0   /* flashcart sd card: libcart, fatfs and sd.c */
#define OVL_STATE   1   /* saving and loading states: state.c, state_lz.c */
#define OVL_CNT     2

#define OVL_MAGIC   0x4B5A4F56  /* "KZOV" */

/* each overlay starts with this header, followed by its image and relocations */
typedef struct {
    uint32_t    magic;
    uint32_t    image_size;     /* text, rodata and data, padded to 16 */
    uint32_t    bss_size;
    uint32_t    reloc_cnt;
    uint32_t    api_offset;     /* offset of the api table in the image */
    uint32_t    base;           /* address the image was linked at */
    uint32_t    pad[2];
} ovl_hdr_t;

/* relocation types, as in elf */
#define OVL_R_32    2
#define OVL_R_26    4
#define OVL_R_HI16  5
#define OVL_R_LO16  6

typedef struct {
    uint32_t    info;           /* type << 24 | offset in the image */
    uint32_t    value;          /* hi16: the full address the hi16/lo16 pair loads */
} ovl_reloc_t;

struct ovl_sd_api {
    int         (*open)         (const char *path, int open_flags, ...);
    int         (*creat)        (const char *path, mode_t mode);
    int         (*mkdir)        (const char *path, mode_t mode);
    int         (*chdir)        (const char *path);
    int         (*stat)         (const char *path, struct stat *buf);
    char       *(*getcwd)       (char *buf, size_t size);
    int         (*close)        (int file);
    int         (*read)         (int file, void *buf, uint32_t byte_cnt);
    int         (*write)        (int file, void *buf, uint32_t byte_cnt);
    dirent_t   *(*readdir)      (DIR *dir);
    int         (*closedir)     (DIR *dir);
    DIR        *(*opendir)      (const char *dir);
    int         (*reset_disk)   (void);
    void        (*shutdown)     (void);
    const char *(*status)       (void);
};

struct ovl_state_api {
    kz_state_hdr_t *(*save_state_alloc)  (void);
    void            (*load_state)        (void *state);
    int             (*state_is_valid)    (kz_state_hdr_t *state);
};

#ifdef KZ_OVERLAY
/* in an overlay: the api table */
#define OVL_API __attribute__((section(".ovl_api"), used))
#else
/* in kz: loads an overlay if needed, returns its api table, or NULL if it couldn't be loaded */
void   *ovl_get         (int id);
/* frees overlays that are no longer needed; called at the start of a frame */
void    ovl_collect     (void);
#endif

#endif
