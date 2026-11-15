# wb900: initial sizing of AROS intuition.library on Exec900 (C900)

Date: 2026-11-14. Source: AROS master (github.com/aros-development-team/AROS), shallow clone.
Line counts below are non-blank C lines unless noted.

## 1. Platform facts that constrain the port

| Item | Fact | Source |
|---|---|---|
| CPU | Z8001 segmented + Z8010 MMU, 6 MHz stock, 10/12/48 MHz turbo on MiSTer | c900-c0 |
| RAM | 512 KB / 1 MB motherboard; optional 2–16 MB RAM card (invented); ~957 KB free under Exec900 on 1 MB | c900-c0, exec816-0f |
| Segments | No object or code hunk may cross 64 KB; pointer arithmetic is 16-bit offset only | exec816-0f |
| Stacks | All task stacks share one 32 KB pool (seg 0x3F); DOS+shell uses ~8.5 KB | exec816-0f |
| Compiler | Mark Williams C: K&R only, no prototypes, no C99/GCC extensions, 16-bit int, 15-char externals | exec816-0f |
| ABI | Stack args, caller pops, result r1/rr0; Amiga LVO jump tables (6-byte JP); non-exec libs take base as hidden first arg | exec816-0f |
| Display | HR card: 128 KB VRAM CPU-mapped at 0x3E0000; jumper-selected 1024x800 or 1024x1024 1bpp (128 B stride), or 640x400 4bpp chunky RGBI (320 B stride, fixed CGA palette). No blitter, no HW cursor, no vblank IRQ | kernel3 |
| | LC card: 8563/8568 VDC, 640x200 bitmap behind register port, block fill/copy. LR: text only | kernel3 |
| Mouse | On HR card, quadrature counters at 0x400/0x402, buttons bits 15:13, polled (no IRQ) | kernel3 |
| Exec900 has | lists, tasks/signals, ports, libraries (V39), devices, pools, semaphores, interrupt servers, dos.library, timer.device (VBLANK 100 Hz only), console, keyboard, gameport | exec816-0f |
| Exec900 lacks | graphics, layers, input.device, keymap, intuition, workbench/icon, diskfont; Cause/soft ints; timer MICROHZ/TR_GETSYSTIME | exec816-0f |

## 2. Licensing

- AROS intuition, graphics, layers, utility, gadtools, workbench, icon: **AROS Public License 1.1** (MPL 1.1 derivative, file-level copyleft). None of these dirs are in AROS `LEGAL`'s GPL/LGPL exception lists.
- Exec900: **GPL-3.0-only** + a GPL §7 application permission; public headers/bindings (`c/`, `abi/`, generated LVO stubs) are **MIT**.
- MPL 1.1/APL is not GPL-compatible, so APL code cannot be merged into GPL Exec900 sources.
- exec816-0f confirms the permission covers independent programs **and libraries** that use Exec's public interfaces (LVO jump tables, message/device interfaces), whether loaded separately or on the same boot image. An APL intuition/Workbench in its own repo, called through Exec900 LVOs, is fine.
- Settled 2026-11-14 (`c900/LICENSING.md`, branch `c900/exec900`): Exec900's `include/` headers, `fd/` files, genlvo-generated call macros, `lib/` startup and amiga.lib helpers, `platform/progabs.s`/`portio.s`, and the Exec900-authored libc DOS layer are MIT. We can build against them.
- Exception: `tools/genlvo.py`, and the vector tables and implementation prototypes it generates for a library's *own* build, stay GPL-3. intuition/graphics/layers need their own jump-table generator (MIT/APL) rather than genlvo's library-side output.
- MWC libc is BSD-3. Ship `libc/NOTICE` with anything that links it.
- Rules that follow: keep AROS-derived files in their own tree under APL with headers intact and source published; never copy Exec900 GPL code into them; write graphics/layers glue fresh (MIT) or take it from AROS (APL), not from Exec900 internals.
- Reusable MIT code: kernel3 HR drivers and ZView (`commodore-900-coh-userland/hr`: gfx, gfx4, fonts, software cursor).

## 3. Size of the AROS code involved

| Component | AROS path | C lines | Needed? |
|---|---|---|---|
| intuition.library | rom/intuition | 39,051 (49.7k raw) | yes |
| — BOOPSI classes | *class*.c | 9,730 | partial |
| — input handler | inputhandler*.c | 4,486 | yes |
| — screens/windows | openscreen/openwindow/windowaction/... | 4,629 | yes |
| — legacy gadgets | bool/prop/str gadgets | 2,711 | yes |
| — requesters | *request*, sysreq* | 2,230 | yes |
| — menus | menu*.c | 1,972 | yes |
| — decorator classes (default look) | windecorclass, scrdecorclass, menudecorclass, decorsupport | 1,891 | **keep**: these draw the borders, title bars, system gadgets and menus |
| — pluggable decorator loading | decoration.c, changedecorationa.c | ~300 | keep: lets us add C900 decorators later |
| AROS skin decorators | workbench/c/Decoration, workbench/libs/decorator | 6.1k | don't port: need truecolour, alpha, PNG via datatypes |
| — monitorclass/CGFX/pointer | monitorclass, pointerclass, intuition_arch | 1,781 | drop/replace |
| graphics.library | rom/graphics | 27.3k raw | port over a thin per-card driver table (§8) |
| gfx HIDD + oop.library | rom/hidds/gfx, rom/oop | 25.5k + 6.0k | drop |
| layers (hyperlayers) | rom/hyperlayers | 7.7k | port |
| utility.library | rom/utility | 4.5k | port (tags, hooks) |
| gadtools.library | workbench/libs/gadtools | 10.5k | later |
| keymap / input.device | rom/keymap, rom/devs/input | 2.0k / 1.0k | port |
| workbench + icon libs | workbench/libs/{workbench,icon} | 4.4k + 8.0k | later |

Total if everything is ported: ~135k lines. The realistic subset below is ~55–65k.

AROS intuition depends on (number of includes): graphics 66, layers 43, utility 41, alib 34, cybergraphics 7+10, keymap 7, timer 5, oop 4, input 3, hidd 11. 154 of its files use AROS_LH/AROS_UFH register-argument macros.

## 4. Port risks, in order

1. **Compiler.** AROS uses ANSI C/C99, GCC extensions, AROS_LH* register macros and flat pointers. MWC is K&R only. Every file needs mechanical de-ANSI-fication and macro replacement, or a different Z8000 compiler.
2. **No graphics stack.** AROS graphics sits on HIDD/oop. We port graphics over a thin per-card driver table instead (§8).
3. **64 KB segments.** Intuition's code will not fit one hunk, so the library has to be split across hunks; cross-hunk jump tables need checking with exec816. 1024x800x1 bitmaps (100 KB) and 4bpp screens (125 KB) exceed 64 KB, so BitMap row pointers must be segment-aware.
4. **32 KB stack pool.** input.device + intuition + its menu and screen-notify tasks + Workbench + apps. Intuition's deep BOOPSI dispatch recursion is stack-heavy.
5. **Exec gaps.** See EXEC_ASKS.md. The main ones are HR display sharing, disk-loaded multi-hunk libraries, TR_GETSYSTIME/GetSysTime, longer external names and the stack budget. Cause is not needed.
6. **No vblank or HW cursor.** The pointer has to be a software sprite drawn by the input handler (as in ZView's hrcur4.c), with save-under, removed and replaced around every render.
7. **Display ownership** (design: exec816 `c900/docs/design/hr-display-sharing.md`). `framebuf.resource`: GetFBInfo/AllocFB/FreeFB. FBInfo has the card and a mode list; each FBMode has fbm_SplitRow/fbm_SplitByte, with FB_ADDR and FB_ROWSPLIT helpers. graphics calls AllocFB at the first LoadView and FreeFB at LoadView(NULL). The kernel reclaims the display on panic.
   - VRAM costs ~7 clocks per access (2–5 wait states) vs 3 for DRAM. Keep save-under and backing store in RAM. A full-screen mono scroll in VRAM is roughly 0.2 s at 6 MHz (estimate).
   - Port 0x0404 is write-only. Owners write only 0x03 (blank), 0x07 (dim) or 0x0F (full). Never word-write any port with A1=0 other than 0x0404 (including the mouse port 0x0400), because that loads the latch.
   - No display-start register, so no hardware double buffering. ChangeScreenBuffer must copy.
   - Mono is big-endian (bit 7 of the even byte is leftmost). Colour is 2 px per byte, high nibble leftmost, bits I R G B.
8. **CPU budget.** 6 MHz with no blitter means full-window moves of a 1024x800 screen are slow. Default to SIMPLE_REFRESH and outline drags; SMART_REFRESH backing store costs RAM.

## 5. Rough footprint (estimate, not measured)

- AmigaOS 3.1 intuition.library was ~100 KB of 68k code. AROS's is larger, and Z8000 segmented code is no denser.
- Full AROS intuition on MWC: likely 200–350 KB. Subset (decorators and decorator loading kept; no monitorclass or CGFX; minimal BOOPSI): ~90–150 KB.
- New graphics + layers + utility + keymap + input: ~60–100 KB.
- Screen bitmap 100–128 KB (in VRAM, so free).
- About 200–300 KB of RAM for the GUI stack before Workbench. That fits a 1 MB machine; the RAM card is advisable for Workbench + icons.
- **Measured 2026-11-14:** utility.library is 8,066 B code + 440 B data from 1,423 code lines, so about **6 bytes per line**. Applied to intuition's ~25–30k code lines (excluding doc comments), the full library comes to ~150–180 KB, and the subset to well under that. This is consistent with the earlier range.
- **Measured 2026-11-14:** graphics.library is 54,828 B code + 3,802 B data; layers.library (hyperlayers without composited layers) is 29,882 B code + 422 B data, one hunk each. layers' deepest call in its test took about 1.1 KB of stack.
- **Measured 2026-11-14 (toolchain 337f2b6):** intuition.library (AROS rom/intuition without monitorclass, CGX/HIDD, screen notification, screen buffers, IPrefs, alerts and the group/model/fillrect/itext/barlabel classes; with the decorator classes and decorator loading) is 201,412 B code in four hunks of 54,786, 62,380, 64,290 and 19,956 B, plus 3,644 B data, from about 38.8k lines (autodocs included). graphics.library grew to 59,032 B code + 4,074 B data with the pen, VideoControl and View calls. In tests/intuition the input task, which runs intuition's handler, used 2,580 B of its stack (now 4,096 B, was 1,536 B) and the menu task 1,209 B of 2,048 B.
- **Measured 2026-11-15:** gadtools.library (all of AROS workbench/libs/gadtools) is 63,626 B code in one hunk plus 1,970 B data, from 8.3k lines. In tests/gadtools the input task used 2,571 B of its 4,096 B stack and the menu task 2,045 B, so its stack is now 3,072 B (was 2,048 B).

- **Measured 2026-11-15:** icon.library (classic .info icons only) is about 21 KB code + 2 KB data; workbench.library 16 KB + 0.4 KB, and its handler process uses 882 of 1,536 B of stack. The Workbench program is a 28 KB hunk; its process uses about 2.6 KB of its 3.5 KB stack in the self test.

## 6. Suggested phasing

1. Toolchain probe: K&R-convert 3 intuition files, compile, and measure size and stack use. Confirm multi-hunk library loading with exec816.
2. utility.library (tags/hooks) + minimal graphics.library on HR 1bpp + software pointer + layers port.
3. input.device + keymap, fed from the existing keyboard/gameport devices.
4. Intuition core: screens, windows, IDCMP, legacy gadgets, menus, requesters.
5. BOOPSI root/image/gadget classes, then gadtools.
6. Workbench: write a small native one (AROS Wanderer is Zune/MUI-based and too heavy) on workbench/icon libs.

## 7. Test targets

- MiSTer C900 core at 48 MHz with the HR card (c900-c0 hands out slots); iterate via HD image or network boot.
- `~/git/commodore-900-emulator` with `--hr` for fast loops.
- Go sim (`~/git/C900/sim`) only when a trace is needed and the user allows it. It mis-aliases HR segments not on a 64 KB boundary.

## 8. Video abstraction (decided 2026-11-14: option C)

Layers, top down: intuition → graphics.library → [driver layer] → framebuf.resource (Exec: ownership and mode info only) → card.

| Option | Driver layer | Reuse | Cost/risk |
|---|---|---|---|
| A. Native graphics | none: graphics has per-format code (1bpp, 4bpp chunky, later LC) | little AROS graphics; ZView primitives | each new card means editing graphics |
| B. AROS as is | oop.library + gfx.hidd, plus hr.hidd and lc.hidd drivers | most of AROS graphics (45 of 204 files call HIDD directly) | ~58k lines; per-call OOP dispatch is slow at 6 MHz; deep stacks |
| **C. AROS graphics + thin driver table (chosen)** | a C function table per display (fill, blit, copy, glyph, pixel, line, palette, mode list), loaded as a module per card | most of AROS graphics, with the ~58 HIDD_* uses rewritten to the table | small dispatch cost; drivers swappable; we own the table design |
