# Exec900 requests from wb900

Opened 2026-11-14; status from exec816-0f and the c900/exec900 log. The user approves each piece of Exec work.

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
| 16 | Faster `ObtainSemaphore`/`ReleaseSemaphore` and `Forbid`/`Permit` (below) | **Landed** 56d2a734: uncontended Obtain/Release about 2,200 to 650-770 clocks; Forbid/Permit 573 to 429 | done |
| 17 | `<dos/dosextens.h>`: the NDK ST_* entry types | **Landed** cc58d673 | done |
| 18 | `mkdisk.py --mkdir=PATH` | **Landed** cc58d673 | done |
| 19 | Free the name `console.device` for the window console (below) | **Landed** 19839cad: the kernel console is `textcon.device` (`TEXTCONNAME`, `<devices/textcon.h>`); units and flags move to `<devices/conunit.h>` | done |
| 20 | `Open()`: set `pr_ConsoleTask` from the handle's `fh_Type` after the open packet (below) | **Landed** 1e6d4cf7: from the first interactive open; CreateNewProc takes NP_ConsoleTask | done |
| 21 | `NewShell WINDOW`: open the window once (below) | **Landed** 1e6d4cf7 | done |
| 22 | dos.library: `DateToStr`, `CompareDates` and `SplitName`, with `<dos/datetime.h>` and `MAXFILENAMELENGTH` (below) | **Landed** 0845a36e, with StrToDate (−750) and the other NDK dos.h constants | done |
| 23 | A Shell command's default stack: 1600 B is too small for intuition programs (below) | **Landed** 58a2e88c: 4,000 B | done |
| 24 | ramlib's low-memory handler walks freed list nodes (below) | **Landed** 0d59aadf: the walk restarts after an unload; devices expunge through RemDevice | done |
| 25 | console.device: read the console ring (below) | open | — |
| 26 | ExecBase: idle time, for a CPU load (below) | open | — |
| 27 | `Close()`: clear the `pr_ConsoleTask` that the handle's `Open()` set (below) | **Landed** d2a5278b: Close clears the console its handle set; Open("*") afterwards fails with ERROR_DEVICE_NOT_MOUNTED | done |
| 28 | `InLoadSeg`: read a hunk's RELOC32 offsets in blocks (below) | **Landed** d547e925: 128-entry reloc buffer; a 2,219-reloc file loads in 0.32 s instead of 1.50 s at 6 MHz | done |
| 29 | A smaller root (boot) stack, so a Workbench disk gives the rest to the pool (below) | **Landed** 9159ccc8: `--rootstack=KB`, 1-16, default 4; boot uses about 500 B, so 2 KB is the safe choice for our root program | done |
| 30 | DH1:'s handler: share DH0:'s code instead of a second copy (below) | **Landed** 49f8df91: one shared v7fs-handler seglist; first use of DH1: costs 11,240 B (7,096 with `--sysbuffers=8`) instead of 37,760 | done |
| 31 | A 63 KB stack segment on 1 MB leaves no two free heap pages for a 640x400x4 bitmap (below) | **Landed** 8c2497f3: blocks of 128 B or less take the stack region's top first, so whole heap pages stay free; asl passes on 1 MB again | done |
| 32 | Tell framebuf.resource the HR jumper at boot (below) | **Landed** 8a6c0d5d: `mkdisk --hrmode=auto|800|1024|rgbi`; framebuf and hr.display follow it. The ScreenMode prefs may still drive a corrected mode while Workbench owns the display | done |
| 33 | A recoverable `Alert()` on the screen through intuition while it lends the display (below) | **Landed** 6ff12776: dos.library patches Alert; a recoverable Task alert outside Forbid/Disable calls DisplayAlert(0, text, 40) as exec 1.2 does; kernel and dead-end alerts stay on the console | done |
| 36 | Cheaper kernel console output (below) | **Landed**: line-at-a-time console output and a 256-byte TX queue on the SCC interrupt; a KPrintf line on HR mono 494K to 127K clocks, RGBI 1,963K to 206K, LC 561K to 200K | done |
| 35 | The boot assign `FONTS:` → `SYS:Fonts` (below) | **Landed** 9f4f7561: dos.library assigns FONTS: to SYS:Fonts at boot (DH1: on --lnames disks) | done |
| 37 | gameport.device on the C emulator, so `GetGPButtons` can be tested (below) | **Fixed in the emulator** (dcfe4c0): ports 0x0400-0x0407 decode like the core under plain `--hr` | done |
| 38 | runtest.sh: `MOUSE:` on the emulator (below) | open | — |
| 39 | lc.display: positive syncs in R37 (below) | **Landed** 620f69eb: lc.display writes R37 = 0xC0 and restores it on hand-back | done |

### 39. R37 in lc.display

R37 bits 7 and 6 set the HSYNC and VSYNC polarity, 1 positive, and read FF after reset (C900_MiSTer D-VID-10). lc.display's `lcinit()` writes every register from `lcregs[]`, R37 included, as 0x00, so the text console runs with negative syncs, though its comment says R37 keeps its reset value. lc.gfxdriver writes 0xC0, the reset value and the C128's default, so the polarity flips at each hand-over. The core accepts either; a real monitor may not.

Asked for: R37 = 0xC0 in `lcregs[]`.

### 38. MOUSE: on the emulator

The emulator now models the HR mouse under `--hr`, but runtest.sh ignores `MOUSE:` there, so a test spells each event as its own `--mouse=T:DX:DY:B` word in `EMUFLAGS` (B: 4 left, 2 middle, 1 right), and `--hr` too (wb900 `tests/gpalert.c`).

Asked for: runtest.sh to turn `MOUSE: TICK:DX:DY[:LRM],...` into `--hr` and one `--mouse` per event, as `EMUKEYS` does for `--key`.

### 37. gameport.device on the C emulator

intuition's `DisplayAlert` on input.device's Task polls `KBD_READMATRIX` and `GetGPButtons`. `make test` drives the keyboard with `--key`, but the buttons can't be: the emulator's `--mouse` answers at 0x0400-0x0403, yet gameport.dev loads only with an HR console, which the emulator never gives, even with `--hr`. Repro: in wb900 `tests/intuition.c`, print `FindName(&SysBase->DeviceList, "gameport.device")` and run with `--hr --mouse=1000:0:0:4`: it is 0.

Asked for: a way to load gameport.dev's HR backend on the emulator (for example when the 0x0400 ports answer, or a mkdisk option), so a test can press a button with `--mouse`.

### 35. FONTS: at boot

AmigaOS's dos.library makes `FONTS:` → `SYS:Fonts` with the other boot assigns; Exec900 makes `SYS:`, `C:`, `S:`, `L:`, `LIBS:` and `DEVS:` only. diskfont.library finds fonts through `GetDeviceProc("FONTS:")`, so without it no disk font opens (repro: wb900 `tests/diskfont.c` without its `AssignLate("FONTS", "SYS:Fonts")`: AvailFonts(AFF_DISK) lists nothing). Meanwhile Workbench and the test make the assign.

Asked for: `FONTS:` → `SYS:Fonts` as a late assign at boot, like `LIBS:`.

### 36. Kernel console output cost

While the boot console owns the HR display, each line `kputc` prints is drawn there: on the profiling emulator at 6 MHz, the 52-byte line intuition printed before `OpenScreen` made opening the Workbench 70 ms slower in mono and 315 ms in RGBI than printing it after, when a byte costs about 1,700 clocks. wb900 now prints it after (repro: wb900 9876d4b `tests/wbdemo.c`, phase screen-open, `C900_HRCONF=1`). Also, `con_putc` busy-waits for the SCC, so on the board each byte should cost a byte time, about 1 ms at 9600 baud; the emulator doesn't pace it.

Asked for: a cheaper display console line (its scroll), and `kputc` output queued for the transmit interrupt when interrupts are on, waiting only when the queue is full or interrupts are off.

### 33. Recoverable alerts on the screen

While framebuf.resource lends the display, a recoverable `Alert()` is serial only, and hr-display-sharing.md plans a hook so that `Alert()` calls intuition's `DisplayAlert`. intuition.library now has `DisplayAlert` (LVO -90) and `TimedDisplayAlert` (-822): the alert covers the top of the frontmost screen until a mouse button, Return or Escape, then the screen is restored; it also prints the number and text on the console. It waits on input.device, so it can't be called from the input.device Task, in `Forbid`/`Disable` or from an interrupt.

Asked for: in `Alert()` without `AT_DeadEnd`, after the console line and `LastAlert`, when intuition.library is in `SysBase->LibList` and the caller is a Task outside `Forbid` and `Disable`, open it and call `TimedDisplayAlert(num, text, 40, 0)`, with text as AmigaOS's (`"Recoverable Alert #NNNNNNNN.TTTTTTTT"` and `"Press left mouse button to continue."`, each a 3-byte position, the string, a continuation byte). Dead-end alerts keep the reclaim path.

### 32. The HR jumper at boot

The strap reads the same for 1024x800 and 1024x1024, and nothing sets `BF_HR1024`, so a 1024x1024 card is drawn as 1024x800. The mono/colour read is inferred from undumped PALs and could disagree with the jumper. Either way HR can boot into a mode the display isn't set for.

Asked for: a boot setting that tells framebuf.resource the jumper (say `mkdisk --hrmode=800|1024|rgbi` into the kboot flags, or a config line read before the console starts), reported by framebuf and used by the console, with the strap read as the default. wb900's ScreenMode preferences then confirm it, and a boot key forces the reported mode.

hr.gfxdriver lists all three jumper modes, the reported one first and the others `DI_AVAIL_NOMONITOR`; opening another (a saved ScreenMode) takes the jumper to be that one. This stays after the boot setting: the ScreenMode tool fixes a wrong jumper without rebuilding the disk, and the console returns to the boot mode at FreeFB.

### 31. Whole heap pages on 1 MB

Since 1e499a37 a 1 MB machine boots with a 63 KB stack segment: `14 heap pages and 4096 bytes` where 32 KB gave 35,840 more bytes. The segment's spare RAM serves small allocations, but not a block of whole pages: tests/asl's `AllocBitMap(640, 400, 4, ...)` chunky bitmap (128,000 B, rows split at 64 KB) now fails, and passes with `mkdisk --stack=32`. Repro: `make test T=asl` with `--stack=32` removed from `asl_PUT`. tests/asl boots with 32 KB meanwhile.

Asked for: the stack segment's unused pages back in the page heap, or a 1 MB default that leaves the page.

### 30. A second v7fs-handler on 1 MB

On a `--lnames` disk, DH1:'s handler is a second loaded copy (about 17 KB) plus its 16 buffers (8.3 KB): 25,400 B less heap at boot. On 1 MB that is the difference for asl.library (hunks of 52,964, 60,662 and 13,174 B): tests/edit opens it for Save As, and with `--lnames` it no longer loads. Before the open: 172,328 B free, largest 131,072 with one handler; 146,928 B, largest 65,536 with two, and no hole left for the 13 KB hunk. tests/edit runs on 1,088 KB meanwhile. Repro: `make test T=edit` with the test's EMUFLAGS line removed.

Asked for: one code copy for both volumes (per-process data, say from the startup message), or fewer DH1: buffers (a mkdisk or kboot.cfg setting).

### 29. Root stack size

On 1 MB, Workbench, a Shell running a command and three tools need about 38 KB of task stacks (SIZING.md §5a), so the Workbench disks boot with `mkdisk --stack=48`, which takes 16 KB of heap; that machine then has about 48 KB of memory free. The root stack (0xF000-0xFFFF) stays 4,096 B, but a Workbench disk's root program only starts Workbench and waits: it used 762 B on the board.

Asked for: the root stack's size as a boot setting beside `sc_kb` (say `mkdisk --rootstack=KB`, 1 to 4), with the freed bytes in the `MEMF_STACK` pool. A 1 KB root stack would let these disks boot with 45 KB instead of 48, 3 KB more heap.

### 16. Semaphore and Forbid cost

An uncontended `ObtainSemaphore` + `ReleaseSemaphore` took ~1.5k cycles at 6 MHz (`obtain` calls `xForbid`, `take`, `xPermit`; `xReleaseSem` calls `xForbid`, `xPermit`), a `Forbid`/`Permit` pair ~330. graphics.library takes the layer and pointer locks around every primitive, so a window open did ~420 obtains and 900 Forbid/Permit pairs: 12% of its 5.1M cycles. Repro: wbdemo's win2-open under the profiling emulator.

Asked for: an assembler fast path for the free, own-nested and shared cases (count and owner set with interrupts off, no calls), else the queue as now; `Forbid`/`Permit` without the C frame.

### 28. LoadSeg's relocation reads

`xInLoadSeg` reads each RELOC32 offset with `lsget`: a `read` call through the funcarray for 4 bytes, which dos's `dlsread` serves from its buffer with a `CopyMem`. That is about 5,000 clocks a relocation: loading hr.gfxdriver (2,172 bytes, 97 relocations) took 0.65M of the 0.9M clocks of the first `OpenScreen()`, and intuition.library has 2,091 relocations, about 1.7 s at 6 MHz. Repro: wbdemo's screen-open under the profiling emulator; the calls come from `xInLoadSeg` +0x8a and +0xbc.

Asked for: read the offsets in blocks (`lsread` of up to, say, 64 longs into a local buffer) or inline the buffered get.

### 19. The name console.device

Amiga programs open `console.device` with a window in `io_Data` (CON: handlers, editors), or unit `CONU_LIBRARY` (-1) for `RawKeyConvert`. wb900's AROS console.device does both, but the kernel's text console held the name and ramlib refuses a second, so ours is `wincon.device` for now.

Asked for: rename the kernel's device (say `kcon.device`, own macro instead of `CONSOLENAME`) in the kernel, `con.c`, the display modules and the tests. wb900 then renames its device `console.device`.

### 20. pr_ConsoleTask after Open

wb900's CON: device process hands each open to a new window process, which sets `fh_Type` to its own port, as AmigaOS per-window handlers do. `xOpen` set an unset `pr_ConsoleTask` to the device's port instead, so a later `Open("*")` (and `SystemTagList`'s default `SYS_Output`) reached the device process, which has no window. Repro: `Open("CON:0/0/300/100/x", MODE_OLDFILE)` from a process with no console, then `Open("*", MODE_NEWFILE)` fails.

Asked for: `me->pr_ConsoleTask = fh->fh_Type` (the port after the packet). Meanwhile wb900's Shell launcher calls `SetConsoleTask(fh_Type)`.

### 21. NewShell WINDOW

`newshell()` in `dos/shell.c` opened the WINDOW name twice, for input and output: two windows with a window CON:. AmigaOS opens it once and gets output as `*` of that window.

Asked for: open it once (`MODE_OLDFILE`), `SetConsoleTask(in's fh_Type)` around `Open("*", MODE_NEWFILE)` for output, and give the new shell that console task.

### 22. DateToStr, CompareDates, SplitName

asl's file requester shows dates with `DateToStr` (`FORMAT_DOS`), sorts with `CompareDates` and splits volume names with `SplitName`. Exec900's dos.library lacked all three (`fd/dos_lib.fd` named them without types, so `<lvo/dos_lib.h>` had no macros), as well as `<dos/datetime.h>` (`struct DateTime`, `FORMAT_*`, `LEN_DATSTRING`) and `MAXFILENAMELENGTH`.

Asked for: the three at AmigaDOS's LVOs (−744 DateToStr, −738 CompareDates, −414 SplitName) and the header definitions. Meanwhile asl leaves date and time empty, compares DateStamps and cuts at the colon itself, and uses 108 for `MAXFILENAMELENGTH`.

### 23. Default command stack

`CLISTACK` gave each Shell command 1600 bytes. More and Edit use 2.6-3.0 KB, like most intuition and gadtools programs, so `SYS:Utilities/More file` from a Shell overflowed: AN_StackProbe, then a trap that stops the machine. `Stack 4096` first works. Repro: tests/more.c without its `Stack 4096`.

Asked for: a 4096-byte default (AmigaOS uses 4000), or a stack size taken from the program file.

### 24. ramlib's low-memory handler and nested expunges

`rlmemh` keeps `n = r->r_Next`, then calls `r`'s Expunge. An Expunge that closes another library (asl closes gadtools, graphics its driver) runs the patched `CloseLibrary`; if that library has `LIBF_DELEXP` and goes, `rlgone` frees its `rlseg`. If that was `n`, the walk continues through freed memory: `Deallocate: block is free` or a segment trap. This was the trap in `OpenLibrary("asl.library")` from a Shell: a 62 KB hunk load ran the handler.

Repro (wb900 tests/expunge.c, after the loop; `lowmem()` allocates `AvailMem + 128 KB`):

```
lowmem();                          /* asl and gadtools gone */
gt = OpenLibrary("gadtools.library", 0L);
asl = OpenLibrary("asl.library", 0L);   /* rlsegs: asl, gadtools */
lowmem();                          /* both LIBF_DELEXP */
OpenLibrary("asl.library", 0L);    /* clears asl's flag */
CloseLibrary(asl); CloseLibrary(asl); CloseLibrary(gt);
lowmem();                          /* asl's Expunge frees gadtools' rlseg */
```

Asked for: restart the walk at `rlsegs` after each unload. Tried in a copy of dos/ramlib.c; the repro then passes:

```
for (r = rlsegs; r != NULL; ) {
	seg = (BPTR)(*rloldrem)(r->r_Lib);
	if (seg != NULL) {
		rlgone(seg);
		done = MEM_ALL_DONE;
		r = rlsegs;
	} else
		r = r->r_Next;
}
```

### 25. Reading the console ring

LogView would show the boot log and console output since graphics took the display. `devices/console.c` keeps the last 1 KB in `bootlog`, but nothing reads it: `CMD_READ` returns typed characters.

Asked for: a console.device command (say `CD_READLOG`, `CMD_NONSTD + 18`) that copies the ring, oldest byte first, into `io_Data` up to `io_Length` and sets `io_Actual`; without a wrap, from the first byte. Meanwhile LogView shows files.

### 26. Idle time

SysMon would show CPU load. Amiga Exec counts `ExecBase->IdleCount` and `DispCount`; Exec900 has neither, and the idle Task is on neither `TaskReady` nor `TaskWait`, so a program cannot measure it.

Asked for: `IdleCount`, incremented by the idle Task's loop or by the tick when it interrupts the idle Task, and `DispCount`, at the end of ExecBase. Meanwhile SysMon shows no load.

### 27. pr_ConsoleTask after Close

Since 20, a process's first interactive `Open()` makes the window its console, but nothing clears it when that handle is closed. wb900's window process ends at `ACTION_END` and its port goes, so `pr_ConsoleTask` dangles. Workbench's Execute opens `CON:` for a command's output only; after the command ends, a later child inherits the stale port, and its `Open("*")` sends a packet to freed memory. Repro, from a process with no console:

```
fh = Open("CON:0/0/300/100/x", MODE_NEWFILE);
Close(fh);                         /* the window process ends */
Open("*", MODE_NEWFILE);           /* packet to the freed port */
```

Asked for: `Close()` clears `pr_ConsoleTask` when this handle set it (a flag in the DOS handle). Clearing whenever it equals `fh_Type` would also clear a Shell's console when a command closes its `*`.

### 10. Keyboard raw codes

keyboard.device sends XT set-1 codes; Amiga raw codes would be (set-1 → Amiga): 01 Esc→45, 02-0B 1..0→01-0A, 0C -→0B, 0D =→0C, 0E BS→41, 0F Tab→42, 10-1B q..]→10-1B, 1C Return→44, 1D Ctrl→63, 1E-28 a..'→20-2A, 29 `→00, 2A LShift→60, 2B \→0D, 2C-35 z../→31-3A, 36 RShift→61, 37 KP*→5D, 38 Alt→64, 39 Space→40, 3A Caps→62, 3B-44 F1-F10→50-59, 47-49 KP7-9→3D-3F, 4A KP-→4A, 4B-4D KP4-6→2D-2F, 4E KP+→5E, 4F-51 KP1-3→1D-1F, 52 KP0→0F, 54 Help→5F, 55 Del→46, 56 F12→6F, 59 KP.→3C, 5A KP/→5C, 5E KPEnter→43, 5F up→4C, 60 left→4F, 61 right→4E, 62 down→4D. No Amiga key: 45 F14, 46 F15, 53 KP00, 57 F13, 58 CE, 5B-5D W1-W3 (unused codes 0E, 1C, 2B, 2C, 3B and 6B-6D are free for them). With Amiga codes, `<devices/keyboard.h>`'s `RAWKEY_*` should become the NDK/AROS `<devices/rawkeycodes.h>` values (today's `RAWKEY_LSHIFT` 0x2A clashes with their 0x60). input.device drops events with `IEQUALIFIER_REPEAT` from keyboard.device, since it times repeat itself.

Minor (fixed by 9's alerts.h): `<exec/alerts.h>` lacked `AN_Unknown` and the `AG_*` codes; input_intern.h defines the four it uses, utility_init.c `AN_UtilityLib`.

## Owned by wb900, not Exec900

- input.device (handler chain, IND_ADDHANDLER, timestamps) and keymap.library: AROS ports (APL).
- graphics.library (AROS over a driver table) and layers.library (AROS hyperlayers).

## Points exec816 raised for us to handle

- Blitter-free bitmap ops at 6 MHz: budget the CPU.
- HR framebuffer vs BitMap Planes: 1bpp mono is one plane; 4bpp RGBI is chunky, so BitMap needs a chunky variant.
- Float is MWC soft-float (Z8070 optional): keep it out of hot paths.
- No MEMF_CHIP: map AROS MEMF_CHIP requests to MEMF_ANY.
- Cross-segment walking of blocks over 64 KB uses `PtrStep`.

## Not needed

`Cause`/soft interrupts, `Supervisor`, `StackSwap`, traps, cache calls, `ColdReboot`, AROS-only `NewCreateTask`/`AddResetCallback`.

## Deferred: C900 keys with no PC key on the MiSTer (2026-11-15)

The MiSTer core maps PC keys to C900 codes (rtl/host/ps2_kbd_matrix.sv). c900-c0 is mapping the arrows (0x5F-0x62), Del (0x55), keypad / (0x5A), Enter (0x5E) and . (0x59). Unreachable from a PC keyboard: Help (0x54, Amiga 0x5F), F12-F15, CE, keypad 00, W1-W3. wb900 uses none yet; when something does, pick PC keys and ask c900-c0 to map them. C900 key numbers: kernel3 os/hrtty/src/kbtab.c, Exec900 docs/platform.md#keyboard.

Exec900 1e499a37 (2026-11-15): the stack segment is 63 KB on every machine and its spare RAM serves ordinary allocations last, so 1 MB needs no `--stack` tuning.
