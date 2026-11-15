/* SPDX-License-Identifier: MIT */
/*
 * hr.gfxdriver: the HR card's bitmap in VRAM as graphics.library's
 * display, in the mode of its jumper.  The strap read can't tell
 * 1024x800 from 1024x1024 and may be wrong about mono or RGBI, so the
 * three jumper modes are listed, the one framebuf.resource reports
 * first and available; opening another takes the jumper to be that.  It
 * draws with graphics' planar or chunky 4-bit operations; the palette is
 * fixed, and pens take its nearest colour.  The card holds one display
 * bitmap.
 * The driver takes the display from the console with the bitmap
 * (blanked until shown) and gives it back when the bitmap is freed and
 * not shown.
 */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <graphics/gfxdriver.h>
#include <graphics/hrmodeid.h>
#include <resources/framebuf.h>

#define HRNAME		"hr.gfxdriver"
#define HRVERSION	45

extern int outb();

struct HRBase {
	struct Library	hb_Lib;
	APTR		hb_Seg;
};

APTR FrameBufBase;

static char hrname[] = HRNAME;
static char hrid[] = "hr.gfxdriver 45.1 (14.11.2026)";
static char hrowner[] = "graphics.library";

static struct FBMode *hr_fbm;
static struct GfxDriver **hr_fmts;
static const struct GfxMode hr_jumper[3] = {
	{ HRMONO_800_ID, 1024, 800, 1, GDF_PLANAR },
	{ HRMONO_1024_ID, 1024, 1024, 1, GDF_PLANAR },
	{ HRCOLOR_ID, 640, 400, 4, GDF_CHUNKY4 }
};
static struct GfxMode hr_modes[3];
static struct GfxBitMap hr_bm;
static UBYTE hr_pixtab[256];
static BOOL hr_alloc, hr_shown;

/* Take the jumper to be m: draw in its format */
static void hr_setmode(struct GfxDriver *gd, const struct GfxMode *m)
{
	struct GfxDriver *f = hr_fmts[m->gm_Format - 1];

	gd->gd_Jumper = m;
	gd->gd_Format = m->gm_Format;
	gd->gd_Fill = f->gd_Fill;
	gd->gd_Template = f->gd_Template;
	gd->gd_Copy = f->gd_Copy;
	gd->gd_ReadPixel = f->gd_ReadPixel;
	gd->gd_WritePixel = f->gd_WritePixel;
	gd->gd_Line = f->gd_Line;
	gd->gd_ReadRow = f->gd_ReadRow;
	gd->gd_WriteRow = f->gd_WriteRow;
}

/* RGBI: a gun that is on is at 0xAA, or 0xFF with I */
static ULONG hr_GetColor(struct GfxDriver *gd, UWORD n)
{
	ULONG on, c;

	if (gd->gd_Format == GDF_PLANAR)
		return (n & 1) ? 0xFFFFFFL : 0;
	on = (n & 8) ? 0xFF : 0xAA;
	c = 0;
	if (n & 4)
		c |= on << 16;
	if (n & 2)
		c |= on << 8;
	if (n & 1)
		c |= on;
	return c;
}

static struct BitMap *hr_AllocDisplay(struct GfxDriver *gd,
    const struct GfxMode *m)
{
	struct BitMap *bm = &hr_bm.gb_BitMap;
	UWORD i;

	if (hr_alloc)
		return NULL;
	if (!hr_shown) {
		if (AllocFB(hrowner) != NULL)
			return NULL;
		outb(FBHR_CTL, FBHR_BLANK);
	}
	if (m != gd->gd_Jumper)
		hr_setmode(gd, m);
	hr_alloc = TRUE;
	bm->BytesPerRow = m->gm_Width * m->gm_Depth / 8;
	bm->Rows = m->gm_Height;
	bm->Depth = m->gm_Depth;
	bm->pad = GFXBM_MAGIC;
	bm->Planes[0] = hr_fbm->fbm_Half[0];
	hr_bm.gb_Half1 = hr_fbm->fbm_Half[1];
	hr_bm.gb_Driver = gd;
	hr_bm.gb_Flags = GBF_DISPLAY;
	hr_bm.gb_PlaneSize = hr_fbm->fbm_Size;
	hr_bm.gb_ModeID = m->gm_ModeID;
	bm->Flags = BMF_DISPLAYABLE;
	if (m->gm_Format == GDF_PLANAR)
		bm->Flags |= BMF_STANDARD;
	for (i = 0; i < 256; i++)
		hr_pixtab[i] = i & ((1 << bm->Depth) - 1);
	hr_bm.gb_PixTab = hr_pixtab;
	return bm;
}

static void hr_FreeDisplay(struct GfxDriver *gd, struct BitMap *bm)
{
	hr_alloc = FALSE;
	if (!hr_shown)
		FreeFB();
}

static BOOL hr_Show(struct GfxDriver *gd, struct BitMap *bm)
{
	if (bm) {
		hr_shown = TRUE;
		outb(FBHR_CTL, FBHR_FULL);
	} else if (hr_shown) {
		hr_shown = FALSE;
		if (hr_alloc)
			outb(FBHR_CTL, FBHR_BLANK);
		else
			FreeFB();
	}
	return TRUE;
}

static struct GfxDriver hr_driver = {
	{ NULL, NULL, 0, 0, "hr" },
	0,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	hr_modes, 3,
	NULL,
	hr_GetColor,
	hr_AllocDisplay,
	hr_FreeDisplay,
	hr_Show,
	NULL,
	NULL
};

struct GfxDriver *HR_5(struct HRBase *base, struct GfxDriver **fmts)
{
	struct FBInfo *fi;
	UWORD r, i, n;

	if (FrameBufBase == NULL)
		FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL)
		return NULL;
	fi = GetFBInfo();
	if (fi->fbi_Card != FBC_HR)
		return NULL;
	hr_fbm = &fi->fbi_Modes[fi->fbi_Mode];
	hr_fmts = fmts;
	r = hr_fbm->fbm_Format != FBF_MONO ? 2 : hr_fbm->fbm_Height > 800;
	hr_modes[0] = hr_jumper[r];
	for (i = 0, n = 1; i < 3; i++)
		if (i != r)
			hr_modes[n++] = hr_jumper[i];
	hr_setmode(&hr_driver, &hr_modes[0]);
	return &hr_driver;
}

static struct Library *hrinit(struct Library *lib, APTR seg, APTR sysbase)
{
	lib->lib_Node.ln_Name = hrname;
	lib->lib_IdString = (APTR)hrid;
	lib->lib_Version = HRVERSION;
	lib->lib_Revision = 1;
	((struct HRBase *)lib)->hb_Seg = seg;
	return lib;
}

struct Library *hropen(struct Library *lib, ULONG ver)
{
	lib->lib_OpenCnt++;
	return lib;
}

APTR hrclose(struct Library *lib)
{
	lib->lib_OpenCnt--;
	return NULL;
}

/* graphics.library keeps it open, and its bitmap may be on the display */
APTR hrexpunge(struct Library *lib)
{
	return NULL;
}

APTR hrnull(void)
{
	return NULL;
}

extern APTR HRFuncs[];

static struct {
	ULONG it_Size;
	APTR it_Funcs, it_Struct, it_Init;
} hrinittab = {
	sizeof(struct HRBase), (APTR)HRFuncs, NULL, (APTR)hrinit
};

struct Resident hrtag = {
	RTC_MATCHWORD, &hrtag, (APTR)(&hrtag + 1), RTF_AUTOINIT, HRVERSION,
	NT_LIBRARY, 0, hrname, hrid, (APTR)&hrinittab
};
