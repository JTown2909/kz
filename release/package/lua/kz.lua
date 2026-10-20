-- kz.lua
-- Patches a vanilla Majora's Mask rom with prebuilt kz. This is build/lua/make.lua
-- from kz, changed to use the prebuilt files in data/ instead of compiling kz,
-- and to generate the loader itself instead of assembling ldr/ldr.S, so no n64
-- toolchain is needed.
--
-- kz_patch(rom, mode) takes a rom loaded with gru.n64rom_load and a mode,
-- "lite" (n64, wii u vc, emulators) or "vc" (wii vc), and returns the patched
-- rom, its rom_info entry and the kz version name.

dofile("lua/rom_info.lua")

function kz_patch(rom, mode)
    local rom_info = roms[rom:crc32()]
    if rom_info == nil then
        error(string.format("not a supported rom (crc32 %08X).\n" ..
                            "Use an unmodified Majora's Mask: NTSC-U 1.0, NTSC-J 1.0, or NTSC-J 1.1.",
                            rom:crc32()), 0)
    end

    local kz_version = "kz-" .. mode .. "-" .. rom_info.rom_id
    local data = "data/" .. kz_version .. "/"
    local ram = 0x8003DF00
    print("Patching " .. rom_info.title .. " rom with " .. kz_version)

    local fs = gru.z64fs_load_blob(rom)
    local code = fs:get(rom_info.code)

    print("Applying hooks")
    local hooks = gru.gsc_load(data .. "hooks.gsc")
    hooks:shift(-rom_info.code_ram)
    hooks:apply_be(code)

    print("Applying size patch")
    local ups = gru.gsc_load(data .. "ups_size_patch.gsc")
    ups:shift(-rom_info.code_ram)
    ups:apply_be(code)

    print("Assembling rom")
    fs:replace(rom_info.code, code, fs:compressed(rom_info.code))
    local patched_rom = fs:assemble_rom()

    print("Applying memory hack")
    gru.gsc_load("data/zero_patch_" .. mode .. ".gsc"):apply_be(patched_rom)

    local prev_ldr = patched_rom:copy(0x1000, 0x60)
    local kz = gru.blob_load(data .. "kz.bin")
    local payload_rom = fs:prom_tail()

    -- the loader dma's kz from rom to ram and jumps to it, same as ldr/ldr.S
    print("Building loader")
    local ldr = gru.blob_blank(0x60)
    local pc = 0
    local function emit(word)
        ldr:write32be(pc, word)
        pc = pc + 4
    end

    local T0, T1 = 8, 9
    -- "la reg, value" the way the assembler expands it
    local function la(reg, value)
        local hi = (value >> 16) & 0xFFFF
        local lo = value & 0xFFFF
        if value < 0x8000 then
            emit(0x24000000 | (reg << 16) | lo)                 -- addiu reg, $zero, lo
        elseif hi == 0 then
            emit(0x34000000 | (reg << 16) | lo)                 -- ori reg, $zero, lo
        else
            emit(0x3C000000 | (reg << 16) | hi)                 -- lui reg, hi
            if lo ~= 0 then
                emit(0x34000000 | (reg << 21) | (reg << 16) | lo) -- ori reg, reg, lo
            end
        end
    end
    local function sw(rt, off, base) emit(0xAC000000 | (base << 21) | (rt << 16) | off) end

    la(T0, 0xA4600000)
    la(T1, ram & 0x1FFFFFFF)
    sw(T1, 0x00, T0)
    la(T1, payload_rom | 0x10000000)
    sw(T1, 0x04, T0)
    la(T1, kz:size() + 0x60 - 1)
    sw(T1, 0x0C, T0)
    local dma_wait = pc
    emit(0x8C000000 | (T0 << 21) | (T1 << 16) | 0x10)           -- lw t1, 0x10(t0)
    emit(0x30000000 | (T1 << 21) | (T1 << 16) | 0x01)           -- andi t1, t1, 1
    emit(0x14000000 | (T1 << 21) | (((dma_wait - pc - 4) >> 2) & 0xFFFF)) -- bnez t1, dma_wait
    emit(0)                                                     -- nop
    la(T1, 0x02)
    sw(T1, 0x10, T0)
    emit(0x08000000 | ((ram >> 2) & 0x3FFFFFF))                 -- j ram
    emit(0)                                                     -- nop

    print("Inserting payload")
    patched_rom:write(0x1000, ldr)
    patched_rom:write(payload_rom, prev_ldr)
    patched_rom:write(payload_rom + 0x60, kz)
    -- kz-lite's overlays are kept in rom right after kz, which finds them there
    local ovl_file = io.open(data .. "kz-ovl.bin", "rb")
    if ovl_file ~= nil then
        ovl_file:close()
        patched_rom:write(payload_rom + 0x60 + ((kz:size() + 15) & ~15), gru.blob_load(data .. "kz-ovl.bin"))
    end
    patched_rom:crc_update()

    return patched_rom, rom_info, kz_version
end
