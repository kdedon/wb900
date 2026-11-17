# MWC compiler: hazards and requests

Toolchain: commodore-900-toolchain (cc0 front end, cc1, cc2, as-z8001).

## Hazards

Still true with the current toolchain.

### Integer widths

- **Calls without a prototype in scope.** An int or `0` argument is pushed as 16 bits, wherever the callee expects a LONG or pointer. Keep prototypes in scope.
- **`...` arguments** get the default promotions only, so an int stays 16 bits. Cast every tag or method slot to ULONG. `-VSRTVC` warns on an int in a `...` position.
- **32-bit shifts and masks.** `red<<16` and `1<<15..18` overflow a 16-bit int; `~0x8000u` zero-extends. Use long constants.
- **UWORD in arithmetic** promotes to unsigned, so `(height - tf_YSize) / 2` and `CWidth * HorizBody` go wrong where AROS's 32-bit int keeps them signed or wide. Cast to WORD or LONG.
- **`y * BytesPerRow` in int** is valid only while a plane fits 64 KB.
- **IPTR casts and arithmetic** run under segmented pointers. IPTR must be 32-bit.

### Prototypes

- Checks compare promoted types only; pointee types and struct tags are not compared.
- A prototyped callee is interchangeable with a K&R one: each parameter passes as its promoted type (char/short/int 16 bits, long and pointers 32, float as double).
- `f(T)` with T a typedef parses as a prototype.
- Each distinct prototype costs 4 bytes plus 1 per parameter, in a 32 KB pool.

### Other limits

- Not supported (the converter or a hand edit removes them): designated initializers, statement expressions, `inline`, variadic macros, the AROS_LH/UFH macros, 64-bit integers.
- A `//` with a line splice between the slashes isn't a comment.
- Avoid `-VDEBUG`: cc2 crashes in fixdbgt(), more often with mixed declarations.
- A cast of an address to a word or byte in a static initializer is an error.
- At `-VKR`, `offsetof()` is not an integer constant expression.
- `readonly` and `alien` are storage classes except under `-VC89` and `-VC99`.
- as-z8001: only `0:` to `9:` are local labels (#13).
- BOOPSI super-method chains are 3-5 deep, each frame holding TagItem arrays: watch stack depth.

### Public headers

- Define `STACKED` (aros/cpu.h, `aligned(4)`) as empty to get the classic packed BOOPSI message layout.
- `text.h:41` uses a statement expression.

## Requests

All additive: code that compiled before compiles byte-identical, objects with and without the features link together, and native cc0 supports them within its 64 KB data segment.

| # | Problem | Status |
|---|---|---|
| 1 | No function prototypes or ANSI definitions | landed in 1a36172 |
| 2 | No `//` comments | landed in 1a36172 |
| 3 | No declarations after statements | landed in 1a36172 |
| 4 | No `switch` on long or unsigned long | landed in 1a36172 |
| 5 | cc1 rejects a pointer cast to an integer in a static initializer | landed in b8ec7d4 |
| 6 | cc0 rejects an automatic struct initialised from a struct expression | landed in b8ec7d4 |
| 7 | cc1 internal error EDFA69 on `*p = (unsigned char)c` | landed in b8ec7d4 |
| 8 | Wrong code returning an assignment from a register variable | landed in b8ec7d4 |
| 9 | cc0 rejects an automatic array sized by its initializer | landed in b8ec7d4 |
| 10 | cc0 rejects `&` applied to an array, and so `offsetof` of an array member | landed in 2fc271c |
| 11 | cc2 internal error 2C59 on `buf + sizeof(buf) - ptr` | landed in 1dcd1d0 |
| 12 | Odd literal pool after a byte constant ("odd relocation") | landed in 1dcd1d0 |
| 13 | as: local labels of more than one digit | open |
| 14 | as: RR0 or R0 as a base register silently assembles a direct address | landed in 31d905a |
| 15 | Wrong code: members missing from an automatic aggregate's initializer not zeroed | landed in 07a477d |
| 16 | `readonly` is a keyword | landed in 691acce, under -VC89 and -VC99 only |
| 17 | Wrong code: `long` with `unsigned int` is unsigned | landed in 691acce |
| 18 | 16-bit multiply and divide go through MULTL/DIVL | landed in 2c8b942 |
| 19 | `srlb`/`srab` by a constant: as and the emulator disagree on the count | as is right; emulator fixed in 75ebf07 |
| 20 | A `?:` operand of a 16-bit product keeps MULTL | landed in 2c8b942 |
| 21 | cc1 internal error EDFA69 on a `?:` of a signed quotient | landed in a9b59bc |

### 1. Prototypes

- At a call through a prototype, each fixed argument converts as if assigned: int to long extends, constant 0 to a pointer is a 32-bit null. Integer to pointer without a cast, and a wrong argument count, are errors.
- A call with no prototype in scope behaves as before.
- An empty `()` list stays unspecified and matches any prototype.
- Native cc0 grew by 3.9 KB text and 0.8 KB data.

### 3. Mixed declarations

`for (T i = e; ...)` scopes `i` to the loop. Jumping past an initialized declaration leaves the value indeterminate.

### 4. switch on long

Case labels span the full 32-bit range (`TAG_USER` tags). Dense ranges get a jump table, sparse ones a compare chain. Int switches generate the same code as before.

### 13. Local labels

```
f_:
12:	dec	r1, $1
	jr	nz, 12b
	ret
```

as-z8001 rejects it; `1:` and `1b` assemble. A routine with over ten branch targets needs names. Expected: any decimal number, as in the Unix assemblers.

### 14. RR0 base

The Z8000 has no based or indexed mode on register 0; as-z8001 now rejects `rr0(16)`.

### 18, 20. 16-bit multiply and divide

cc2 now uses MULT for a product of two 16-bit operands widened to long, and DIV where divisor and quotient fit.

### 19. Byte shifts

as writes the count word of a byte shift with 00 in the high byte, so `srlb rl0, $1` is `B201 00FF`.
