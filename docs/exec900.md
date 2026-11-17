# Exec900 requests from wb900

Requests wb900 made of Exec900. Commits are in the Exec900 repository unless named otherwise.

| # | Request | Status |
|---|---|---|
| 1 | Shared HR display | landed in 6f97eb56 |
| 2a | OpenLibrary/OpenDevice fall back to LIBS:/DEVS: | landed in 4699b0b3 |
| 2b | Library code over 64 KB | landed in ab848a9d |
| 3, 7 | timer.device: TR_GETSYSTIME/SETSYSTIME, GetSysTime/AddTime/SubTime/CmpTime, UNIT_MICROHZ | landed in f750bc7b |
| 4 | Long external names | landed in toolchain b6b5468 and ab848a9d |
| 5a | Stack high-water reporting | landed |
| 5b | Bigger stack pool | landed |
| 6 | Periodic interrupt for pointer/mouse | landed |
| 8 | Gameport trigger, key-up, qualifiers | landed |
| 9 | `<devices/inputevent.h>`: `ie_dead` and the `ie_Prev*` macros | landed in 6f97eb56 |
| 10 | keyboard.device: Amiga raw key codes | landed in 6f97eb56 |
| 11 | `MEMF_SEM_PROTECTED` pools | landed in ce299d9a |
| 12 | `AllocVecPooled` / `FreeVecPooled` | landed in ce299d9a |
| 13 | `IECLASS_NEWMOUSE` and `NM_WHEEL_*` | landed in 79572c7b |
| 14 | `FreeSignal(-1)` panics, so a port with `mp_SigBit = -1` can't be deleted | landed in 79572c7b |
| 15 | `mkdisk.py`: exec partition fixed at 512 KB | landed in 79572c7b |
| 16 | Faster semaphores and `Forbid`/`Permit` | landed in a8a6e094 |
| 17 | `<dos/dosextens.h>`: the NDK ST_* entry types | landed in 79572c7b |
| 18 | `mkdisk.py --mkdir=PATH` | landed in 79572c7b |
| 19 | Free the name `console.device` for the window console | landed in 6f97eb56 |
| 20 | `Open()`: set `pr_ConsoleTask` from the handle's `fh_Type` | landed in 1fb21177 |
| 21 | `NewShell WINDOW` opens the window twice | landed in 1fb21177 |
| 22 | dos.library: `DateToStr`, `CompareDates`, `SplitName`, `<dos/datetime.h>` | landed in d5f4df45 |
| 23 | Default Shell command stack of 1600 B too small | landed in 5863676a |
| 24 | ramlib's low-memory handler walks freed list nodes | landed in 4699b0b3 |
| 25 | console.device: read the console ring | open |
| 26 | ExecBase: idle time, for a CPU load | open |
| 27 | `Close()`: clear the `pr_ConsoleTask` its `Open()` set | landed in f57f07b0 |
| 28 | `InLoadSeg`: read RELOC32 offsets in blocks | landed in 134ad585 |
| 29 | Smaller root (boot) stack | landed in 79572c7b |
| 30 | DH1:'s handler shares DH0:'s code | landed in 98f9e4a3 |
| 31 | 63 KB stack segment on 1 MB leaves no whole heap pages | landed in a8a6e094 |
| 32 | Tell framebuf.resource the HR jumper at boot | landed in 4c8e77a7 |
| 33 | Recoverable `Alert()` on screen through intuition | landed in 7bec5b16 |
| 35 | Boot assign `FONTS:` → `SYS:Fonts` | landed in 134ad585 |
| 36 | Cheaper kernel console output | landed |
| 37 | gameport.device on the C emulator | fixed in the emulator, 3c6656d |
| 38 | runtest.sh: `MOUSE:` on the emulator | landed 79572c7b |
| 39 | lc.display: positive syncs in R37 | landed in f57f07b0 |

## Notes

**1.** framebuf.resource: GetFBInfo −6, AllocFB −12, FreeFB −18; current mode only, 1024x1024 reported as 1024x800. gameport.dev is its own module.

**2a.** ramlib-style loading; a Task loads through a ramlib process; the low-memory handler expunges. Recipe: Exec900 docs/platform.md, "Building a disk library". Rebuild against the new execbase.h. Everything genlvo.py generates is MIT; genlvo.py itself stays GPL.

**2b.** `mkhunk.py --split --ld=$LD [--entry=start] OUT objs... [libs.a] progabs.o` packs objects into code hunks of 64 KB or less, with RELOC32 between them. Each function and object must fit 64 KB, no hand-written CALR/JR may cross groups, and symcheck.py must run over all sources.

**3, 7.** AmigaOS 3.x timer.device, all units, plus ReadEClock. Requests complete at the 100 Hz tick.

**4.** `-VLNAME` (cc2, as/ld `-N`, up to 127 characters); mkhunk/symcheck `--lname`.

**5a.** `C:StkUse`; guard word at `tc_SPLower` (AN_StackProbe).

**5b.** Sized at boot: 63 KB on every machine, its spare RAM serving ordinary allocations last. `mkdisk.py --stack=KB` overrides. `C:StackUse` lists per-Task use and pool free. ExecBase grew (TaskReady/TaskWait): rebuild.

**6.** `AddIntServer(INTV_TICK, is)` joins the 100 Hz CIO tick chain; `INTB_VERTB` alias.

**8.** GPD_SET/ASKTRIGGER with Amiga semantics; `IECODE_UP_PREFIX` and `IEQUALIFIER_*`. Keyboard not yet board-tested.

**9.** Also the full NDK set; `<exec/alerts.h>` gets AN_*/AG_*/AO_*.

**10.** Key-up is 0x80; keypad keys carry NUMERICPAD. `<devices/rawkeycodes.h>` matches AROS. C900-only keys: KP 00 0x0E, CE 0x1C, W1 0x2B, W2 0x2C, W3 0x3B, F13-F15 0x6B-0x6D. input.device drops keyboard.device events with `IEQUALIFIER_REPEAT`, since it times repeat itself. Not yet tried on a real keyboard.

**11.** MEMF_SEM_PROTECTED is 1L<<22 (AROS's bit 20 is MEMF_LINEAR here).

**12.** AllocVecPooled −894, FreeVecPooled −900; an 8-byte header keeps 8-byte alignment. FreeVecPooled needs the owning pool.

**13.** IECLASS_NEWMOUSE 0x16; `<devices/newmouse.h>` NM_WHEEL_* 0x7A-0x7D, matching AROS RAWKEY_NM_*.

**14.** FreeSignal ignores 0xFF, as Exec V37+.

**15.** The exec partition is sized from its contents; `--exec=BLOCKS` overrides.

**16.** Uncontended Obtain/Release about 2,200 to 650-770 clocks; Forbid/Permit 573 to 429.

**19.** The kernel console is `textcon.device` (`TEXTCONNAME`, `<devices/textcon.h>`); units and flags move to `<devices/conunit.h>`.

**20.** Set from the first interactive open. CreateNewProc takes NP_ConsoleTask.

**22.** Also StrToDate (−750) and the other NDK dos.h constants.

**23.** 4,000 B.

**24.** The walk restarts after an unload; devices expunge through RemDevice.

**25.** LogView would show the boot log. `devices/console.c` keeps the last 1 KB in `bootlog`, but `CMD_READ` returns typed characters. Asked for: a command (say `CD_READLOG`, `CMD_NONSTD + 18`) copying the ring, oldest byte first, into `io_Data` up to `io_Length`, setting `io_Actual`. Meanwhile LogView shows files.

**26.** SysMon would show CPU load. Exec900 has no `IdleCount` or `DispCount`, and the idle Task is on neither `TaskReady` nor `TaskWait`. Asked for: both counts at the end of ExecBase, `IdleCount` incremented by the idle loop or by the tick when it interrupts the idle Task. Meanwhile SysMon shows no load.

**27.** Close clears the console only if its handle set it; `Open("*")` afterwards fails with ERROR_DEVICE_NOT_MOUNTED.

**28.** A 128-entry reloc buffer: a 2,219-reloc file loads in 0.32 s instead of 1.50 s at 6 MHz.

**29.** `mkdisk --rootstack=KB`, 1-16, default 4. Boot uses about 500 B, so 2 KB is safe for wb900's root program.

**30.** First use of DH1: costs 11,240 B (7,096 with `--sysbuffers=8`) instead of 37,760.

**31.** Blocks of 128 B or less take the stack region's top first, so whole heap pages stay free.

**32.** `mkdisk --hrmode=auto|800|1024|rgbi`; framebuf and hr.display follow it. hr.gfxdriver lists all three jumper modes, the reported one first and the others `DI_AVAIL_NOMONITOR`; opening another (a saved ScreenMode) takes the jumper to be that one, and the console returns to the boot mode at FreeFB.

**33.** dos.library patches Alert: a recoverable Task alert outside Forbid/Disable calls `DisplayAlert(0, text, 40)`, as exec 1.2 does. Kernel and dead-end alerts stay on the console.

**35.** On `--lnames` disks FONTS: is on DH1:.

**36.** Line-at-a-time console output and a 256-byte TX queue on the SCC interrupt. A KPrintf line: HR mono 494K to 127K clocks, RGBI 1,963K to 206K, LC 561K to 200K.

**37.** Ports 0x0400-0x0407 decode like the core under plain `--hr`.

**38.** runtest.sh turns `MOUSE: TICK:DX:DY[:LRM],...` into `--hr` and one `--mouse` per event, as `EMUKEYS` does for `--key`.

**39.** lc.display writes R37 = 0xC0 (positive syncs, the reset value) and restores it on hand-back.

## Owned by wb900

- input.device (handler chain, IND_ADDHANDLER, timestamps) and keymap.library: AROS ports (APL).
- graphics.library (AROS over a driver table) and layers.library (AROS hyperlayers).

## Points from Exec900 for wb900

- Blitter-free bitmap ops at 6 MHz: budget the CPU.
- HR framebuffer vs BitMap Planes: 1bpp mono is one plane; 4bpp RGBI is chunky, so BitMap needs a chunky variant.
- Float is MWC soft-float (Z8070 optional): keep it out of hot paths.
- No MEMF_CHIP: map AROS MEMF_CHIP requests to MEMF_ANY.
- Walking a block over 64 KB across segments uses `PtrStep`.

## Not needed

`Cause`/soft interrupts, `Supervisor`, `StackSwap`, traps, cache calls, `ColdReboot`, AROS-only `NewCreateTask`/`AddResetCallback`.

## Deferred: C900 keys with no PC key

The C900_MiSTer core maps PC keys to C900 codes (rtl/host/ps2_kbd_matrix.sv). The arrows (0x5F-0x62), Del (0x55), keypad / (0x5A), Enter (0x5E) and . (0x59) are being mapped. Unreachable from a PC keyboard: Help (0x54, Amiga 0x5F), F12-F15, CE, keypad 00, W1-W3. wb900 uses none yet; when something does, pick PC keys and ask for them in C900_MiSTer. C900 key numbers: Exec900 docs/platform.md#keyboard.
