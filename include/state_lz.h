/*
* state_lz.h
*
* tiny streaming lz77 compressor used by kz-lite save states.
*
* kz-lite does not have enough free ram to hold an uncompressed save state,
* so every chunk the state code writes is compressed as it is written, and
* decompressed directly into game memory when it is read back.  Matches only
* reference data within the same chunk, so no history buffer is required on
* either side.
*/

#ifndef _STATE_LZ_H
#define _STATE_LZ_H

#include <stdint.h>
#include <stddef.h>

#define LZ_HASH_BITS    12
#define LZ_HASH_SIZE    (1 << LZ_HASH_BITS)

/* hash table used while compressing, LZ_HASH_SIZE entries */
typedef const uint8_t *lz_hash_t;

/**
 * Compress len bytes from src into out, never writing past out_end.
 * Returns the new output position, or NULL if the output buffer overflowed.
 */
uint8_t        *lz_compress     (uint8_t *out, uint8_t *out_end, const void *src, size_t len, lz_hash_t *hash);

/**
 * Decompress exactly len bytes from src into dst.
 * Returns the position in src following the compressed data.
 */
const uint8_t  *lz_decompress   (const uint8_t *src, void *dst, size_t len);

#endif
