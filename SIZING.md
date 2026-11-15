# wb900: sizing AROS intuition.library on Exec900 (C900)

2026-11-14, measurements to 2026-11-15. Source: AROS master (github.com/aros-development-team/AROS), shallow clone. Line counts are non-blank C lines unless noted.

## 1. Platform constraints

| Item | Fact | Source |
|---|---|---|
| CPU | Z8001 segmented + Z8010 MMU, 6 MHz stock, 10/12/48 MHz turbo on MiSTer | c900-c0 |
| RAM | 512 KB / 1 MB motherboard; optional 2–16 MB RAM card (invented); ~957 KB free under Exec900 on 1 MB | c900-c0, exec816-0f |
| Segments | No object or code hunk may cross 64 KB; pointer arithmetic is 16-bit offset only | exec816-0f |
| Stacks | One shared pool, 63 KB (EXEC_ASKS 5b), its unused RAM lent to the heap; Workbench disks take a 2 KB root stack (§5a); DOS+shell uses ~8.5 KB | exec816-0f |
| Compiler | Mark Williams C: K&R, 16-bit int, 15-char externals; prototypes, `//`, mixed declarations and long switch added (COMPILER_ASKS), long names via `-VLNAME` (EXEC_ASKS 4) | exec816-0f |
| ABI | Stack args, caller pops, result r1/rr0; Amiga LVO jump tables (6-byte JP); non-exec libs take base as hidden first arg | exec816-0f |
| Display | HR card: 128 KB VRAM CPU-mapped at 0x3E0000; jumper-selected 1024x800 or 1024x1024 1bpp (128 B stride), or 640x400 4bpp chunky RGBI (320 B stride, fixed CGA palette). No blitter, HW cursor or vblank IRQ | kernel3 |
| | LC card: 8563/8568 VDC, 640x200 bitmap behind register port, block fill/copy (LC_PLAN.md). LR: text only | kernel3 |
| Mouse | On HR card, quadrature counters at 0x400/0x402, buttons bits 15:13, polled | kernel3 |
| Exec900 has | lists, tasks/signals, ports, libraries (V39), devices, pools, semaphores, interrupt servers, dos.library, timer.device (all units, EXEC_ASKS 3/7), console, keyboard, gameport, framebuf.resource | exec816-0f |
| Exec900 lacks | graphics, layers, input.device, keymap, intuition, workbench/icon, diskfont (wb900 provides all but diskfont); Cause/soft ints (not needed) | exec816-0f |

## 2. Licensing

- AROS intuition, graphics, layers, utility, gadtools, workbench, icon: **AROS Public License 1.1** (MPL 1.1 derivative, file-level copyleft), none in AROS `LEGAL`'s GPL/LGPL exceptions.
- Exec900: **GPL-3.0-only** + a GPL §7 application permission. APL is not GPL-compatible, so APL code never goes into Exec900 sources.
- The permission covers independent programs **and libraries** using Exec's public interfaces (LVOs, message/device interfaces), loaded separately or on the same boot image (exec816-0f). So APL intuition/Workbench in its own repo is fine.
- MIT (`c900/LICENSING.md`, branch `c900/exec900`, 2026-11-14): `include/`, `fd/`, `lib/` startup and amiga.lib helpers, `platform/progabs.s`/`portio.s`, the Exec900 libc DOS layer, and everything genlvo generates (EXEC_ASKS 2a). `tools/genlvo.py` itself stays GPL-3.
- MWC libc is BSD-3: ship `libc/NOTICE` with anything linking it.
- Rules: AROS-derived files stay in their own tree under APL, headers intact, source published; no Exec900 GPL code in them; graphics/layers glue is fresh MIT or AROS APL, never Exec900 internals. Exception: wb/edit is GPL 2 (aros/UPSTREAM.md).
- Reusable MIT code: kernel3 HR drivers and ZView (`commodore-900-coh-userland/hr`: gfx, gfx4, fonts, software cursor).

## 3. AROS code involved

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
| — pluggable decorator loading | decoration.c, changedecorationa.c | ~300 | ported, for C900 decorators later |
| AROS skin decorators | workbench/c/Decoration, workbench/libs/decorator | 6.1k | not ported: need truecolour, alpha, PNG |
| — monitorclass/CGFX/pointer | monitorclass, pointerclass, intuition_arch | 1,781 | dropped |
| graphics.library | rom/graphics | 27.3k raw | ported over the driver table (§8) |
| gfx HIDD + oop.library | rom/hidds/gfx, rom/oop | 25.5k + 6.0k | dropped |
| layers (hyperlayers) | rom/hyperlayers | 7.7k | ported |
| utility.library | rom/utility | 4.5k | ported |
| gadtools.library | workbench/libs/gadtools | 10.5k | ported |
| keymap / input.device | rom/keymap, rom/devs/input | 2.0k / 1.0k | ported |
| workbench + icon libs | workbench/libs/{workbench,icon} | 4.4k + 8.0k | ported (icon: classic .info only) |

Everything: ~135k lines; the subset ~55–65k.

intuition's includes: graphics 66, layers 43, utility 41, alib 34, cybergraphics 7+10, keymap 7, timer 5, oop 4, input 3, hidd 11. 154 of its files use AROS_LH/AROS_UFH register macros.

## 4. Port risks (all addressed)

1. **Compiler.** AROS is ANSI/C99 with GCC extensions, AROS_LH* macros and flat pointers. Addressed by the toolchain additions in COMPILER_ASKS.md; hazards in HAZARDS.md.
2. **No graphics stack.** AROS graphics sits on HIDD/oop. Done: graphics over a per-card driver table (§8).
3. **64 KB segments.** Done: libraries split into ≤64 KB hunks (EXEC_ASKS 2b); intuition is four. 1024x800x1 (100 KB) and 4bpp (125 KB) bitmaps exceed 64 KB, so BitMap row pointers must be segment-aware.
4. **Stack pool.** input.device, intuition's tasks, Workbench and apps share it, and BOOPSI recursion is deep. Pool enlarged (EXEC_ASKS 5b); use measured in §5.
5. **Exec gaps.** See EXEC_ASKS.md. Cause is not needed.
6. **No vblank or HW cursor.** Done: software sprite with save-under (libs/graphics/softsprite.c), as in ZView's hrcur4.c.
7. **Display ownership.** Done: `framebuf.resource` (EXEC_ASKS 1; design in exec816 `c900/docs/design/hr-display-sharing.md`): GetFBInfo/AllocFB/FreeFB. FBInfo has the card and a mode list; each FBMode has fbm_SplitRow/fbm_SplitByte, with FB_ADDR and FB_ROWSPLIT. graphics calls AllocFB at the first LoadView and FreeFB at LoadView(NULL). The kernel reclaims the display on panic.
   - VRAM costs ~7 clocks per access (2–5 wait states) vs 3 for DRAM, so save-under and backing store stay in RAM. A full-screen mono scroll in VRAM is ~0.2 s at 6 MHz (estimate).
   - Port 0x0404 is write-only; owners write only 0x03 (blank), 0x07 (dim) or 0x0F (full). Never word-write a port with A1=0 other than 0x0404 (mouse port 0x0400 included): it loads the latch.
   - No display-start register, so no hardware double buffering; ChangeScreenBuffer must copy.
   - Mono is big-endian (bit 7 of the even byte leftmost). Colour is 2 px per byte, high nibble leftmost, bits I R G B.
8. **CPU budget.** No blitter at 6 MHz makes full-window moves on 1024x800 slow. Default to SIMPLE_REFRESH and outline drags; SMART_REFRESH backing store costs RAM.

## 5. Footprint

Estimated 2026-11-14: AmigaOS 3.1 intuition was ~100 KB of 68k code and Z8000 code is no denser, so full AROS intuition 200–350 KB, the subset 90–150 KB; graphics + layers + utility + keymap + input 60–100 KB; screen bitmap 100–128 KB in VRAM; 200–300 KB RAM for the GUI before Workbench, which fits 1 MB (RAM card advisable for Workbench + icons). utility's measured ~6 B per code line put intuition at ~150–180 KB.

Measured (B; "stack" is the most a task used of its allocation):

| Module | Code | Data | Source | Notes |
|---|---|---|---|---|
| utility.library | 8,066 | 440 | 1,423 code lines | |
| graphics.library | 59,032 | 4,074 | | was 54,828 + 3,802 before pens, VideoControl and View calls |
| layers.library | 29,882 | 422 | hyperlayers, no composited layers | deepest test call ~1.1 KB stack |
| intuition.library | 201,412 in 4 hunks (54,786, 62,380, 64,290, 19,956) | 3,644 | ~38.8k lines with autodocs; toolchain 337f2b6 | without monitorclass, CGX/HIDD, screen notification, screen buffers, IPrefs, alerts, group/model/fillrect/itext/barlabel; with decorators. input task and menu task in §5a |
| gadtools.library | 63,626 | 1,970 | all of it, 8.3k lines | menu task 1,893 (§5a) |
| icon.library | ~21 K | ~2 K | classic .info only | |
| workbench.library | 16 K | 0.4 K | | handler 886 of 1,280 |
| Workbench | 28 K | | | 2,626 of 3,072 |
| asl.library | 126,800 in 3 hunks (52,964, 60,662, 13,174) | 3,488 | all but locale, prefs, truecolour images; 16.1k lines | caller needs ~3.4 K (tests/asl 3,360) |
| commodities.library | 9,960 | 1,950 | | handlers run on the input task: 2,652 of 3,840 in tests/cxtools; three allocs and frees per event per broker |
| NoCapsLock, AutoPoint, ClickToFront, Blanker, DepthMenu | 2.5, 3.1, 3.3, 4.3, 6.6 K | | | 850–1,300 of 2,048 each |
| More | 18.6 K | 1.7 K + 6.0 K bss | | 2,869 with asl's requester open; icon 4,096; files ≤ 65,520 B (one segment) |
| Edit (JanoEditor) | 72.7 K in 2 hunks (62.0, 10.6) | 4.1 K | | as More |
| Calculator | 8.3 K | 0.6 K | | 2,209; icon 2,816 |
| Clock | 6.2 K | 0.6 K | | 1,192; icon 2,048 |
| KeyShow | 6.0 K | 0.9 K + 2.2 K bss | | 1,282; icon 2,048 |
| SysMon | 6.3 K | 0.8 K + 4.6 K bss | | 2,539; icon 3,072 |
| LogView | 5.0 K | 0.4 K | | 2,532; icon 3,072 |


### 5a. Stack budget

Every task stack comes from one pool. Each stack is its measured peak (the emulator tests, `C:StackUse`'s count) plus 384 B, rounded up to 256 B: 256 B of interrupt headroom and 128 B of slack. Workbench raises a tool's icon stack to its 2,048 B default.

| Task | Peak | Was | Now | Deepest path |
|---|---:|---:|---:|---|
| input.device | 3,336 | 4,096 | 3,840 | a window resize redrawing the frame's gadgets |
| Intuition menu handler | 1,893 | 3,072 | 2,304 | menu text; submenus draw no deeper |
| Workbench | 2,626 | 3,584 | 3,072 | OpenScreen; its requesters peak lower |
| Workbench Handler | 886 | 1,536 | 1,280 | LoadSeg and CreateNewProc |
| CON: window process | 3,354 | 4,096 | 3,840 | opening the Workbench screen itself |
| console.device task | 1,336 | 2,048 | 1,792 | refresh |
| con-handler | 328 | 1,024 | 768 | |
| More, Edit (icons; Notes, ReadMe) | 2,869 | 6,144 | 4,096 | asl's Parent: 3,360 from a shallow caller |
| Calculator | 2,209 | 4,096 | 2,816 | |
| Clock, KeyShow | 1,282 | 3,072 | 2,048 | |
| SysMon, LogView | 2,539 | 4,096 | 3,072 | |
| commodities | 1,314 | 2,048 | 2,048 | |

Cuts: asl's Parent and Add go through one path buffer instead of two and its pattern buffer is pooled (asl from 3,874 to 3,360); gadtools' text gadget keeps its text buffer off the frame and label drawing (SysMon −168, More −195); Workbench's scans and commands keep their paths static.

The pool on 1 MB, in bytes, with two v7fs-handlers (long-name disks), a 63 KB segment and a 2 KB root stack:

| | |
|---|---:|
| Pool after the root and interrupt stacks | 60,672 |
| ramlib, 2 × v7fs-handler, ram-handler, con-handler | 6,400 |
| input.device, Workbench, its handler, menu task | 10,496 |
| Shell: CON: window, console.device task, Shell process | 7,680 |
| Left: 9 Shell commands of 4,000 B (exec816), or Clock, SysMon, More and 6 | 36,096 |

The segment's unused RAM serves ordinary allocations once the heap is full, so the Workbench disks keep the default 63 KB and boot with `mkdisk --rootstack=2` (shellwin and wbmulti keep 4 KB: shellwin's root program opens CON:). tests/wbmulti runs Workbench, a Shell, Clock, SysMon and More (with both v7fs-handlers and the test's two 2,048 B helpers): 20,480 B of pool stay free and 25 KB of memory.

## 6. Phasing (done)

1. Toolchain probe and multi-hunk library loading.
2. utility, graphics on HR 1bpp, software pointer, layers.
3. input.device and keymap on the keyboard/gameport devices.
4. Intuition core: screens, windows, IDCMP, legacy gadgets, menus, requesters.
5. BOOPSI classes, then gadtools.
6. A small native Workbench (AROS Wanderer needs Zune/MUI, too heavy) on the workbench/icon libraries.

Next: BACKLOG.md, LC_PLAN.md.

## 7. Test targets

- MiSTer C900 core at 48 MHz with the HR card (c900-c0 hands out slots); HD image or network boot.
- `~/git/commodore-900-emulator --hr` for fast loops.
- Go sim (`~/git/C900/sim`) only for traces, and only when the user allows. It mis-aliases HR segments not on a 64 KB boundary.

## 8. Video abstraction (decided 2026-11-14: option C, built)

intuition → graphics.library → driver table (`LIBS:hr.gfxdriver`) → framebuf.resource (ownership and mode info only) → card.

| Option | Driver layer | Reuse | Cost/risk |
|---|---|---|---|
| A. Native graphics | none: per-format code in graphics (1bpp, 4bpp chunky, later LC) | little AROS graphics; ZView primitives | each new card edits graphics |
| B. AROS as is | oop.library + gfx.hidd, plus hr.hidd and lc.hidd | most of AROS graphics (45 of 204 files call HIDD directly) | ~58k lines; OOP dispatch slow at 6 MHz; deep stacks |
| **C. AROS graphics + thin driver table (chosen)** | a C function table per display (fill, blit, copy, glyph, pixel, line, palette, mode list), one module per card | most of AROS graphics, ~58 HIDD_* uses rewritten to the table | small dispatch cost; swappable drivers; we own the table |
