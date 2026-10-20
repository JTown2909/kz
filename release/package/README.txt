kz with save states (v4)
========================

kz is a practice rom for The Legend of Zelda: Majora's Mask
(https://github.com/krimtonz/kz). This package patches your own copy of the
game. It contains no game data.

  * Wii VC wad  -> kz for the Wii Virtual Console
  * N64 rom     -> kz-lite, for a real N64 (with an Expansion Pak), the
                   Wii U VC and standard emulators

Supported games (unmodified):
  * Wii VC wads: USA and Japan
  * N64 roms (.z64, .n64 or .v64): NTSC-U 1.0, NTSC-J 1.0, NTSC-J 1.1


Patching a Wii VC wad
---------------------
Windows:       drag your wad onto build-wad.bat
               (or copy the wad into this folder and double click build-wad.bat)
Linux:         ./build-wad.sh /path/to/game.wad
macOS:         the same, with gru (https://github.com/glankk/n64) and gzinject
               (https://github.com/krimtonz/gzinject) built and in your PATH

The patched wad appears in this folder as kz-vc-NZSE.wad (USA) or
kz-vc-NZSJ.wad (Japan). It installs as its own channel next to the
original game, so the original is not overwritten.

On the Wii VC the GameCube controller is mapped like this:
  N64 L = C-stick down, so the kz menu is R + C-stick down.
  The GameCube d-pad left/right do not reach the game, so bind save state and
  load state to other buttons in kz's settings -> commands.

Patching an N64 rom (kz-lite)
-----------------------------
Windows:       drag your rom onto build-rom-lite.bat
Linux:         ./build-rom-lite.sh /path/to/rom.z64
macOS:         the same, with gru built and in your PATH

The patched rom appears in this folder as kz-lite-<version>.z64.


Save states
-----------
  D-Left        save state
  D-Right       load state
  R + L         open the kz menu; "states" selects the slot, renames, clears,
                exports/imports, and shows the state's size and free memory

Loading a state puts minigame and event timers (beaver race, shooting
gallery, ...) back to where they were when the state was saved. Timers,
including bottle timers such as hot spring water, stand still while kz is
paused.

kz-lite has very little free memory, so its states are compressed. A typical
state is around 100kb. If a save says "not enough memory", clear a state in
another slot.

SD card
-------
Wii VC: states, memfiles and positions are exported to and imported from
the Wii's SD card.
kz-lite: the same on a 64drive, EverDrive-64 (X-series, v1-v3, ED64+) or
SummerCart64. The card must be FAT32 or FAT16 (not exFAT).

The file browser remembers the folder, scroll position and file you used last,
and handles folders with hundreds of states. States from kz-lite and from the
Wii VC version are not interchangeable.

Macros (kz-lite)
----------------
Like gz's macros: record controller input, then play it back from a state.
  R + L -> macro    record / play / rewind / trim / clear, export / import
The "record macro" and "play macro" commands have no default buttons; bind
them in settings -> commands.


Source: https://github.com/JTown2909/kz
kz and gzinject are licensed under the GNU GPL v3, see LICENSE.txt.
kz-lite's SD card support uses libcart (LICENSE.libcart.txt) and FatFs
(LICENSE.fatfs.txt). The Wii VC version uses homeboy
(https://github.com/krimtonz/homeboy).
