# AROS GUI code: conversion hazards

Survey of AROS eb6ffb3, 2026-11-14, using a heuristic scanner with spot checks. Columns: intuition / graphics / hyperlayers / gadtools / keymap+input.

| | Int | Gfx | Lay | GT | K+I |
|---|---|---|---|---|---|
| files / kLOC | 213/52.6 | 217/28.8 | 50/8.2 | 35/11.1 | 13/3.2 |
| ANSI definitions | 494 | 165 | 46 | 141 | 13 |
| ANSI prototypes | 276 | 111 | 55 | 22 | 10 |
| `//` lines | 254 | 17 | 15 | 6 | 1 |
| mixed declarations (approx.) | ~119 | ~40 | ~11 | ~28 | ~6 |
| AROS_LH/UFH/UFC uses | 821 | 1086 | 337 | 113 | 40 |
| designated initializers | 39 | 71 | 13 | 0 | 0 |
| IPTR casts | 225 | 35 | 11 | 202 | 2 |
| switches on 32-bit values / tag case labels | 59/374 | 11/86 | 0 | 43/162 | 0 |
| variadic tag/method call sites | 74 | 10 | 5 | 26 | 0 |
| files using HIDD/oop / CGX / monitorclass | 11/10/12 | 54/11/0 | 2/2/0 | 0 | 0 |

No floats, no static array over 64 KB, no local array over 256 B, no bool/stdint. Inline asm appears only in m68k/disabled paths.

## Top hazards, by effort

1. **Argument widths without prototypes.** 1017 library parameters, 592 pointers and 172 LONG. Any int or `0` argument is pushed as 16 bits.
2. **Graphics on the driver table.** 54 files, ~830 HIDD/OOP references (allocbitmap.c, gfxfuncsupport.c, graphics_display.c, getdisplayinfodata.c).
3. **Varargs tag/method calls.** ~115 sites; every slot must be cast to ULONG.
4. **switch on 32-bit values.** ~113 switches with 622 tag cases; MWC took int only.
5. **BOOPSI class glue.** 33 classes, 252 methods. AROS's genmodule generates it; we have to.
6. **IPTR casts and arithmetic under segmented pointers.** ~470 casts, ~144 arithmetic expressions.
7. **monitorclass/CGX in intuition.** 34 files.
8. **32-bit shifts and masks in 16-bit int.** For example `red<<16`, `(1<<15..18)` in intuition.h, and `~0x8000u` zero-extension.
9. **Plane addressing and stack depth.** `y*BytesPerRow` in int is valid only while a plane fits 64 KB. BOOPSI super-method chains are 3–5 deep, each frame holding TagItem arrays.
10. **C99/GCC features.** Designated initializers, inline and statement expressions, ~150 sites.
11. **UWORD in arithmetic.** With a 16-bit int, UWORD promotes to unsigned, so `(height - tf_YSize) / 2` and `CWidth * HorizBody` go wrong where AROS's 32-bit int keeps them signed or wide. Cast to WORD or LONG. gadtools had six such sites, intuition's prop knob two.

## Public headers

- Define `STACKED` (aros/cpu.h, `aligned(4)`) as empty to get the classic packed BOOPSI message layout.
- IPTR must be 32-bit.
- `text.h:41` uses a statement expression.

## Manual effort

About 159 of 528 files need manual attention (Int 63, Gfx 70, Lay 6, GT 17, K+I 3). The rest convert automatically, but only with type knowledge (hazard 1).

## Compiler vs converter (decided: compiler)

The MWC donor (COHERENT 4.6.3) never parsed prototypes: `garglist` (gdecl.c:390) takes identifiers only; `cproto` (gdecl.c:720) is a stub. A converter could only match prototype support by becoming a type checker.

So cc0 gained prototype parsing and call-site argument conversion (est. 600–1000 lines), which removes hazard 1 and keeps the headers unchanged, plus `//` comments, mixed declarations (lexer and locals) and switch on long (cc0 and cc1's switch table). Landed: COMPILER_ASKS.md 1–4.
