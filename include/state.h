/*
* state.h
*
* defintions for save state functionality
*/

#ifndef _STATE_H
#define _STATE_H

#include <stdint.h>
#include <stdlib.h>

#define STATE_VERSION 8

#ifdef LITE
/* lite states are compressed, so they are versioned apart from full/vc states */
#define STATE_LITE          0x8000
#define STATE_HDR_VERSION   (STATE_VERSION | STATE_LITE)
#else
#define STATE_HDR_VERSION   STATE_VERSION
#endif

#ifdef LITE
/*
 * memory left free when allocating a state, for kz's own allocations such as
 * menu textures.  menu icons that do not fit are simply not drawn.
 */
#define STATE_MEM_RESERVE   0x10000
/* smallest output buffer worth attempting to compress a state into */
#define STATE_MIN_ALLOC     0x8000
#endif

typedef struct {
    uint16_t    z2_version;
    uint16_t    state_version;
    size_t      size;
    char        name[64];
} kz_state_hdr_t;

size_t  save_state      (void *state);
void    load_state      (void *state);
int     state_is_valid  (kz_state_hdr_t *state);

#ifdef LITE
/**
 * Saves a compressed state into newly allocated memory.
 * Returns NULL if there is not enough free memory to hold the state.
 */
kz_state_hdr_t *save_state_alloc(void);

/**
 * Allocates memory for a state, keeping STATE_MEM_RESERVE free.
 */
void           *state_mem_alloc(size_t size);
#endif

#endif
