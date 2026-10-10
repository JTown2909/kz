#ifdef LITE
#include <stdint.h>
#include <stddef.h>
#include "state_lz.h"

/*
* stream format, a sequence of tokens:
*   0x00-0x7F   literal run, token + 1 literal bytes follow
*   0x80-0xFF   match, followed by a 16 bit big endian distance (1-65535).
*               length = (token & 0x7F) + LZ_MIN_MATCH.  when (token & 0x7F)
*               is 0x7F, extension bytes follow, each is added to the length,
*               and another extension byte follows when one is 0xFF.
*/

#define LZ_MIN_MATCH    3
#define LZ_MAX_LIT      0x80
#define LZ_MAX_DIST     0xFFFF
#define LZ_LEN_EXT      0x7F

static inline uint32_t lz_hash(const uint8_t *p) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return (v * 2654435761u) >> (32 - LZ_HASH_BITS);
}

static uint8_t *lz_emit_literals(uint8_t *out, uint8_t *out_end, const uint8_t *lit, size_t cnt) {
    while(cnt > 0) {
        size_t run = cnt > LZ_MAX_LIT ? LZ_MAX_LIT : cnt;
        if(out_end - out < run + 1) {
            return NULL;
        }
        *out++ = run - 1;
        for(size_t i = 0; i < run; i++) {
            *out++ = lit[i];
        }
        lit += run;
        cnt -= run;
    }
    return out;
}

static uint8_t *lz_emit_match(uint8_t *out, uint8_t *out_end, size_t dist, size_t len) {
    len -= LZ_MIN_MATCH;
    size_t ext_cnt = len >= LZ_LEN_EXT ? (len - LZ_LEN_EXT) / 0xFF + 1 : 0;
    if(out_end - out < ext_cnt + 3) {
        return NULL;
    }
    *out++ = 0x80 | (len >= LZ_LEN_EXT ? LZ_LEN_EXT : len);
    *out++ = dist >> 8;
    *out++ = dist;
    if(len >= LZ_LEN_EXT) {
        len -= LZ_LEN_EXT;
        while(len >= 0xFF) {
            *out++ = 0xFF;
            len -= 0xFF;
        }
        *out++ = len;
    }
    return out;
}

uint8_t *lz_compress(uint8_t *out, uint8_t *out_end, const void *src, size_t len, lz_hash_t *hash) {
    const uint8_t *start = src;
    const uint8_t *ip = start;
    const uint8_t *end = start + len;
    const uint8_t *lit = start;

    if(out == NULL) {
        return NULL;
    }

    if(len >= LZ_MIN_MATCH) {
        const uint8_t *match_end = end - (LZ_MIN_MATCH - 1);
        while(ip < match_end) {
            uint32_t h = lz_hash(ip);
            const uint8_t *ref = hash[h];
            hash[h] = ip;

            /* entries left over from other chunks are rejected by the range check */
            if(ref < start || ref >= ip || ip - ref > LZ_MAX_DIST ||
               ref[0] != ip[0] || ref[1] != ip[1] || ref[2] != ip[2]) {
                ip++;
                continue;
            }

            size_t max = end - ip;
            size_t mlen = LZ_MIN_MATCH;
            while(mlen < max && ref[mlen] == ip[mlen]) {
                mlen++;
            }

            out = lz_emit_literals(out, out_end, lit, ip - lit);
            if(out == NULL) {
                return NULL;
            }
            out = lz_emit_match(out, out_end, ip - ref, mlen);
            if(out == NULL) {
                return NULL;
            }

            ip += mlen;
            lit = ip;

            /* keep the table fresh for data following a long match */
            if(ip < match_end) {
                hash[lz_hash(ip - 1)] = ip - 1;
            }
        }
    }

    return lz_emit_literals(out, out_end, lit, end - lit);
}

const uint8_t *lz_decompress(const uint8_t *src, void *dst, size_t len) {
    uint8_t *op = dst;
    uint8_t *end = op + len;

    while(op < end) {
        uint8_t token = *src++;
        if(token < 0x80) {
            size_t run = token + 1;
            for(size_t i = 0; i < run; i++) {
                *op++ = *src++;
            }
        } else {
            size_t mlen = token & LZ_LEN_EXT;
            size_t dist = (src[0] << 8) | src[1];
            src += 2;
            if(mlen == LZ_LEN_EXT) {
                uint8_t ext;
                do {
                    ext = *src++;
                    mlen += ext;
                } while(ext == 0xFF);
            }
            mlen += LZ_MIN_MATCH;

            /* byte by byte so overlapping matches act as run length encoding */
            const uint8_t *ref = op - dist;
            for(size_t i = 0; i < mlen; i++) {
                *op++ = *ref++;
            }
        }
    }

    return src;
}

#endif
