/*
* ovl.c
*
* kz-lite keeps parts of itself in rom as overlays and loads them into heap
* memory only while they are used (see ovl.h).  An overlay is linked at
* hdr.base and relocated to wherever it is loaded.
*
* The overlays are stored in rom right after kz.bin, padded to 16 bytes:
* a directory (overlay count, then the offset of each overlay), then the
* overlays.  kz finds its own rom address in the loader at rom 0x1000.
*/
#if defined(LITE) && !defined(KZ_OVERLAY)
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "ovl.h"
#include "cache.h"
#include "kz.h"
#include "z2.h"

extern char _start[];
extern char __bss_start[];

struct ovl {
    char   *mem;
    void   *api;
};

static struct ovl   ovls[OVL_CNT];
static uint32_t     ovl_rom;
static uint32_t     dma_buf[0x60 / 4] __attribute__((aligned(16)));

int ovl_sd_busy(void);
void ovl_sd_released(void);

/*
 * the loader at rom 0x1000 dma's the payload (the game's original 0x60 byte
 * entry, then kz.bin) with "la $t1, rom | 0x10000000; sw $t1, 0x04($t0)"
 */
static uint32_t find_ovl_rom(void){
    if(z2_DmaRomToRam(0x1000, dma_buf, sizeof(dma_buf)) != 0){
        return 0;
    }
    uint32_t t1 = 0;
    for(int i = 0; i < sizeof(dma_buf) / sizeof(*dma_buf); i++){
        uint32_t w = dma_buf[i];
        uint32_t op = w >> 26;
        uint32_t rs = (w >> 21) & 0x1F;
        uint32_t rt = (w >> 16) & 0x1F;
        if(w == 0xAD090004){
            uint32_t kz_rom = (t1 & 0x0FFFFFFF) + 0x60;
            uint32_t kz_size = __bss_start - _start;
            return kz_rom + ((kz_size + 15) & ~15);
        }
        if(rt != 9){
            continue;
        }
        if(op == 0x0F){         /* lui */
            t1 = (w & 0xFFFF) << 16;
        }else if(op == 0x0D){   /* ori */
            t1 = (rs == 9 ? t1 : 0) | (w & 0xFFFF);
        }else if(op == 0x09){   /* addiu */
            t1 = (rs == 9 ? t1 : 0) + (int16_t)(w & 0xFFFF);
        }
    }
    return 0;
}

static void relocate(char *mem, ovl_hdr_t *hdr){
    uint32_t delta = (uint32_t)mem - hdr->base;
    ovl_reloc_t *reloc = (ovl_reloc_t*)(mem + hdr->image_size);
    for(int i = 0; i < hdr->reloc_cnt; i++, reloc++){
        uint32_t *p = (uint32_t*)(mem + (reloc->info & 0xFFFFFF));
        switch(reloc->info >> 24){
            case OVL_R_32:
                *p += delta;
                break;
            case OVL_R_26:
                *p = (*p & 0xFC000000) | ((((*p & 0x03FFFFFF) << 2) + delta) >> 2 & 0x03FFFFFF);
                break;
            case OVL_R_HI16:
                *p = (*p & 0xFFFF0000) | (((reloc->value + delta + 0x8000) >> 16) & 0xFFFF);
                break;
            case OVL_R_LO16:
                *p = (*p & 0xFFFF0000) | ((*p + delta) & 0xFFFF);
                break;
        }
    }
}

static void *ovl_load(int id){
    if(ovl_rom == 0){
        ovl_rom = find_ovl_rom();
        if(ovl_rom == 0){
            return NULL;
        }
    }

    /* directory */
    if(z2_DmaRomToRam(ovl_rom, dma_buf, 0x10) != 0 || id >= dma_buf[0]){
        return NULL;
    }
    uint32_t rom = ovl_rom + dma_buf[1 + id];

    ovl_hdr_t hdr;
    if(z2_DmaRomToRam(rom, dma_buf, sizeof(hdr)) != 0){
        return NULL;
    }
    memcpy(&hdr, dma_buf, sizeof(hdr));
    if(hdr.magic != OVL_MAGIC){
        return NULL;
    }

    /* the relocations are loaded after the image, where the bss will be */
    size_t reloc_size = hdr.reloc_cnt * sizeof(ovl_reloc_t);
    size_t size = hdr.image_size + (hdr.bss_size > reloc_size ? hdr.bss_size : reloc_size);
    char *mem = memalign(16, size);
    if(mem == NULL){
        return NULL;
    }
    if(z2_DmaRomToRam(rom + sizeof(hdr), mem, (hdr.image_size + reloc_size + 1) & ~1) != 0){
        free(mem);
        return NULL;
    }
    relocate(mem, &hdr);
    memset(mem + hdr.image_size, 0, hdr.bss_size);
    osWritebackDCache(mem, size);
    osInvalICache(mem, hdr.image_size);

    ovls[id].mem = mem;
    ovls[id].api = mem + hdr.api_offset;
    return ovls[id].api;
}

static void ovl_unload(int id){
    free(ovls[id].mem);
    ovls[id].mem = NULL;
    ovls[id].api = NULL;
}

void *ovl_get(int id){
    if(ovls[id].api != NULL){
        return ovls[id].api;
    }
    return ovl_load(id);
}

void ovl_collect(void){
    /* the state code is only used during a save or load */
    if(ovls[OVL_STATE].mem != NULL){
        ovl_unload(OVL_STATE);
    }
    /* the sd card code is kept while the menu is open or a file is open */
    if(ovls[OVL_SD].mem != NULL && !kz.menu_active && !ovl_sd_busy()){
        ((struct ovl_sd_api*)ovls[OVL_SD].api)->shutdown();
        ovl_unload(OVL_SD);
        ovl_sd_released();
    }
}
#endif
