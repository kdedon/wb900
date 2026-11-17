# Porting AROS to the C900 and Exec900

Source: AROS master (github.com/aros-development-team/AROS). Line counts are non-blank C lines unless noted.

## Platform

| Item | Fact |
|---|---|
| CPU | Z8001 segmented + Z8010 MMU, 6 MHz stock, 10/12/48 MHz turbo on MiSTer |
| RAM | 512 KB / 1 MB motherboard; optional 2–16 MB RAM card (invented); ~957 KB free under Exec900 on 1 MB |
| Segments | No object or code hunk may cross 64 KB; pointer arithmetic is 16-bit offset only |
| Stacks | One shared pool, 63 KB (exec900.md #5b), its unused RAM lent to the heap; Workbench disks take a 2 KB root stack; DOS+shell uses ~8.5 KB |
| Compiler | Mark Williams C: K&R, 16-bit int, 15-char externals; prototypes, `//`, mixed declarations and long switch added (compiler.md), long names via `-VLNAME` (exec900.md #4) |
| ABI | Stack args, caller pops, result r1/rr0; Amiga LVO jump tables (6-byte JP); non-exec libs take base as hidden first arg |
| Display | HR card: 128 KB VRAM CPU-mapped at 0x3E0000; jumper-selected 1024x800 or 1024x1024 1bpp (128 B stride), or 640x400 4bpp chunky RGBI (320 B stride, fixed CGA palette). No blitter, HW cursor or vblank IRQ |
| | LC card: 8563/8568 VDC, 640x200 bitmap behind register port, block fill/copy (lc.md). LR: text only |
| Mouse | On HR card, quadrature counters at 0x400/0x402, buttons bits 15:13, polled |
| Exec900 has | lists, tasks/signals, ports, libraries (V39), devices, pools, semaphores, interrupt servers, dos.library, timer.device (all units, exec900.md #3, #7), console, keyboard, gameport, framebuf.resource |
| Exec900 lacks | graphics, layers, input.device, keymap, intuition, workbench/icon, diskfont (wb900 provides them); Cause/soft ints (not needed) |

## Licensing

- AROS intuition, graphics, layers, utility, gadtools, workbench, icon: **AROS Public License 1.1** (MPL 1.1 derivative, file-level copyleft), none in AROS `LEGAL`'s GPL/LGPL exceptions.
- Exec900: **GPL-3.0-only** + a GPL §7 application permission. APL is not GPL-compatible, so APL code never goes into Exec900 sources.
- The permission covers independent programs **and libraries** using Exec's public interfaces (LVOs, message/device interfaces), loaded separately or on the same boot image. So APL intuition/Workbench in its own repo is fine.
- MIT (Exec900's `c900/LICENSING.md`, branch `c900/exec900`): `include/`, `fd/`, `lib/` startup and amiga.lib helpers, `platform/progabs.s`/`portio.s`, the Exec900 libc DOS layer, and everything genlvo generates (exec900.md #2a). `tools/genlvo.py` itself stays GPL-3.
- MWC libc is BSD-3: ship `libc/NOTICE` with anything linking it.
- AROS-derived files stay in their own tree under APL, headers intact, source published, with no Exec900 GPL code. graphics/layers glue is fresh MIT or AROS APL, never Exec900 internals. Exception: wb/edit is GPL 2 (aros/UPSTREAM.md).
- Fonts (wb/fonts): `fixed` 8 is Daniel Hepper's font8x8_basic (github.com/dhepper/font8x8, public domain, after Marcel Sondaar's public-domain font8x8), as Exec900 ships it; `fixed` 11 and 16 are made from it; `sans` 9 is drawn for wb900, CC0 1.0. No Commodore fonts.
- Reusable MIT code: the HR drivers and ZView in commodore-900-coh-userland (`hr`: gfx, gfx4, fonts, software cursor).

## AROS code

| Component | AROS path | C lines | Status |
|---|---|---|---|
| intuition.library | rom/intuition | 39,051 (49.7k raw) | ported |
| — BOOPSI classes | *class*.c | 9,730 | ported, less group/model/fillrect/itext/barlabel |
| — input handler | inputhandler*.c | 4,486 | ported |
| — screens/windows | openscreen/openwindow/windowaction/... | 4,629 | ported |
| — legacy gadgets | bool/prop/str gadgets | 2,711 | ported |
| — requesters | *request*, sysreq* | 2,230 | ported |
| — menus | menu*.c | 1,972 | ported |
| — decorator classes (default look) | windecorclass, scrdecorclass, menudecorclass, decorsupport | 1,891 | ported: borders, title bars, system gadgets, menus |
| — pluggable decorator loading | decoration.c, changedecorationa.c | ~300 | ported |
| AROS skin decorators | workbench/c/Decoration, workbench/libs/decorator | 6.1k | not ported: need truecolour, alpha, PNG |
| — monitorclass/CGFX/pointer | monitorclass, pointerclass, intuition_arch | 1,781 | dropped |
| graphics.library | rom/graphics | 27.3k raw | ported over the driver table |
| gfx HIDD + oop.library | rom/hidds/gfx, rom/oop | 25.5k + 6.0k | dropped |
| layers (hyperlayers) | rom/hyperlayers | 7.7k | ported |
| utility.library | rom/utility | 4.5k | ported |
| gadtools.library | workbench/libs/gadtools | 10.5k | ported |
| keymap / input.device | rom/keymap, rom/devs/input | 2.0k / 1.0k | ported |
| workbench + icon libs | workbench/libs/{workbench,icon} | 4.4k + 8.0k | ported (icon: classic .info only) |
| diskfont.library | workbench/libs/diskfont | 4.1k raw | ported, 1.8k raw: bitmap fonts, scaling; no outline, colour, tags or font cache file |

Everything: ~135k lines; the subset ~55–65k. 154 intuition files use AROS_LH/AROS_UFH register macros.

Workbench and the tools are native, on intuition and gadtools.

## Porting approach

1. **Compiler.** AROS is ANSI/C99 with GCC extensions, AROS_LH* macros and flat pointers. The toolchain gained what it needed; requests and conversion hazards are in compiler.md.
2. **Graphics.** AROS graphics sits on HIDD/oop; here it sits on a per-card driver table (see Video).
3. **64 KB segments.** Libraries are split into ≤64 KB hunks (exec900.md #2b); intuition is four. 1024x800x1 (100 KB) and 4bpp (125 KB) bitmaps exceed 64 KB, so BitMap row pointers are segment-aware.
4. **Stack pool.** input.device, intuition's tasks, Workbench and apps share it, and BOOPSI recursion is deep. See Stacks.
5. **Exec gaps.** See exec900.md.
6. **No vblank or HW cursor.** Software sprite with save-under (libs/graphics/softsprite.c), as in ZView's hrcur4.c.
7. **Display ownership.** `framebuf.resource` (exec900.md #1; design in Exec900's `c900/docs/design/hr-display-sharing.md`): GetFBInfo/AllocFB/FreeFB. FBInfo has the card and a mode list; each FBMode has fbm_SplitRow/fbm_SplitByte, with FB_ADDR and FB_ROWSPLIT. graphics calls AllocFB at the first LoadView and FreeFB at LoadView(NULL). The kernel reclaims the display on panic.
   - VRAM costs ~7 clocks per access (2–5 wait states) vs 3 for DRAM, so save-under and backing store stay in RAM. A full-screen mono scroll in VRAM is ~0.2 s at 6 MHz (estimate).
   - Port 0x0404 is write-only; owners write only 0x03 (blank), 0x07 (dim) or 0x0F (full). Never word-write a port with A1=0 other than 0x0404 (mouse port 0x0400 included): it loads the latch.
   - No display-start register. The front screen's bitmap (or screen buffer) has VRAM and draws there; the others in its mode are RAM bitmaps of the full mode size. Bringing one to the front swaps its pixels and planes with VRAM's: no extra buffer, about 0.27 s for 100 KB mono and 0.35 s for 125 KB RGBI at 6 MHz (modelled cycles, before VRAM wait states). On LC the shadow moves to the front screen's bitmap and all 16 KB is pushed, about 0.24 s. A screen on a custom bitmap of another size is copied in, the VRAM screen first moved to a RAM bitmap allocated for the time.
   - Mono is big-endian (bit 7 of the even byte leftmost). Colour is 2 px per byte, high nibble leftmost, bits I R G B.
8. **CPU budget.** No blitter at 6 MHz makes full-window moves on 1024x800 slow. Default to SIMPLE_REFRESH and outline drags; SMART_REFRESH backing store costs RAM.

## Video

intuition → graphics.library → driver table (`LIBS:hr.gfxdriver`) → framebuf.resource (ownership and mode info only) → card.

The driver table is a C function table per display (fill, blit, copy, glyph, pixel, line, palette, mode list), one module per card. Most of AROS graphics is kept; its ~58 HIDD_* uses call the table instead. Rejected:

- Native per-format code in graphics: little AROS reuse, and each new card edits graphics.
- AROS oop.library + gfx.hidd with hr/lc HIDDs: ~58k lines, OOP dispatch slow at 6 MHz, deep stacks.

## Sizes

Bytes. "Stack" is the most a task used of its allocation.

| Module | Code | Data | Source | Stack, notes |
|---|---|---|---|---|
| utility.library | 8,066 | 440 | 1,423 code lines | |
| graphics.library | 59,032 | 4,074 | | |
| layers.library | 29,882 | 422 | hyperlayers, no composited layers | deepest test call ~1.1 KB |
| intuition.library | 215,016 in 4 hunks (57,726, 63,016, 65,136, 29,138) | 4,276 | ~38.8k lines with autodocs; toolchain b8ec7d4 | without monitorclass, CGX/HIDD, screen notification, group/model/fillrect/itext/barlabel; with decorators |
| gadtools.library | 63,626 | 1,970 | all of it, 8.3k lines | |
| icon.library | ~21 K | ~2 K | classic .info only | |
| workbench.library | 16 K | 0.4 K | | handler 886 of 1,280 |
| Workbench | 32 K | | | 2,626 of 3,072 |
| asl.library | 126,800 in 3 hunks (52,964, 60,662, 13,174) | 3,488 | all but locale, prefs, truecolour images; 16.1k lines | caller needs ~3.4 K (tests/asl 3,360) |
| commodities.library | 9,960 | 1,950 | | handlers run on the input task: 2,652 of 3,840 in tests/cxtools; three allocs and frees per event per broker |
| NoCapsLock, AutoPoint, ClickToFront, Blanker, DepthMenu | 2.5, 3.1, 3.3, 4.3, 6.6 K | | | 850–1,300 of 2,048 each |
| More | 19.9 K | 1.6 K + 6.2 K bss | | 2,869 with asl's requester open; icon 4,096; reads through a window of up to 16 KB with 4 B per line, so 300 KB takes 36 KB; the scroller works to 65,537 lines |
| Edit (JanoEditor) | 73.8 K in 2 hunks (62.7, 11.1) | 4.0 K | | as More; holds the file in buffers of whole lines (16 KB, up to 64 KB for a long line) plus 24 B per line: about 120 KB opens on 1 MB beside Workbench |
| Calculator | 8.3 K | 0.6 K | | 2,209; icon 2,816 |
| Clock | 6.2 K | 0.6 K | | 1,192; icon 2,048 |
| KeyShow | 6.0 K | 0.9 K + 2.2 K bss | | 1,282; icon 2,048 |
| SysMon | 6.3 K | 0.8 K + 4.6 K bss | | 2,539; icon 3,072 |
| LogView | 5.0 K | 0.4 K | | 2,532; icon 3,072 |

## Stacks

Every task stack comes from one pool. Each stack is its measured peak (emulator tests, `C:StackUse`) plus 384 B, rounded up to 256 B: 256 B of interrupt headroom and 128 B of slack. Workbench raises a tool's icon stack to its 2,048 B default.

| Task | Peak | Stack | Deepest path |
|---|---:|---:|---|
| input.device | 3,336 | 3,840 | a window resize redrawing the frame's gadgets |
| Intuition menu handler | 1,893 | 2,304 | menu text; submenus draw no deeper |
| Workbench | 2,626 | 3,072 | OpenScreen; its requesters peak lower |
| Workbench Handler | 886 | 1,280 | LoadSeg and CreateNewProc |
| CON: window process | 3,354 | 3,840 | opening the Workbench screen itself |
| console.device task | 1,336 | 1,792 | refresh |
| con-handler | 328 | 768 | |
| More, Edit (icons; Notes, ReadMe) | 2,869 | 4,096 | asl's Parent: 3,360 from a shallow caller |
| Calculator | 2,209 | 2,816 | |
| Clock, KeyShow | 1,282 | 2,048 | |
| SysMon, LogView | 2,539 | 3,072 | |
| commodities | 1,314 | 2,048 | |

To keep peaks low, asl's Parent and Add share one path buffer and pool the pattern buffer, gadtools' text gadget keeps its text buffer off the stack, and Workbench's scans and commands keep their paths static.

The pool on 1 MB, in bytes, with two v7fs-handlers (long-name disks), a 63 KB segment and a 2 KB root stack:

| | |
|---|---:|
| Pool after the root and interrupt stacks | 60,672 |
| ramlib, 2 × v7fs-handler, ram-handler, con-handler | 6,400 |
| input.device, Workbench, its handler, menu task | 10,496 |
| Shell: CON: window, console.device task, Shell process | 7,680 |
| Left: 9 Shell commands of 4,000 B, or Clock, SysMon, More and 6 | 36,096 |

The segment's unused RAM serves ordinary allocations once the heap is full, so the Workbench disks keep the default 63 KB and boot with `mkdisk --rootstack=2` (shellwin and wbmulti keep 4 KB: shellwin's root program opens CON:). tests/wbmulti runs Workbench, a Shell, Clock, SysMon and More (with both v7fs-handlers and the test's two 2,048 B helpers): 20,480 B of pool stay free and 25 KB of memory.

## Test targets

- MiSTer C900 core at 48 MHz with the HR card; HD image or network boot.
- commodore-900-emulator `--hr` for fast loops.
- The Go simulator (C900 `sim`) only for traces. It mis-aliases HR segments not on a 64 KB boundary.
