# Exec900 requests from wb900

Opened 2026-11-14. Status reported by exec816-0f the same day. Exec work is a plan, not a commitment: the user approves each piece.

## Status

| # | Request | Status | exec816 plan order |
|---|---|---|---|
| 6 | Periodic interrupt for pointer/mouse | **Done**: `AddIntServer(INTV_TICK, is)` joins the 100 Hz CIO tick chain. `INTB_VERTB` alias offered | — |
| 8 | Gameport trigger, key-up, qualifiers | **Done**: GPD_SET/ASKTRIGGER (Amiga semantics); `IECODE_UP_PREFIX` and `IEQUALIFIER_*`. Keyboard not yet board-tested | — |
| 5a | Stack high-water reporting | **Done**: `C:StkUse`; guard word at `tc_SPLower` (AN_StackProbe) | — |
| 3, 7 | TR_GETSYSTIME/SETSYSTIME, GetSysTime/AddTime/SubTime/CmpTime, UNIT_MICROHZ | **Landed** cfd765bd (merged 7b940540): AmigaOS 3.x timer.device, all units, TR_GETSYSTIME/SETSYSTIME, AddTime/SubTime/CmpTime/ReadEClock/GetSysTime. Requests complete at the 100 Hz tick | done |
| 2a | OpenLibrary/OpenDevice fall back to LIBS:/DEVS: | **Landed** 52ac08f4: ramlib-style loading; a Task goes via a ramlib process; low-memory handler expunges. Recipe: platform.md "Building a disk library", example tests/hunks/dlt.c. ExecBase grew (EXECBASESIZE 130): rebuild against the new execbase.h. Everything genlvo.py generates (incl. `--table`, implementation prototypes, asm vector tables) is now MIT; genlvo.py itself stays GPL | done |
| 2b | Library code over 64 KB | **Landed** 122a8dc8: `mkhunk.py --split --ld=$LD [--entry=start] OUT objs... [libs.a] progabs.o` packs objects into groups of 64 KB or less, one code hunk each, with RELOC32 between them. Limits: each function/object fits 64 KB; no hand-written CALR/JR across groups; run symcheck.py over all sources (truncation merges). Docs: platform.md, Program loading, "Over 64 KB" | done |
| 5b | Bigger stack pool | **Landed**: sized at boot. More than 1 MB RAM gets 63 KB (~58 KB free for Tasks); 1 MB gets 32 KB. `mkdisk.py --stack=KB` overrides. `C:StackUse` lists per-Task use and pool free. ExecBase is now EXECBASESIZE 158 (TaskReady/TaskWait): rebuild | done |
| 1 | Shared HR display | **Landed** f5db3a15: framebuf.resource (GetFBInfo −6, AllocFB −12, FreeFB −18), current mode only, 1024x1024 reported as 1024x800. gameport.dev is its own module. Recipe: c900/docs/testing.md "Graphics test boot disk" | done |
| 4 | Long external names | **Landed**: toolchain 39a93e9 (`-VLNAME`: cc2 VLNAME, as/ld -N, up to 127 characters); mkhunk/symcheck `--lname` (3de4a308). wb900 builds with them | done |
| 9 | `<devices/inputevent.h>`: the NDK's `ie_dead` member of `ie_position` and the `ie_Prev1DownCode`/`ie_Prev1DownQual`/`ie_Prev2DownCode`/`ie_Prev2DownQual` macros | **Landed** 757b8066: ie_dead, the ie_Prev* macros and the full NDK set; alerts.h gets AN_*/AG_*/AO_* | done |
| 10 | keyboard.device: Amiga raw key codes (table below) | **Landed** 757b8066: Amiga raw codes (key-up 0x80, keypad with NUMERICPAD). C900-only keys: KP 00 0x0E, CE 0x1C, W1 0x2B, W2 0x2C, W3 0x3B, F13-F15 0x6B-0x6D. `<devices/rawkeycodes.h>` matches AROS. Not yet tried on a real keyboard | done |
| 11 | `MEMF_SEM_PROTECTED` for pools (AROS/MorphOS): a pool created with it serialises its own `AllocPooled`/`FreePooled`/`DeletePool` with a semaphore, so several Tasks can share it | **Landed** f5db3a15: MEMF_SEM_PROTECTED = 1L<<22 (AROS bit 20 is MEMF_LINEAR here) | done |
| 12 | `AllocVecPooled(pool, size)` / `FreeVecPooled(pool, mem)` (AROS exec 149/150, also MorphOS and AmigaOS 4): `AllocPooled` that keeps the size, so `FreeVecPooled` needs none | **Landed** 5286bbba: AllocVecPooled −894, FreeVecPooled −900; 8-byte header keeps 8-byte alignment; FreeVecPooled needs the owning pool | done |
| 13 | `<devices/inputevent.h>`: `IECLASS_NEWMOUSE` (0x16), AROS's class for wheel events (`NM_WHEEL_*` codes in `ie_Code`) | **Landed** 9a6e75b2: IECLASS_NEWMOUSE 0x16, <devices/newmouse.h> NM_WHEEL_* 0x7A-0x7D (match AROS RAWKEY_NM_*) | done |
| 14 | `FreeSignal(-1)` for a port's `mp_SigBit` of -1: `DeleteMsgPort` passes the UBYTE as 255, and `FreeSignal` panics "reserved bit". AmigaOS takes the low byte, so a port whose signal was given away (`mp_SigBit = -1`, as amiga.lib's `TimeDelay` and AROS code do) can be deleted | **Landed** 9a6e75b2: FreeSignal ignores 0xFF, as Exec V37+ | done |
| 15 | `mkdisk.py`: a larger exec partition (an option such as `--exec=BLOCKS`): it is fixed at 1024 blocks (512 KB) | **Landed** 9a6e75b2: exec partition sized from contents; `--exec=BLOCKS` overrides | done |
| 16 | Faster `ObtainSemaphore`/`ReleaseSemaphore` and `Forbid`/`Permit` (below) | open | — |
| 17 | `<dos/dosextens.h>`: the NDK ST_* entry types | **Landed** cc58d673 | done |
| 18 | `mkdisk.py --mkdir=PATH` | **Landed** cc58d673 | done |
| 19 | Free the name `console.device` for the window console (below) | open | — |
| 20 | `Open()`: set `pr_ConsoleTask` from the handle's `fh_Type` after the open packet (below) | open | — |
| 21 | `NewShell WINDOW`: open the window once (below) | open | — |

### 16. Semaphore and Forbid cost

An uncontended `ObtainSemaphore` plus `ReleaseSemaphore` takes about 1.5k cycles at 6 MHz (`obtain` calls `xForbid`, `take` and `xPermit`; `xReleaseSem` calls `xForbid` and `xPermit`), and `Forbid`/`Permit` about 330 cycles a pair. graphics.library takes the layer lock and the pointer lock around every primitive, so opening a window does about 420 obtains and 900 Forbid/Permit pairs: 12% of its 5.1M cycles. Repro: wbdemo's win2-open under the profiling emulator.

Asked for: an assembler fast path for the free, own-nested and shared cases (count and owner set with interrupts held off, no calls), falling back to the queue as now, and `Forbid`/`Permit` without the C call frame.

### 19. The name console.device

Amiga programs open `console.device` with a window in `io_Data` (CON: handlers, editors), or unit `CONU_LIBRARY` (-1) for `RawKeyConvert`. wb900's AROS console.device does both, but the kernel's text console holds the name, and ramlib refuses a second device of that name, so ours is `wincon.device` for now.

Asked for: rename the kernel's device (say `kcon.device`, with its own macro instead of `CONSOLENAME`) in the kernel, `con.c`, the display modules and the tests. wb900 then renames its device to `console.device`.

### 20. pr_ConsoleTask after Open

wb900's CON: is one process per device that hands each open to a new window process; the window process sets `fh_Type` to its own port, as AmigaOS per-window handlers do. `xOpen` sets an unset `pr_ConsoleTask` to the device's port instead, so a later `Open("*")` (and `SystemTagList`'s default `SYS_Output`) reaches the device process, which has no window to give it. Repro: `Open("CON:0/0/300/100/x", MODE_OLDFILE)` from a process with no console, then `Open("*", MODE_NEWFILE)` fails.

Asked for: `me->pr_ConsoleTask = fh->fh_Type` (the port after the packet). Meanwhile wb900's Shell launcher calls `SetConsoleTask(fh_Type)` itself.

### 21. NewShell WINDOW

`newshell()` in `dos/shell.c` opens the WINDOW name twice, for input and output. With a window CON: that is two windows. AmigaOS opens it once and gets the output stream as `*` of that window.

Asked for: open the window once (`MODE_OLDFILE`), `SetConsoleTask(in's fh_Type)` around `Open("*", MODE_NEWFILE)` for the output, and give the new shell that console task. Until then `NewShell` from a Shell window opens two windows.

### 10. Keyboard raw codes

keyboard.device sends XT set-1 codes; Amiga raw codes would be (set-1 → Amiga): 01 Esc→45, 02-0B 1..0→01-0A, 0C -→0B, 0D =→0C, 0E BS→41, 0F Tab→42, 10-1B q..]→10-1B, 1C Return→44, 1D Ctrl→63, 1E-28 a..'→20-2A, 29 `→00, 2A LShift→60, 2B \→0D, 2C-35 z../→31-3A, 36 RShift→61, 37 KP*→5D, 38 Alt→64, 39 Space→40, 3A Caps→62, 3B-44 F1-F10→50-59, 47-49 KP7-9→3D-3F, 4A KP-→4A, 4B-4D KP4-6→2D-2F, 4E KP+→5E, 4F-51 KP1-3→1D-1F, 52 KP0→0F, 54 Help→5F, 55 Del→46, 56 F12→6F, 59 KP.→3C, 5A KP/→5C, 5E KPEnter→43, 5F up→4C, 60 left→4F, 61 right→4E, 62 down→4D. No Amiga key: 45 F14, 46 F15, 53 KP00, 57 F13, 58 CE, 5B-5D W1-W3 (unused codes 0E, 1C, 2B, 2C, 3B and 6B-6D are free for them). With Amiga codes, `<devices/keyboard.h>`'s `RAWKEY_*` should become the NDK/AROS `<devices/rawkeycodes.h>` values (today's `RAWKEY_LSHIFT` 0x2A clashes with their 0x60). input.device drops events with `IEQUALIFIER_REPEAT` from keyboard.device, since it times repeat itself.

Minor: `<exec/alerts.h>` lacks `AN_Unknown` and the `AG_*` general codes; input_intern.h defines the four it uses, as utility_init.c defines `AN_UtilityLib`.

## Owned by wb900, not Exec900

- input.device (handler chain, IND_ADDHANDLER, timestamps) and keymap.library: ported from AROS (APL) in this repo.
- graphics.library and layers.library: native graphics, AROS hyperlayers port.

## Points exec816 raised for us to handle

- Blitter-free bitmap ops at 6 MHz: budget the CPU.
- HR framebuffer layout vs BitMap Planes: 1bpp mono is one plane. 4bpp RGBI is chunky, so BitMap needs a chunky variant.
- Float is MWC soft-float (Z8070 optional): keep it out of hot paths.
- No MEMF_CHIP: map AROS MEMF_CHIP requests to MEMF_ANY.
- Cross-segment walking of blocks over 64 KB uses `PtrStep`.

## Not needed

`Cause`/soft interrupts, `Supervisor`, `StackSwap`, traps, cache calls, `ColdReboot`, AROS-only `NewCreateTask`/`AddResetCallback`.

## Deferred: C900 keys with no PC key on the MiSTer (2026-11-15)

The MiSTer core maps PC keys to C900 keyboard codes (rtl/host/ps2_kbd_matrix.sv). The arrows (0x5F-0x62), Del (0x55), keypad / (0x5A), keypad Enter (0x5E) and keypad . (0x59) are being mapped by c900-c0. These C900 keys stay unreachable from a PC keyboard: Help (0x54, Amiga 0x5F), F12-F15, CE, keypad 00 and W1-W3. Nothing in wb900 uses them yet. Pick PC keys for them when something needs them, and ask c900-c0 to add them to the core map. The C900 key numbers are in kernel3 os/hrtty/src/kbtab.c and Exec900 docs/platform.md#keyboard.
