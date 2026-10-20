#!/usr/bin/env python3
# mkovl.py: makes kz-lite overlays (see include/ovl.h).
#
#   mkovl.py ovl <overlay.elf> <out.ovl>
#       turns an overlay linked with ovl.ld and --emit-relocs into an
#       overlay file: header, image (text, rodata, data) and the
#       relocations for addresses inside the overlay.
#   mkovl.py syms <kz.elf> <out.ld> <overlay objects...>
#       writes the address of every symbol in kz.elf that the overlay's
#       objects don't define themselves, so the overlay can be linked
#       against kz (and uses kz's malloc, memcpy etc. instead of its own).
#   mkovl.py keep <out.ld> <a.o> ... [-- <b.o> ...]
#       for overlays made of the given objects (groups separated by --),
#       writes EXTERN() for every symbol they need from kz, so kz's link
#       (with --gc-sections) keeps them.
#   mkovl.py pack <out.bin> <a.ovl> [<b.ovl> ...]
#       packs overlays into kz-ovl.bin: count, offsets, overlays.

import struct
import sys

SHT_SYMTAB, SHT_NOBITS, SHT_REL, SHT_RELA = 2, 8, 9, 4
SHF_ALLOC = 2
SHN_UNDEF, SHN_ABS, SHN_COMMON = 0, 0xFFF1, 0xFFF2
R_MIPS_NONE, R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_PC16, R_MIPS_JALR = 0, 2, 4, 5, 6, 10, 37
OVL_MAGIC = 0x4B5A4F56


def align(n, a):
    return (n + a - 1) & ~(a - 1)


class Elf:
    def __init__(self, path):
        self.data = data = open(path, 'rb').read()
        if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 2:
            raise SystemExit('%s: not a 32-bit big endian elf' % path)
        shoff, = struct.unpack_from('>I', data, 0x20)
        shentsize, shnum, shstrndx = struct.unpack_from('>HHH', data, 0x2E)
        self.sections = []
        for i in range(shnum):
            f = struct.unpack_from('>IIIIIIIIII', data, shoff + i * shentsize)
            self.sections.append(dict(name=f[0], type=f[1], flags=f[2], addr=f[3], offset=f[4],
                                      size=f[5], link=f[6], info=f[7], entsize=f[9], index=i))
        strtab = self.sections[shstrndx]
        for s in self.sections:
            s['name'] = self.cstr(strtab['offset'] + s['name'])

    def cstr(self, off):
        return self.data[off:self.data.index(b'\0', off)].decode()

    def section(self, name):
        for s in self.sections:
            if s['name'] == name:
                return s
        return None

    def symbols(self):
        symtab = next(s for s in self.sections if s['type'] == SHT_SYMTAB)
        strtab = self.sections[symtab['link']]
        syms = []
        for off in range(symtab['offset'], symtab['offset'] + symtab['size'], 16):
            name, value, size, info, other, shndx = struct.unpack_from('>IIIBBH', self.data, off)
            syms.append(dict(name=self.cstr(strtab['offset'] + name), value=value, info=info, shndx=shndx))
        return syms


def make_ovl(elf_path, out_path):
    elf = Elf(elf_path)
    syms = elf.symbols()
    base = elf.section('.text')['addr']

    image_secs = [s for s in elf.sections if s['flags'] & SHF_ALLOC and s['type'] != SHT_NOBITS and s['size'] > 0]
    bss_secs = [s for s in elf.sections if s['flags'] & SHF_ALLOC and s['type'] == SHT_NOBITS and s['size'] > 0]
    end = max([s['addr'] + s['size'] for s in image_secs] + [base])
    image = bytearray(align(end - base, 16))
    for s in image_secs:
        image[s['addr'] - base:s['addr'] - base + s['size']] = elf.data[s['offset']:s['offset'] + s['size']]
    bss_end = max([s['addr'] + s['size'] for s in bss_secs] + [base + len(image)])
    bss_size = align(max(bss_end - (base + len(image)), 0), 16)

    def word(addr):
        return struct.unpack_from('>I', image, addr - base)[0]

    relocs = []
    image_idx = {s['index'] for s in image_secs}
    for rs in elf.sections:
        if rs['type'] == SHT_RELA:
            raise SystemExit('%s: rela relocations are not supported' % elf_path)
        if rs['type'] != SHT_REL or rs['info'] not in image_idx:
            continue
        target = elf.sections[rs['info']]
        entries = []
        for off in range(rs['offset'], rs['offset'] + rs['size'], 8):
            r_offset, r_info = struct.unpack_from('>II', elf.data, off)
            addr = r_offset if target['addr'] <= r_offset < target['addr'] + target['size'] else target['addr'] + r_offset
            entries.append((addr, r_info >> 8, r_info & 0xFF))
        for i, (addr, sym, rtype) in enumerate(entries):
            shndx = syms[sym]['shndx']
            if shndx in (SHN_UNDEF, SHN_ABS, SHN_COMMON) or rtype in (R_MIPS_NONE, R_MIPS_PC16, R_MIPS_JALR):
                # in kz, or pc relative: doesn't move with the overlay
                continue
            if rtype not in (R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16):
                raise SystemExit('%s: unsupported relocation type %d at %08X (compile overlays with -G 0)' % (elf_path, rtype, addr))
            value = 0
            if rtype == R_MIPS_HI16:
                lo = next((e for e in entries[i + 1:] if e[2] == R_MIPS_LO16 and e[1] == sym), None)
                if lo is None:
                    raise SystemExit('%s: hi16 at %08X has no lo16' % (elf_path, addr))
                lo_val = word(lo[0]) & 0xFFFF
                value = ((word(addr) & 0xFFFF) << 16) + (lo_val - 0x10000 if lo_val & 0x8000 else lo_val)
                value &= 0xFFFFFFFF
            relocs.append(struct.pack('>II', (rtype << 24) | (addr - base), value))

    api = next((s for s in syms if s['name'] == 'ovl_api' and s['shndx'] not in (SHN_UNDEF, SHN_ABS)), None)
    if api is None:
        raise SystemExit('%s: no ovl_api' % elf_path)
    hdr = struct.pack('>8I', OVL_MAGIC, len(image), bss_size, len(relocs), api['value'] - base, base, 0, 0)
    out = hdr + bytes(image) + b''.join(relocs)
    open(out_path, 'wb').write(out + bytes(align(len(out), 16) - len(out)))
    print('%s: %d bytes image, %d bytes bss, %d relocations' % (out_path, len(image), bss_size, len(relocs)))


def make_syms(elf_path, out_path, objs):
    own = set()
    for o in objs:
        own |= object_symbols(o)[0]
    elf = Elf(elf_path)
    lines = []
    for s in elf.symbols():
        bind = s['info'] >> 4
        if bind not in (1, 2) or s['shndx'] == SHN_UNDEF or not s['name'] or s['name'] in own:
            continue
        lines.append('%s = 0x%08X;\n' % (s['name'], s['value']))
    open(out_path, 'w').write(''.join(sorted(set(lines))))


def object_symbols(path):
    elf = Elf(path)
    defined, undefined = set(), set()
    for s in elf.symbols():
        if s['info'] >> 4 not in (1, 2) or not s['name']:
            continue
        (undefined if s['shndx'] == SHN_UNDEF else defined).add(s['name'])
    return defined, undefined


def make_keep(out_path, args):
    groups, cur = [], []
    for a in args:
        if a == '--':
            groups.append(cur)
            cur = []
        else:
            cur.append(a)
    groups.append(cur)
    need = set()
    for g in groups:
        defined, undefined = set(), set()
        for o in g:
            d, u = object_symbols(o)
            defined |= d
            undefined |= u
        need |= undefined - defined
    open(out_path, 'w').write(''.join('EXTERN(%s)\n' % n for n in sorted(need)))


def pack(out_path, ovls):
    blobs = [open(p, 'rb').read() for p in ovls]
    dir_size = align(4 + 4 * len(blobs), 16)
    offsets, pos = [], dir_size
    for b in blobs:
        offsets.append(pos)
        pos += align(len(b), 16)
    out = bytearray(struct.pack('>I', len(blobs)) + b''.join(struct.pack('>I', o) for o in offsets))
    out += bytes(dir_size - len(out))
    for b in blobs:
        out += b + bytes(align(len(b), 16) - len(b))
    open(out_path, 'wb').write(out)


if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == 'ovl':
        make_ovl(sys.argv[2], sys.argv[3])
    elif len(sys.argv) >= 4 and sys.argv[1] == 'syms':
        make_syms(sys.argv[2], sys.argv[3], sys.argv[4:])
    elif len(sys.argv) >= 4 and sys.argv[1] == 'keep':
        make_keep(sys.argv[2], sys.argv[3:])
    elif len(sys.argv) >= 4 and sys.argv[1] == 'pack':
        pack(sys.argv[2], sys.argv[3:])
    else:
        raise SystemExit(__doc__ if __doc__ else 'usage: mkovl.py ovl|syms|pack ...')
