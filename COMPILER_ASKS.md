# Requested MWC compiler changes

Target: commodore-900-toolchain, cc0 front end (cc1 where noted). Motivation: HAZARDS.md. Each later ask's heading gives its status; 13, 17, 18 and 20 are open.

1–4 landed on toolchain main as 4bf4702, 2026-11-14 (prototypes bbcf7d6, long switch 8a9a078, `//` 20be671, mixed declarations 5027c09). Existing code stays byte-identical; native and host builds match.

Limits as delivered:
- Prototype checks compare promoted types only; pointee types and struct tags are not compared.
- `f(T)` with T a typedef now parses as a prototype.
- The int-in-`...` warning is behind `-VSRTVC`.
- A `//` with a line splice between the slashes isn't recognised.
- Avoid `-VDEBUG`: cc2 crashes in fixdbgt(), and mixed declarations trigger it more often.
- Native cc0 grows by 3.9 KB text and 0.8 KB data. Each distinct prototype costs 4 bytes plus 1 per parameter, in a 32 KB pool.

## Rules for all four

- **Additive.** Code that compiles today compiles byte-identical. The self-host fixed point, `check-native` and COHERENT kernel/userland builds don't change.
- **No ABI change.** Objects built with and without these features link and call each other as today.
- **Native too.** They work in the native cc0 on the C900, within its 64 KB data segment.

## 1. Function prototypes and ANSI definitions

**Accept:**
- Parameter type lists in every function declarator: declarations, definitions, typedefs, casts, struct members, `sizeof`, and nested function-pointer parameters.
- Named or unnamed parameters, `(void)` for no parameters, and a trailing `, ...`.
- ANSI definitions `T f(T1 a, T2 b) { ... }`, which mean the same as the K&R form. Parameter names in a prototype are scoped to the prototype.

**At a call through a prototype** (a direct call, or a call through a pointer of prototyped type):
- Each fixed argument is converted as if assigned to its parameter type:
  - int to long sign- or zero-extends.
  - A constant 0 to a pointer is a 32-bit null.
  - Pointer to pointer works as assignment does today.
  - Integer to pointer, or pointer to integer, without a cast is an error.
- A wrong argument count is an error.
- Arguments matching `...` get the default promotions: char/short become int, float becomes double. A 16-bit int therefore stays 16 bits.
- A call with no prototype in scope behaves as today.

**ABI:**
- Each parameter is passed as its promoted type: char/short/int as 16 bits, long and pointers as 32, float as double.
- So a prototyped callee is interchangeable with a K&R one, and a K&R definition after a prototype is accepted when the promoted types agree.

**Checking:**
- Error on a redeclaration whose parameter list or return type conflicts.
- An empty `()` list stays "unspecified" and is compatible with any prototype.

**Optional:** a warning for an int-typed expression in a `...` position, catching most tag-list mistakes (hazard 3).

**Tests:**
- int and 0 passed to LONG and pointer parameters
- wrong argument count
- prototype/K&R mixing, and a prototype followed by a K&R definition
- function-pointer prototypes in struct members and casts
- `...` with promotions
- one object compiled with prototypes linked against one compiled without

## 2. `//` comments

- cc0's lexer and preprocessor accept C99 line comments, ending at the newline.
- Not recognised inside string or character constants.
- A comment inside a `#define` ends the macro body.
- Backslash-newline splicing happens before comment recognition, as C99 specifies.
- **Tests:** comments after code, inside directives, inside strings (not a comment), and spliced lines.

## 3. Declarations after statements

- A block may contain declarations anywhere. Each name is in scope from its declarator to the end of the block.
- An initializer runs where it appears.
- `for (T i = e; ...; ...)` declares `i` with the loop as its scope.
- Shadowing an outer name follows the existing block rules.
- Jumping into a block past an initialized declaration is allowed and leaves the value indeterminate (C99 without VLAs).
- Frame layout may differ from equivalent top-of-block code; code with all declarations at the top must not change.
- **Tests:** declaration after statements, use before and after the declaration, a `for` declaration with an outer variable of the same name, nested blocks, `goto` past a declaration.

## 4. `switch` on long and unsigned long

- The controlling expression may be int, unsigned, long, unsigned long, char or enum. Each case label is a constant expression converted to the promoted controlling type.
- Duplicates are checked after conversion.
- Values span the full 32-bit range, including `0x80000000` and above (`TAG_USER` tags).
- **Code generation (cc0 and cc1):** a jump table when the range is dense, else a compare chain or binary search. Sparse tag values must not produce a table.
- Int switches generate exactly the code they do today.
- **Tests:**
  - tags at `TAG_USER` + n
  - negative longs
  - values that collide only in their low 16 bits
  - default handling
  - fallthrough
  - a dense long range that uses a table

## 5. Pointer cast to an integer in a static initializer (opened 2026-11-14; landed in toolchain 337f2b6)

cc1 rejects an address constant cast to a 32-bit integer, which AROS uses for IPTR tables (keymaps, tag lists):

```c
static char a[] = "x";
char *p[] = { a };				/* accepted */
unsigned long t[] = { (unsigned long)a };	/* cc1: initializer too complex */
```

Expected: the relocated long the pointer initializer emits. **Blocks** keymap.library: libs/keymap/defaultkeymap.c, the `DEAD()`/`STRING()` entries of `lokeymap` and `hikeymap` (40 sites from line 384).

## 6. Automatic struct initialised from a struct expression (opened 2026-11-14; landed in toolchain 337f2b6)

cc0 rejects the C89 form, which AROS uses to copy rectangles:

```c
struct R { short a, b; };
int f(struct R *p) { struct R o = *p; return o.a; }	/* cc0: type clash */
```

`struct R o; o = *p;` and brace initialisers (even non-constant) compile. Expected: the code of the declaration plus the assignment. **Blocks** graphics.library: libs/graphics/andrectregion.c:54, gfxfuncsupport.c:109 (`struct Rectangle part = parts[i], sb;`) and :165 (`struct Rectangle torender = *rr;`). Also **blocks** layers.library: libs/layers/basicfuncs.c:647, :663 and :770 (`struct Rectangle rect = intersect;`). intuition has more.

## 7. cc1 internal error EDFA69 (opened 2026-11-14; landed in toolchain 337f2b6)

```c
void f(unsigned long *p, char c) { *p = (unsigned char)c; }	/* cc1: Internal compiler error: : EDFA69 */
```

`(unsigned char)*s` and `(unsigned char)s->c` crash it too. These compile: `return (unsigned char)c;` from a long function, `*p = c & 0xFF;` and `*p = *ucharptr;`. Not blocking: graphics' getrpattrsa.c, new code, uses `& 0xFF`.

## 8. Wrong code: returning an assignment from a register variable (opened 2026-11-14; landed in toolchain 337f2b6)

```c
struct A { long x; };
struct A *f(struct A **pp) { register struct A *a = *pp; a++; return *pp = a; }
```

cc2 stores `a` (RR10) through `pp` and returns without moving it into RR0: garbage. MWC puts plain locals in registers itself, so AROS code hits this with no `register`. **Blocks** graphics.library's regions, and with them layers: libs/graphics/intregions.c:170 (`return *LastRectPtr = (struct RegionRectangle *)RRE;`) returns 0x00000001, and the next region operation traps.

## 9. Automatic array sized by its initializer (opened 2026-11-14; landed in toolchain 337f2b6)

```c
long f(int i) { long a[] = { 1, 2 }; return a[i]; }	/* cc0: cannot declare flexible automatic array, ... */
```

`long a[2] = { 1, 2 };` and `static const long a[] = { ... };` compile. Expected: the C89 meaning, an array of as many elements as initializers. **Blocks** graphics.library: libs/graphics/gfxnew.c:63 and gfxfree.c:61 (`const ULONG GfxNew_memsizes[] = { ... }`). Also **blocks** layers.library: libs/layers/createbehindlayertaglist.c:92 (`struct TagItem behindtags[] = { ... };`).

Since 337f2b6, a cast of an address to a word or byte in a static initializer is an error.

## 10. `&` applied to an array (opened 2026-11-14; landed in toolchain ca42506)

cc0 rejects the address of an array, which C89 defines as a pointer to the array (at its first element):

```c
struct S { short a[4]; } s;
short b[4];
short *f(struct S *p) { return (short *)&p->a; }	/* cc0: lvalue required */
short *g(void) { return (short *)&b; }		/* cc0: lvalue required */
```

`offsetof(struct S, a)` fails the same way, since it takes `&((struct S *)0)->a`. Expected: the address of the first element. **Blocks** intuition.library: libs/intuition/openscreen.c (`(ULONG*)&IBase->DriPens2` and `memclr(&color_alloced,256)`), openwindow.c (`(struct TagItem *)&layertags`, 3 sites), setprefs.c:74-76 (`offsetof(struct Preferences, PointerMatrix)`, `memcmp(&prefbuffer->PointerMatrix, ...)`).

`offsetof()` of any member is also not an integer constant expression for cc0 (`typedef char t[offsetof(struct S, a) == 0 ? 1 : -1];` gives "array bound must be a constant"), though a static initializer takes it. Nothing in the tree depends on that. Fixed above -VKR by toolchain d2cad6e.

## 11. cc2 internal error 2C59 (opened 2026-11-14; landed in toolchain ca42506)

```c
int f(char *ptr) { char buf[64]; return buf + sizeof(buf) - ptr; }	/* cc2: Internal compiler error: : 2C59 */
```

`&buf[64] - ptr` crashes too; `buf - ptr` and `sizeof(buf) - (ptr - buf)` compile. **Blocks** intuition.library: libs/intuition/strgclass.c, set_buffer_str() (`len = buf + sizeof(buf) - ptr;`).

## 12. Odd literal pool after a byte constant (opened 2026-11-14; landed in toolchain ca463ce)

```c
char *GfxBase;
void f(rp, c) char *rp; int c;
{
	(*(void (*)())(GfxBase - 342))(GfxBase, rp, (unsigned char)(c ? 5 : 0));
	(*(void (*)())(GfxBase - 30))(GfxBase, "xy", "zw");
}
```

cc2 puts the conditional's constant 5 in the data section as one byte, and the address constants after it at odd offsets, which mkhunk refuses ("odd relocation"). `SetAPen(rp, c ? 5 : 0)` through `<lvo/graphics_lib.h>` does it, as in tests/gfxdemo.c. libs/intuition/drawimagestate.c:171 was a site too; its code changes with the fix.

## 13. as: local labels of more than one digit (opened 2026-11-15)

```
	.shri
f_:
12:	dec	r1, $1
	jr	nz, 12b
	ret
```

as-z8001 rejects it (`0003 q`, `0004 u`, `0004 a`); `1:` and `1b` assemble. Only `0:` to `9:` are local, so a routine with over ten branch targets needs names. Not blocking: libs/graphics/gfxasm.s uses names (`tr1`, `ln1`, `sr1`). Expected: any decimal number, as in the Unix assemblers.

## 14. as: RR0 or R0 as a base register (opened 2026-11-15; landed in toolchain 853caaf, merged ecc0421)

```
	.shri
f_:
	ld	r9, rr0(16)
	ldl	rr2, rr0(4)
	ret
```

The Z8000 has no based or indexed mode on register 0, but as-z8001 silently emits direct-address loads (`LD R9,0x0014`, `LDL RR2,0x0004`), reading low memory. Expected: an error, as for any other mode the CPU lacks. Not blocking: libs/graphics/gfxasm.s now uses RR10 there.

## 15. Wrong code: members missing from an automatic aggregate's initializer are not zeroed (opened 2026-11-15; landed in toolchain d38dbf2, merged 27fc54d)

```c
struct N { unsigned char t; char *l; char *k; short f; };
char *g(struct N *p);
char *k(void) { struct N n[] = { {1, (char *)10, 0, 3}, {2} }; return g(n); }
```

cc0 stores `n[1].t = 2` and leaves `n[1].l`, `k` and `f` as stack garbage; C89 zeroes them, as for a static. **Blocks** asl.library: its menus' `{NM_ITEM, NM_BARLABEL}` entries reach gadtools' CreateMenusA() with a garbage `nm_CommKey`, which traps. Meanwhile libs/asl spells out the zeros in its NewMenu tables. AROS's `{TAG_DONE}` entries are hit too, harmlessly.

## 16. `readonly` is a keyword (opened 2026-11-15; landed in toolchain 98b790c, merged 27fc54d, under -VC89 and -VC99 only)

```c
struct S { char readonly; };	/* cc0: class not allowed in structure body */
```

cc0 is built with `READONLY`, making `readonly` (and `alien` with `ALIEN`) a storage class; standard C has neither. Not blocking: libs/asl renamed AROS's listview field `isreadonly`. Expected: both are ordinary identifiers.

## 17. Wrong code: `long` with `unsigned int` is unsigned (opened 2026-11-15; landed in toolchain cd3e7d4)

```c
long h(long dx, unsigned c) { return dx / c; }		/* h(-100, 49) is not -2 */
long m(long dx) { return (dx * 0xFFFF) / 7L; }		/* m(-1) is not -9362 */
```

With a 16-bit int, C89 converts the `unsigned int` to `long`, which holds all its values, so both are signed. cc2 divides unsigned (no EXTSL before DIVL; `long / 7L` alone gets one). Not blocking: intuition's prop knob dragged past its container's start jumped to the far end; propgadgets.c now casts both operands to LONG.

## 18. 16-bit multiply and divide go through the 32-bit instructions (opened 2026-11-15; landed in toolchain 9ef8108 and f8c24f0)

```c
unsigned short q(unsigned short a, unsigned short b) { return a / b; }
long p(short a, short b) { return (long)a * b; }
unsigned long u(unsigned short a, unsigned short b) { return (unsigned long)a * b; }
```

cc2 gives `q` a DIVL (zero-extended to 64 bits, about 740 clocks) and `p` and `u` a MULTL (about 300), where DIV (32 by 16, about 107) and MULT (16 by 16 to 32, about 70) give the same result: for `p` always, for `u` and `q` when the operands are under 32768 or the code corrects for the sign. Row offsets (`(ULONG)y * BytesPerRow`) and line steps in graphics.library spent 10-20% of a line's setup there. Not blocking: libs/graphics/gfxasm.s has `gfx_MulW` and `gfx_MulDiv`. Expected: MULT for a product of two 16-bit operands widened to long, and DIV where the divisor and quotient are known to fit.

## 19. `srlb`/`srab` by a constant: as and the emulator disagree on the count (opened 2026-11-15; as is right; the emulator, fixed in db4d6fe)

```
	ldb	rl0, $0xF8
	srlb	rl0, $1		/ as: B201 00FF; the emulator shifts rl0 left 255: 0, C clear
```

as writes byte shifts' count word with 00 in the high byte (`machine.c`: "byte forms: 00 in the high byte"), so a right shift by 1 is 0x00FF. The emulator reads the word as a signed 16-bit count, sees +255 and shifts left. One of them is wrong; the Z8000 manual decides which. Not blocking: libs/lc/lcport.s shifts the byte as a word (`srl r0, $1`). Expected: the right shift, C the bit shifted out.

## 20. A conditional operand of a 16-bit product keeps MULTL (opened 2026-11-15; landed in toolchain 5c7a167)

```c
unsigned long f(int c, short a, short b, unsigned short w)
{
	return (unsigned long)(c ? a : b) * w;	/* MULTL, 453 clocks */
}
```

18's MULT is missed when the widened operand is a `?:`, with or without a cast to `short` inside: 453 clocks against 164 for `(unsigned long)a * w`. A `?:` nested in a sum, `(unsigned long)(a + (c ? b : 0)) * w`, gets MULT (220). Not blocking: graphics.library puts the `?:` outside the products. Expected: MULT when both arms are 16-bit.

## Out of scope (handled by the converter or by hand)

Designated initializers (~123 sites), statement expressions, `inline`, variadic macros (4 sites), the AROS_LH/UFH macros, and 64-bit integers.
