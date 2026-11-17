# LC card driver

`LIBS:lc.gfxdriver` (MIT, `libs/lc/`) makes the LC card's 8568 VDC graphics.library's display: 1 bit deep, bitmap mode, attributes off. Nothing is taken from the GPL `devices/lc.c`.

| Part | Where |
|---|---|
| Driver | `libs/lc/lc.c` |
| Port layer (assembly) | `libs/lc/lcport.s`, `lcport.h` |
| Graphics hooks | `gd_Flush`, `gd_Adopt` in `gfxdriver.h`; `softsprite.c`; card table in `display.c`; mode IDs in `hrmodeid.h` |
| 8568 port model | `tests/lc/vdcmodel.c`, `vdcio.s` |
| Tests | `lcvdc`, `lcgfx`, `lcwb` (model); `lcdemo`, `lcmodes`, `lcbench`, `wbboot-lc.bin` (board) |

## Layers

```
 Workbench, apps
        |
 intuition ---- input.device <-- gameport.device u0 <-- Mouse Systems on U74 A
        |          (MoveSprite)                           [Exec900]
 layers
        |
 graphics.library: clipping, soft sprite, gfx_Planar row engine
        |   GfxDriver table (+ gd_Flush, gd_Adopt)
 lc.gfxdriver
   wrappers: draw into shadow -> mark dirty bytes | fast path
   gd_Flush: dirty runs -> R31 push        fast path -> R30 fill/copy
        |                                    |
 shadow in RAM (<= 38 KB)          port layer (asm): 0x0181 / 0x0183
                                             |
                       8568, 64 KB VRAM: slots of bitmaps, scratch 0xFFB0-0xFFFF
 framebuf.resource AllocFB/FreeFB <-> lc.display (text console) [Exec900]
```

## VRAM model

The CPU reaches VRAM only through registers: R18/R19 the update address, R31 data (auto-increment), R30 block count, R32/R33 copy source, R24 bit 7 copy (else fill). Reading VRAM costs as much as writing, so the driver never reads it.

- **Shadow:** each display bitmap is a RAM plane drawn by graphics' planar engine. It is the truth; VRAM follows it.
- **Slots:** VRAM holds as many bitmaps of the mode as fit (at most 8), each on a 256-byte boundary so R12 alone moves the display start. 0xFFB0-0xFFFF is copy scratch.

```
 drawing --> shadow --mark--> dirty spans --gd_Flush--> R18/R19, R31 --+
                \                                                      v
                 +-- fill of one byte value / aligned copy --> R30 --> VRAM slot
```

## Modes

All 1 bit, R27 = 0, R28 64 KB, R36 = 5 refresh cycles a line, R37 = 0xC0 (positive syncs). The other registers are lc.display's 80x25 text timing.

| ModeID | Name | Ticks | Bitmap | Slots | R0 R1 R2 R22 R25 R34 R35 | R4 R5 R6 R7 | Line, frame |
|---|---|---|---|---|---|---|---|
| `LCHIRES_ID` 0x00110000 | LC:640x200 | 22 x 53 | 16000 B | 4 of 0x3F00 | 7E 50 66 78 87 7D 64 | 20 00 19 1D | 63.5 µs x 264, 59.65 Hz |
| `LCLORES_ID` 0x00110100 | LC:320x200 | 44 x 53 | 8000 B | 7 of 0x2000 | 3F 28 33 89 97 3E 32 | 20 00 19 1D | 64 µs x 264, 59.18 Hz |
| `LCHIRESPAL_ID` 0x00110200 | LC:640x256 | 22 x 41 | 20480 B | 3 of 0x5000 | as 640x200 | 26 00 20 23 | 63.5 µs x 312, 50.47 Hz |
| `LCLORESPAL_ID` 0x00110300 | LC:320x256 | 44 x 41 | 10240 B | 6 of 0x2800 | as 320x200 | 26 00 20 23 | 64 µs x 312, 50.08 Hz |
| `LCHIRESLACE_ID` 0x00110004 | LC:640x400 Interlace | 22 x 26 | 32000 B | 2 of 0x7D00 | as 640x200 | 1F 0D 19 1D | 63.5 µs x 525, 59.99 fields |
| `LCLORESLACE_ID` 0x00110104 | LC:320x400 Interlace | 44 x 26 | 16000 B | 4 of 0x3F00 | as 320x200 | 1F 0D 19 1D | 64 µs x 525, 59.52 fields |
| `LCHIRESLACE480_ID` 0x00110404 | LC:640x480 Interlace | 22 x 22 | 38400 B | 1 | as 640x200 | 1F 0D 1E 1F | 63.5 µs x 525, 59.99 fields |
| `LCLORESLACE480_ID` 0x00110504 | LC:320x480 Interlace | 44 x 22 | 19200 B | 3 of 0x4B00 | as 320x200 | 1F 0D 1E 1F | 64 µs x 525, 59.52 fields |

- **Lores:** R25 bit 4 halves the character clock; R22 = 0x89 shows a column as 8 doubled pixels. Shadow and VRAM are 40 bytes a row, no doubling in software.
- **Ticks:** a pixel's size on a 4:3 screen, 22 for 640 across (Amiga hires is 22 x 44). Both are over 2:1, so gadtools and sysiclass pick hires imagery. Interlaced IDs set LACE (bit 2) and `DIPF_IS_LACE`.
- 640x200 is WB 1.x hires: topaz 8 gives 80x25 text. AROS's 1024x800 default falls back to it.
- `make LCMODE=640x400 wbboot-lc.bin` boots the Workbench in that mode through ENV:Sys/screenmode.prefs; Shift at boot ignores it.
- **Not built:** overscan (not Amiga sizes, needs board tuning), 640x512 interlaced (not asked for), attribute colour (see [Colour](#colour)).

## Interlace

The MiSTer core, with R8 = 3 and R25 bit 7, loads R12/R13 once a frame at the even field and fetches the odd field on from where the even one ended. It flags the odd field to the scaler, which weaves.

- **Registers:** R8 = 3, R9 = 0x0F (16-line rows, 8 a field), R23 = 0x10, R4 = 0x1F, R5 = 0x0D (262.5 lines a field), R6 = 25 or 30 rows. R9 = 15 splits the fields exactly.
- **Layout:** the shadow is top to bottom; a slot holds the even rows, then the odd ones. The port layer turns shadow rows into VRAM addresses:

```
 VRAM slot (640x400, bpr 80)        row y at Base + (y/2)*bpr + (y&1)*200*bpr
 +--------------------------+ Base
 | rows 0, 2, 4 ... 398     |  even field
 +--------------------------+ Base + 200*80
 | rows 1, 3, 5 ... 399     |  odd field
 +--------------------------+
 lp_Step = { rows/2*bpr, bpr - rows/2*bpr }   progressive: { bpr, bpr }
```

- **Fills** of whole rows are one block a field.
- **Copies** of whole rows by an even number of lines stay in their field: one block a field. An odd number crosses fields, so rows copy one at a time in shadow order (scroll by 1 at 640x400: 82 ms, by 2: 65 ms). Partial rows always go a row at a time.

### 480-line vsync

The core counts shown rows from 1 (row 0 holds the R5 adjust), so R7 = R6 starts VSYNC over the last shown row. With R7 = 0x1E and R6 = 30, VSYNC covered field lines 232-239; the MiSTer video path blanks under VSYNC and the odd field's VSYNC rises mid-line, so the scaler saw a short line and garbled the frame. R7 = 0x1F puts VSYNC in row 31, below the picture, leaving 22.5 blank lines a field. 400-line modes keep R7 = 0x1D.

## Port costs

From the core's RTL at 6 MHz; the board decides.

| Item | Value |
|---|---|
| CPU RAM slot | one a column (0.5 µs) after fetch (columns 0-79) and refresh; free columns 85-126 on a display line, about 122 on a blank line |
| Slots per R31 byte / fill byte / copy byte | 2 / 1 / 2 |
| /WAIT | covers the register access only: 0 waits at 6 MHz |
| Push loop (poll, OUTIB, JR) | about 49 cycles, 8.2 µs a byte; CPU-bound, as a display line takes about 20 R31 bytes |
| Seek (R18, R19, select R31) | about 25 µs |
| Block fill | about 1 µs a byte; copy about 2 µs a byte |

- Poll Update Ready (status bit 7) before every R18/R19/R30/R31 write: a write while busy replaces the pending action and loses a byte.
- R31 writes one byte itself, so N bytes are an R31 write then R30 = N-1. R30 = 0 means 256, for fill and copy.
- A seek writes R18 or R19 only when its byte changes; R31 stays selected across a run.
- The layer prices a pushed byte at 16 µs and a register write at 10 µs against its piece budget.

## Drawing

`gd_Fill`, `gd_Template`, `gd_Copy`, `gd_WritePixel`, `gd_Line` and `gd_WriteRow` run gfx_Planar on the shadow, then mark dirty bytes or take a fast path. Bitmaps without a slot get the planar table. `gd_ReadPixel` and `gd_ReadRow` are gfx_Planar's.

- **Dirty tracking:** a row keeps one span while marks touch it (gap of 1 byte or less), else one bit a byte. Lines mark per-row x spans, not the bounding box.
- **Flush:** finds dirty rows with CPIRB and pushes runs, bridging gaps under 2 clean bytes (a seek costs about 2 pushed bytes).
- **Fill fast path:** plane 0 in the mask, not COMPLEMENT, 4 or more whole bytes, and a row byte that is constant (solid, or a pattern row whose bytes agree). Full-width rows fill as one block; partial edge bytes are pushed beside each row's block, without a seek. No flush first.
- **Copy fast path:** display to itself, minterm 0xC0, `sx%8 == dx%8`, 4 or more whole bytes. Flush, copy the shadow, then VDC-copy each row's interior; moving down goes bottom-up in blocks that do not overlap their source; a rightward move within one row goes through scratch. R24 bit 7 is set before each copy.
- **Batching:** every display drawing passes `gfx_SpriteBefore/After`, which call `gd_Flush` once the sprite is back; `OwnBlitter()`/`DisownBlitter()` hold flushes, and intuition wraps each action in them. One flush per operation, not per call: a sprite show is 3 `gd_Template` calls plus a hide.
- **Interrupts:** the 8568 raises INTR (to the NVI) on every update-ready edge until the status is read. Every port sequence runs under `Disable()` (masks VI and NVI) in pieces of at most 1 ms, each ending with a status read that finds the chip ready.

## Display, hand-off, screen switches

- **`gd_AllocDisplay`:** `AllocFB("graphics.library")` stops lc.display; allocate the shadow, program R0-R37 (R26 = 0, blank), clear slot 0 with a block fill.
- **`gd_Show`:** R26 = pen colours, or 0 to blank. Once the display is freed and not shown, `FreeFB` runs lc.display's `cd_Reset`: every writable register, the font, cleared planes; the console replays its ring. A crash takes the card back the same way.
- **Switches:** screens behind keep their shadow and slot and are pushed as they are drawn, so a switch only writes R12 and R26 (1.6 ms at 640x200, mostly the pointer moving; 16 KB pushed would be 240 ms). `gd_Adopt` gives the incoming bitmap its slot, or a free one, or the one longest behind (with one slot, the front's), and pushes it whole: 0.24 s at 640x200, 0.49 s at 640x400, 0.58 s at 640x480. Drawing to a bitmap behind makes its slot the port's target, after pushing the last target's dirty bytes. Freeing a bitmap behind frees its slot; freeing the display sends the others back to RAM.

## Pointer and mouse

- **Pointer:** graphics' soft sprite. Save-under reads the shadow at no port cost; a move pushes about 13 rows of 4-5 bytes, about 1 ms.
- **Mouse:** Exec900's gameport.dev, unit 0, picks the serial backend on U74 A when the card is not HR, with HR's interface: an event per half packet, Y down positive, three buttons. It loads only when listed in `Modules`; the LC images list it.

## Colour

| Pen | WB 1.x | LC |
|---|---|---|
| 0 | blue | background, R26 low nibble |
| 1 | white | foreground, R26 high nibble |
| 2 | black | pixel 0 |
| 3 | orange | pixel 1 |

`gd_SetColor` maps pens 0 and 1 to the nearest of the core's fixed CGA RGBI colours (R=8 G=4 B=2 I=1); the default is 2 (0000C4) on 15 (FFFFFF). Other pens keep pixel `n & 1`.

Attribute colour is rejected: an 8x8 cell holds 2 colours but intuition draws 3-4 pens per title bar and gadget, and the attribute fetch takes CPU slots.

## Measurements

Port actions counted on the model by `lcgfx` and `lcwb`, priced at the costs above:

| Operation (640x200) | Time |
|---|---|
| Full-screen clear | 16.6 ms |
| Text line, 80 chars JAM2 | 9.6 ms |
| Scroll, full width, 8 rows | 32-33 ms |
| Scroll, 40-byte window | 23 ms |
| Window move 300x100, aligned | 14 ms |
| Window move 300x100, unaligned | 60 ms |
| Pointer move | 0.8-1.0 ms (1.2 ms through input.device) |
| Window open 320x120 | 50 ms |
| Full-screen push | 0.24 s |
| Allocating the display | 33 ms |

At 640x400: clear 33 ms; scroll by 2 lines 65 ms, by 1 or 3 lines 83 ms; 40-byte window by 1 line 48 ms.

The CPU, not the chip, sets the board's speed. Emulator instruction counts, against graphics drawing into a RAM bitmap:

| Case | Driver | RAM |
|---|---|---|
| Clear | 18 k | 11 k |
| Text line, 80 chars | 22 k | 16 k |
| Scroll, full width | 15 k | 7 k |
| Scroll, 40-byte window | 30 k | 7 k |
| Window move 300x100, unaligned | 67 k | 26 k |
| Window move 300x100, aligned | 26 k | 3 k |
| Pointer move | 9 k | 6 k |
| Window open 320x120 | 255 k | 120 k |

## Tests

The emulators have no 8568. The tests link the driver's own `lcport.s`, each port instruction turned into a call to a C model of the port interface; a fake framebuf.resource reports an LC card, and a patched `Enable()` counts pieces ending with the chip busy. After each step the bitmap the model shows equals the shadow, and `lcm_frame()` derives from the registers what is shown (start, width, stride, lines above VSYNC, fields, dot width, field rate). The model reads unused register bits as 1, as the chip does.

On the board, `lcmodes` shows each mode with a test pattern, alternates two bitmaps, prints timings and the field rate (status bit 5), and reads each shown bitmap back through R31 to compare with the shadow. `lcbench` times select, seek, R31, R30 fill and R32/R33 copy, and checks copies wrote the source bytes.

## Limits

- Programs writing `Screen->BitMap.Planes[0]` directly change only the shadow; it never shows.
- Window drags through intuition push the whole window (about 80 ms for 320x120): layers moves a layer through backup bitmaps, so the driver sees a RAM-to-display copy.
- 50 Hz and lores modes are untested on the board.
- Block copy via R24 bit 7 is unverified on a real 8568; so are fill and R30 = 0 as 256.
