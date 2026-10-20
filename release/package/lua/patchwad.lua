-- patchwad.lua
-- Patches a vanilla Majora's Mask Wii VC wad with prebuilt kz. This is
-- build/lua/makewad.lua from kz, using the prebuilt files in data/.
--
-- usage: gru lua/patchwad.lua <gzinject> <wad> [output]
-- wadwork/common-key.bin must already exist (gzinject -a genkey).

local args = { ... }
local gzinject = args[1]
local in_wad = args[2]
local out_wad = args[3]

if in_wad == nil then
    error("usage: gru lua/patchwad.lua <gzinject> <wad> [output]", 0)
end

dofile("lua/kz.lua")

local work = "wadwork"
local key = work .. "/common-key.bin"
local extract = work .. "/wadextract"
local romc = extract .. "/content5/romc"

local function run(cmd)
    local ok = os.execute(cmd)
    if not ok then
        error("command failed: " .. cmd, 0)
    end
end

print("Extracting " .. in_wad)
run(gzinject .. " -a extract -w \"" .. in_wad .. "\" -d \"" .. extract .. "\" -k \"" .. key .. "\" --cleanup")
run(gzinject .. " -a romc -m \"" .. romc .. "\" -o \"" .. work .. "/rom.z64\"")

local ok, patched_rom, rom_info, kz_version = pcall(kz_patch, gru.n64rom_load(work .. "/rom.z64"), "vc")
if not ok then
    error(in_wad .. " does not contain a supported rom: " .. patched_rom, 0)
end

patched_rom:write(4, patched_rom)
patched_rom:write32be(0, 0x08000000)
patched_rom:save(romc)
os.remove(work .. "/rom.z64")

if out_wad == nil then
    out_wad = kz_version .. ".wad"
end

local vc = "data/vc/" .. rom_info.vc_fix_inject:match("[^/]+$") .. "/"
local gzi = "data/gzi/" .. rom_info.wad_patch:match("[^/]+$")

print("Packing " .. out_wad)
run(gzinject ..
    " -d \"" .. extract .. "\"" ..
    " -k \"" .. key .. "\"" ..
    " -a pack" ..
    " -w \"" .. out_wad .. "\"" ..
    " -i " .. rom_info.title_id ..
    " -t \"" .. rom_info.title .. "\"" ..
    " -p \"" .. gzi .. ".gzi\"" ..
    " -p \"" .. gzi .. "-remap.gzi\"" ..
    " -p \"" .. vc .. "kz-vc.gzi\"" ..
    " --dol-inject \"" .. vc .. "kz-vc.bin\"" ..
    " --dol-loading \"" .. rom_info.vc_fix_addr .. "\"" ..
    " --dol-inject \"" .. vc .. "homeboy.bin\"" ..
    " --dol-loading \"" .. rom_info.hb_addr .. "\"" ..
    " --dol-after 1" ..
    " -p \"data/gzi/compress.gzi\"" ..
    " --cleanup")
