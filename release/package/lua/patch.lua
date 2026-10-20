-- patch.lua
-- Patches a vanilla Majora's Mask rom with prebuilt kz-lite.
--
-- usage: gru lua/patch.lua <rom> [output]

local args = { ... }
local in_rom = args[1]
local out_rom = args[2]

if in_rom == nil then
    error("usage: gru lua/patch.lua <rom> [output]", 0)
end

dofile("lua/kz.lua")

local ok, patched_rom, rom_info, kz_version = pcall(kz_patch, gru.n64rom_load(in_rom), "lite")
if not ok then
    error(in_rom .. " is " .. patched_rom, 0)
end

if out_rom == nil then
    out_rom = kz_version .. ".z64"
end
print("Saving " .. out_rom)
patched_rom:save(out_rom)
