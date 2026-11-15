# LC card Workbench: plan

Date: 2026-11-15. Status: phases 1-3 built and passing on the port model; phase 4 images ready for the board. See [Built](#built-2026-11-15).

## Decisions

1. **Mode:** 640x200 mono bitmap, attributes off. Interlace later, if the core supports it. No attribute colour.
2. **Drawing:** option C. A RAM shadow bitmap is the truth, drawn by the existing planar engine. Changed bytes are pushed through R31, and fills and aligned copies run as VDC blocks.
3. **Driver:** `LIBS:lc.gfxdriver` (MIT), shaped like `hr.gfxdriver`. Graphics gains one table entry, `gd_Flush`.
4. **Pointer:** the existing soft sprite. Save-under and reads come from the shadow, so the VDC is never read.
5. **Mouse:** gameport.device unit 0 with HR's interface, backed by the U74 A serial mouse. input.device stays unchanged.

## Layers

```
 Workbench, apps
        |
 intuition ---- input.device <-- gameport.device u0 <-- Mouse Systems on U74 A
        |          (MoveSprite)          [Exec900, exec816]
 layers
        |
 graphics.library: clipping, soft sprite, gfx_Planar row engine
        |   GfxDriver table (+ gd_Flush)
 lc.gfxdriver (new)
   wrappers: draw into shadow -> mark dirty bytes | fast path
   gd_Flush: dirty runs -> CPU push      fast path -> VDC fill/copy
        |                                    |
 shadow, 16000 B RAM               port layer (asm): 0x0181 / 0x0183
                                             |
                          8568, 64 KB VRAM: 0x0000 bitmap, 0x4000+ scratch
 framebuf.resource AllocFB/FreeFB <-> lc.display (text console) [Exec900]
```

## 1. Modes and VRAM

| Mode | VRAM | Verdict |
|---|---|---|
| 640x200 mono | bitmap 16000 B at 0x0000 | **Default** |
| 640x200, 8x8 attributes | +2000 B | No: pen clash, 25% less CPU bandwidth |
| 640x200, 8x2 attributes | +8000 B | No: the attribute fetch uses almost all of the CPU slots on display lines |
| 640x400 interlaced mono | 32000 B | Phase 5, if the core and scaler handle R8=3 |
| 640x480 interlaced mono | 38400 B | Same as 640x400 |

- The timing registers are lc.display's 80x25 set (8-line rows, 25 rows, 264 lines). Only R25 changes (bitmap on, attributes off), and R26 holds the colours. R12/R13 = 0, R27 = 0, and R28 selects 64 KB.
- **Free VRAM:** 0x4000-0xFFFF is scratch for overlapping copies.
- **ModeID:** a new monitor, `LC_MONITOR_ID 0x00110000`, with `LCMONO_200_ID`. It goes in `hrmodeid.h`, renamed to the C900's mode IDs.
- **640x200 is Amiga hires non-interlaced (WB 1.x):**
  - Topaz 8 gives 80x25 text and a 10-11 pixel title bar.
  - Pixels are about 1:2.4 (tall) on a 4:3 screen.
  - `CalcScreenResolution` gives `dri_Resolution` 22x56; Amiga hires is 22x44. Both are over 2:1, so gadtools and sysiclass pick hires-aspect imagery. No change needed.
  - AROS's 1024x800 nominal falls back to 640x200 already (`openworkbench.c`).

## 2. Drawing strategy

### Bus and VDC cost model (reasoned from the core RTL at 6 MHz; measure on the board)

| Item | Value |
|---|---|
| VDC RAM slot | one per character column, 0.5 µs (16 MHz dots, 8 dots) |
| Free slots, display line | columns 86-127, about 21 µs of every 64 µs (bitmap fetch 0-79, refresh 80-85) |
| Free slots, 64 blank lines | all but refresh |
| Slots per CPU byte write | 2 (write, then read-ahead) |
| Slots per fill / copy byte | 1 / 2 |
| Port access | 10 cycles + about 1 /WAIT state (about 150 ns handshake) |
| Poll (INB, BITB, JR) | about 21 cycles, 3.5 µs |
| Push loop (poll + OUTIB + JR) | about 49 cycles, **8.2 µs/byte** when ready |
| Push average over a frame | 3 bytes per display line, 8 per blank line: **about 15 µs/byte (65 KB/s)** |
| Row setup (R18, R19, select R31) | about 17 µs + slot waits: **about 25 µs** |
| Block fill, 255-byte piece | about 0.27 ms; **about 1 µs/byte** over a frame |
| Block copy | **about 2 µs/byte** |

Each R31 access, R30 write and R18/R19 write waits for Update Ready. A write while busy replaces the pending action and loses data. The push loop must be assembly, because MWC calls to `outb()` would be several times slower.

### Options

| Operation (6 MHz) | A: shadow + push | B: direct through VDC | **C: shadow + push + VDC blocks** |
|---|---|---|---|
| Full-screen clear | 0.25 s | 17 ms | **17 ms** |
| Full-screen push | 0.25 s | n/a | 0.25 s (rare) |
| Text line, 80 chars JAM2 | 10 ms | 40 ms (read-modify-write about 60 µs/byte) | **10 ms** |
| Scroll, full width, 190 rows | 0.25 s | 32 ms | **32 ms** |
| Scroll, 40-byte window, 180 rows | 0.12 s | 30 ms | **about 30 ms** (edges pushed) |
| Window open, 320x120 | 0.1 s | about 80 ms + read-modify-write | **about 50 ms** |
| Window move, unaligned, 300x100 | 60 ms | slow (read-modify-write) | 60 ms; **8 ms** if dx%8 = 0 |
| Pointer move (section 4) | 1 ms | VRAM reads for save-under | **about 1 ms** |
| RAM | 16 KB | 0 | 18 KB |

Window open in C: background fill 10 ms, borders 10 ms, title 5 ms, gadgets 5 ms, about 2 ms per icon.

**C wins:** shadow reads are free (ReadPixel, BltBitMap from the screen, sprite save-under), drawing reuses gfx_Planar, and fills and scrolls run at VDC block speed. B loses on read-modify-write, A on clears and scrolls.

### How C works

- **Dirty tracking:** 1 bit per VRAM byte (2000 B) plus a flag per row. Flush walks the runs and bridges gaps of 2 bytes or less (a row setup costs about 2 bytes of push). Rows that run the full width are contiguous, so the flush does no reseek.
- **Lines:** mark per-row x spans, not the bounding box.
- **Fill fast path** (mono, mask bit 0, JAM1/JAM2, solid or a pattern row whose two bytes are equal, interior of 4 bytes or more): flush, fill the shadow, then VDC-fill each row's interior. A full-width rectangle is one run. Edge bytes are marked dirty. COMPLEMENT always pushes.
- **Copy fast path** (display to display, minterm 0xC0, `sx%8 == dx%8`, interior of 4 bytes or more):
  - Flush first, so VRAM matches the shadow, then copy the shadow and VDC-copy each row's interior.
  - When dy > 0, rows go bottom-up. A rightward overlap within one row goes through scratch VRAM (two copies).
  - Edges are marked dirty.
  - Vertical scrolls always qualify.
- **Interrupts:** the 8568 raises INTR on every ready edge, and NVI is on in Task FCWs. Each piece (64 pushed bytes or one 255-byte block, at most 1 ms) runs under `Disable()` and ends with a status read that finds the chip ready, as lc.display does.

## 3. GfxDriver fit

`lc.gfxdriver`:

- **`GetGfxDriver`:** `fbi_Card == FBC_LC`. One mode at first, with the mode list owned by the driver (the VDC is programmable).
- **Wrapped:** `gd_Fill`, `gd_Template`, `gd_Copy`, `gd_WritePixel`, `gd_Line` and `gd_WriteRow` call gfx_Planar on the shadow, then mark dirty bytes or take a fast path. They forward unchanged when the destination is not the display (sprite save, friend bitmaps).
- **Unwrapped:** `gd_ReadPixel` and `gd_ReadRow` are gfx_Planar's.
- **`gd_AllocDisplay`:**
  - `AllocFB("graphics.library")`.
  - Allocate the shadow and use it as `Planes[0]` (bpr 80, `GBF_DISPLAY`, `BMF_STANDARD`).
  - Pixel table `n & 1`.
  - Program every register: bitmap mode, R26 = 0x00 (blank).
  - VDC-fill 0x0000-0x3E7F.
- **`gd_Show(bm)`:** R26 = pen colours. **`gd_Show(NULL)`:** blank, or `FreeFB` once freed. **`gd_FreeDisplay`:** free the shadow, then `FreeFB` if not shown. Same as hr.c.
- **`gd_SetColor`:** pen 0 sets the background nibble of R26 and pen 1 the foreground, each the nearest RGBI. Other pens keep pixel `n & 1`.
- **`gd_Flush` (new):** pushes the dirty runs.

Changes in graphics.library:

| File | Change |
|---|---|
| `include/graphics/gfxdriver.h` | `void (*gd_Flush)(struct GfxDriver *, struct BitMap *)` at the end; NULL for HR |
| `softsprite.c` | `gfx_SpriteBefore` also locks a display bitmap whose driver has `gd_Flush`. `gfx_SpriteAfter`, `MoveSprite` and `ChangeExtSpriteA` call it after the sprite is shown again |
| `display.c` | card-to-driver table: HR to `hr.gfxdriver`, LC to `lc.gfxdriver` |
| `hrmodeid.h` | LC mode IDs |
| Makefile | `GFXDRIVERS = hr lc` |

- **Why `gd_Flush` and not a push per call:** one sprite show is 3 `gd_Template` calls plus a hide. Pushing per call costs 3 ms a move; one flush per operation costs 1 ms. Every display drawing already passes through `gfx_SpriteBefore/After`, so this is the only hook.
- **Hand-off:**
  - `AllocFB` stops lc.display.
  - `FreeFB` runs `cd_Reset`, which reprograms the VDC for text and reloads the font; the console then replays its ring.
  - A crash takes the card back the same way.
  - The bitmap overwrites the console's text, attributes and font (0x0000-0x2FFF). That is fine if `cd_Reset` reloads all of them (see the questions).
- **Limitation:** programs that write `Screen->BitMap.Planes[0]` directly change only the shadow, so the change never appears.
- **Licensing:** the driver is new MIT code. Nothing is copied from the GPL `devices/lc.c`.

## 4. Pointer and mouse

- **Soft sprite:** save-under copies display to RAM, which reads the shadow at zero port cost. Show and hide draw into the shadow and mark dirty bits.
- **Cost of a move:** the union of the old and new rows is about 13 rows of 4-5 bytes, so 13 x (25 + 4 x 15 µs) ≈ **1.1 ms**. A packet arrives every 41.7 ms, which is about 3% CPU, or 5% at two events per packet.
- **What wb900 needs from gameport.device** (unit 0, same as HR, exists with LC):
  - `GPD_SETCTYPE` with `GPCT_MOUSE`, `GPD_SETTRIGGER`, `GPD_READEVENT`.
  - `IECLASS_RAWMOUSE` events: relative `ie_X`/`ie_Y` with **Y down** (Mouse Systems sends Y up), `IECODE_LBUTTON`/`RBUTTON`/`MBUTTON` with `IECODE_UP_PREFIX`, and `IEQUALIFIER_RELATIVEMOUSE` plus the buttons held.
  - Preferably an event per packet half (dX1/dY1 after byte 3, dX2/dY2 after byte 5), for about 48 Hz instead of 24 Hz.
- **Pixel aspect:** pixels are 2.4:1, so vertical speed may need halving in input.device. Tune on the board.

## 5. Colour

| Pen | WB 1.x | LC mono (default) |
|---|---|---|
| 0 background | blue | RGBI blue (R26 low nibble) |
| 1 text, shine | white | RGBI bright white (R26 high nibble) |
| 2 black | black | pixel 0 (background) |
| 3 orange | orange | pixel 1 |

- **Prefs:** pens 0 and 1 are a real palette of 16 RGBI choices. "White paper" is pen 0 white and pen 1 black, set from prefs with `SetRGB4`.
- **No attributes:** an 8x8 cell holds 2 colours, but intuition draws 3-4 pens per title bar and gadget, so colours would clash at every cell. Mapping pens to cells would need a non-Amiga pen model, and attributes cost CPU slots. Mono only, with no attribute phase planned.

## 6. Risks

| Risk | Mitigation |
|---|---|
| The 65 KB/s push makes unaligned window drags and diagonal-heavy drawing slow | Fast paths; measure in phase 4 |
| The real slot model differs from the core reading | Count port operations in tests; time on the board |
| Interrupt latency (SCC at 9600 baud has a 3-byte FIFO) | At most 1 ms per piece under `Disable()` |
| NVI storm if a sequence runs with NVI on | Every piece runs under `Disable()` and ends with a ready status read; the test checks the stray-NVI count |
| Direct plane writes are invisible | Document it; wb900's own tools draw through graphics |
| RAM: 18 KB shadow and dirty bits, about 4 KB code | Acceptable on 512 KB |

### Questions for c900-c0

1. Is the slot reading right for these registers: bitmap fetch in columns 0-79, refresh 80-85, CPU actions only in columns 86-127 on display lines, 2 slots per R31 write, 1 per fill byte, 2 per copy byte?
2. How many wait states does the card's /WAIT add to a 0x0181/0x0183 access at 6 MHz (we assume 1)?
3. On the core and on a real 8568, may R19 follow R18, or R31 follow R19, without a ready poll? Does R30 = 0 mean 256?
4. Does the core do R8 = 3 interlaced bitmap at 400 and 480 lines, and do lc_video, the scandoubler and the HDMI scaler show it?
5. What aspect does the scaler output for LC 640x200, and what RGB levels are blue and bright white (CGA palette 0)?
6. Will the commodore-900-emulator or the Go model get an 8568?

### Questions for exec816

1. Serial mouse: will gameport.device unit 0 present the HR interface listed in section 4 (Y sign, M button, events per packet half)? What is its module name, and is it loaded when `fbi_Card == FBC_LC`? Who keeps serial.device off U74 A?
2. Does `Disable()` mask NVI as well as VI? internals.md says both in one place and VI only in another. Is there a call that clears only NVIE, so VI can stay on during VDC sequences? What is the longest VI-off window allowed (we plan 1 ms)?
3. Does LC `FreeFB` run `cd_Reset` with the full init (every register, the font), so any VDC state graphics leaves is fine? Does anything but the NVI status read touch the VDC while it is lent?
4. May graphics reprogram the LC mode (bitmap, R26, later interlace) under the "mode is a strap" framebuf contract? Should `fbi_Modes` stay one entry, with the driver owning the LC mode list?

## Phases and tests

The commodore-900-emulator and the Go model have no LC card. Until the board, tests are emulator root programs linking the driver sources against a C model of the 8568's port interface instead of the assembly port layer.

| Phase | Work | Test |
|---|---|---|
| 0 | Answers to the questions above (c900-c0's: below) | n/a |
| 1 | Assembly port layer (select, poll, push run, fill, copy) and its C model: registers 18/19/24/30/31/32/33, auto-increment, random busy counts, lost writes when not ready | `tests/lcvdc.c`: model and layer agree; a write without a poll is caught |
| 2 | `lc.gfxdriver`: shadow, wrappers, dirty bits, `gd_Flush`; the graphics changes | `tests/lcgfx.c`: after each scripted operation (RectFill, Text, Draw, BltBitMap, sprite move) model VRAM equals the shadow; port-operation counts printed as a cost regression; HR tests unchanged |
| 3 | Fill and copy fast paths, overlap cases | `lcgfx` cases: aligned, unaligned, up, down, rightward same-row, full width |
| 4 | Board: Workbench on LC, serial mouse | Boot to Workbench; a timing program (timer.device) for clear, text line, scroll, window open and pointer move against section 2; stray-NVI count 0; console back after `LoadView(NULL)` |
| 5 | Optional: 640x400 interlace, colour prefs | Same, at the new size |

## Confirmed by c900-c0 (2026-11-15)

Source: C900_MiSTer rtl/vendor/vdc856x and rtl/lc_display.

- **Slots:** the CPU gets one RAM slot per column, after fetch and refresh. R36 resets to 0 in the core, so set it to 5. With R1=80, the free columns are 86-126: 41 a display line and about 121 a blank line. An R31 write takes 2 slots, a fill byte 1, a copy byte 2. That is about 20 R31 bytes a display line, so a 16 KB push takes 1.5-2 frames.
- **/WAIT:** it covers only the register access, never the RAM slot. 0 extra waits at 6 MHz, about 5-6 at 48 MHz.
- **Ordering:** R18, then R19, then R31 back to back is safe. But writing R31 while an update is pending drops the earlier byte, and there is a 1-clock race. Poll status bit 7 before each R18/R19/R30/R31 write.
- **Fill:** fills with the last byte written to R31. R31 writes one byte itself, so N bytes = R31 value, then R30 = N-1. R30 = 0 means 256 (the same for copy). Unverified on a real 8568.
- **INTR:** goes to _NVI on update-ready 0 to 1 and is cleared by a status read.
- **Interlace:** implemented in the core, but untested at 400/480 lines. The scaler gets no field flag, so expect bob. Stay non-interlaced.
- **Output:** the aspect is the shared OSD option (4:3 gives non-square pixels). The palette is fixed CGA with index bits R=8 G=4 B=2 I=1: 0 000000, 2 0000C4, 7 4EF3F3, 15 FFFFFF, C C47E00 brown.
- **Testing:** no 8568 is planned for either emulator. Test against our C port model, then the board.

## Answered by exec816 (2026-11-15)

- **Mouse:** gameport.dev picks the serial backend (U74 A, held exclusively) when the card is not HR, with HR's interface: an event per half packet, Y down positive, three buttons. It loads only if the disk's `Modules` lists it; software cannot read the OSD bit. Its board test is pending, so the first LC images leave it out.
- **Interrupts:** `Disable()` masks VI and NVI. There is no NVI-only call. A VI-off section may take 1 ms.
- **FreeFB:** `cd_Reset` rewrites every writable register (R37 included), reloads the font and clears the planes. While lent, only the NVI's status read and a crash takeover touch the 8568.
- **Mode:** the owner may reprogram the whole chip and must leave it idle with no INTR pending. The driver owns the LC mode list.
- The kernel's console is now textcon.device; console.device is the window console.

## Built (2026-11-15)

| Part | Where |
|---|---|
| Port layer (assembly) | `libs/lc/lcport.s`, `lcport.h` |
| Driver, 6.1 KB code, 1.8 KB BSS, 16 KB shadow | `libs/lc/lc.c` → `LIBS:lc.gfxdriver` |
| `gd_Flush`, card table, LC mode ID | `gfxdriver.h`, `softsprite.c`, `display.c`, `hrmodeid.h` |
| 8568 port model, port-access shims | `tests/lc/vdcmodel.c`, `tests/lc/vdcio.s` |
| Tests | `tests/lcvdc.c` (phase 1), `lcgfx.c` (2, 3), `lcwb.c` (Workbench) |
| Board | `lcdemo.bin`, `wbboot-lc.bin`, `lcbench.bin` (`tests/lc/bench.s`) |

The tests run the driver's own `lcport.s`: the Makefile turns each port instruction into a call to the model. A fake framebuf.resource reports an LC card, and a patched `Enable()` counts pieces that end with the chip busy.

### Deviations

- **Dirty tracking:** see [Board run 1](#board-run-1-and-the-cpu-cost-2026-11-15).
- **Fills** do not flush first, as pushes read the shadow. Partial edge bytes of fills and copies are pushed beside each row's block, without a seek.
- **`hrmodeid.h`** keeps its name and gains `LC_MONITOR_ID` and `LCMONO_200_ID`.
- **Push rate:** with c900-c0's slots (about 20 R31 bytes a display line) a push is CPU-bound at about 8.2 µs a byte. The tests still price it at 15 µs.

### Measured port actions (lcgfx, lcwb), priced at the plan's rates

| Operation | Plan | Estimate |
|---|---|---|
| Full-screen clear | 17 ms | 16.6 ms (block fill) |
| Text line, 80 chars JAM2 | 10 ms | 9.6 ms (640 bytes, 1 seek) |
| Scroll, full width, 8 rows | 32 ms | 32-33 ms |
| Scroll, 40-byte window | 30 ms | 23 ms |
| Window move 300x100, aligned | 8 ms | 14 ms: each row needs R18/R19, R32/R33 and R30 |
| Window move 300x100, unaligned | 60 ms | 60 ms |
| Pointer move | 1.1 ms | 0.8-1.0 ms (lcwb through input.device: 1.2 ms) |
| Window open 320x120 | 50 ms | 50 ms |
| Full-screen push | 0.25 s | 0.24 s |
| Allocating the display | n/a | 33 ms (init fill, then AllocBitMap's clear) |

### Open points

- **Window drags** through intuition push the whole window (about 80 ms for 320x120): layers moves a layer through its backup bitmaps, so the driver sees a RAM-to-display copy. A display-to-display blit in layers would take the VDC copy (about 15 ms).

## Board run 1 and the CPU cost (2026-11-15)

Core C900_74ce82a, 6 MHz: every case took 2-10 times the plan (clear 93 ms, scroll 100 ms, aligned move 250 ms, pointer move 30 ms, window open 1.16 s). The chip was not the limit; the driver's C was:

- `lc_room` divided (DIVL, about 740 clocks) before every port action, and each action was 3-4 C calls.
- Every row of a fill, copy or flush went through C, and the 3-span merge ran per row of every primitive.
- Each primitive flushed: a window open pushed the same rows about 6 times (786 row pushes).

Emulator instruction counts (no port waits), before and after; RAM is graphics' drawing into a RAM bitmap, the floor:

| Case | Before | After | RAM |
|---|---|---|---|
| Clear | 45 k | 18 k | 11 k |
| Text line, 80 chars | 25 k | 22 k | 16 k |
| Scroll, full width | 26 k | 15 k | 7 k |
| Scroll, 40-byte window | 93 k | 30 k | 7 k |
| Window move 300x100, unaligned | 86 k | 67 k | 26 k |
| Window move 300x100, aligned | 75 k | 26 k | 3 k |
| Pointer move | 12 k | 9 k | 6 k |
| Window open 320x120 | 450 k | 255 k | 120 k (no driver) |

Changes:

- **Port layer** (`lcport.s`, `lcport.h`): row loops for fills and copies with their edge bytes, the flush, and dirty marking are assembly. The piece budget is a subtraction; a piece ends inside the layer (ready, Enable, Disable). A seek writes R18 only when the high byte changes. R31 stays selected across a run.
- **Dirty tracking:** a row keeps one span while marks touch it (gap of 1 byte or less), else one bit a byte. A flush finds dirty rows with CPIRB.
- **Batching:** `OwnBlitter()`/`DisownBlitter()` in graphics hold flushes; intuition wraps each action in them. Window open: 1332 pushed bytes to 699.
- **Mouse:** `lcdemo.bin` and `wbboot-lc.bin` list gameport.dev in `Modules`.

Expect about 11 clocks an instruction, so roughly: clear 45 ms, scroll 55 ms, 40-byte scroll 55 ms, aligned move 50 ms, pointer move 16 ms, window open 0.5 s. `lcbench.bin` measures what is left: clocks per select, seek, R31 write (polled and not, lost bytes read back), R30 fill and R32/R33 copy by block size, display on and with R1 = 8, the port layer's loops, and graphics' RAM drawing.

