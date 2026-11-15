/* SPDX-License-Identifier: MIT */
/*
 * hr.gfxdriver: the HR card's bitmap in VRAM as graphics.library's
 * display, in the mode framebuf.resource reports (1024x800 mono or
 * 640x400 RGBI).  It draws with graphics' planar or chunky 4-bit
 * operations; the palette is fixed, and pens take its nearest colour.  The card holds one display bitmap.
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
static struct GfxMode hr_mode;
static struct GfxBitMap hr_bm;
static UBYTE hr_pixtab[256];
static BOOL hr_alloc, hr_shown;

/* RGBI: a gun that is on is at 0xAA, or 0xFF with I */
static ULONG hr_GetColor(struct GfxDriver *gd, UWORD n)
{
	ULONG on, c;

	if (hr_fbm->fbm_Format == FBF_MONO)
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

	if (hr_alloc || m != &hr_mode)
		return NULL;
	if (!hr_shown) {
		if (AllocFB(hrowner) != NULL)
			return NULL;
		outb(FBHR_CTL, FBHR_BLANK);
	}
	hr_alloc = TRUE;
	bm->BytesPerRow = hr_fbm->fbm_BytesPerRow;
	bm->Rows = hr_fbm->fbm_Height;
	bm->Depth = hr_fbm->fbm_Depth;
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
	&hr_mode, 1,
	NULL,
	hr_GetColor,
	hr_AllocDisplay,
	hr_FreeDisplay,
	hr_Show
};

struct GfxDriver *HR_5(struct HRBase *base, struct GfxDriver **fmts)
{
	struct FBInfo *fi;
	struct GfxDriver *f;

	if (FrameBufBase == NULL)
		FrameBufBase = OpenResource(FRAMEBUFNAME);
	if (FrameBufBase == NULL)
		return NULL;
	fi = GetFBInfo();
	if (fi->fbi_Card != FBC_HR)
		return NULL;
	hr_fbm = &fi->fbi_Modes[fi->fbi_Mode];
	hr_mode.gm_Width = hr_fbm->fbm_Width;
	hr_mode.gm_Height = hr_fbm->fbm_Height;
	hr_mode.gm_Depth = hr_fbm->fbm_Depth;
	if (hr_fbm->fbm_Format == FBF_MONO) {
		hr_mode.gm_Format = GDF_PLANAR;
		hr_mode.gm_ModeID = hr_fbm->fbm_Height > 800 ?
		    HRMONO_1024_ID : HRMONO_800_ID;
	} else {
		hr_mode.gm_Format = GDF_CHUNKY4;
		hr_mode.gm_ModeID = HRCOLOR_ID;
	}
	f = fmts[hr_mode.gm_Format - 1];
	hr_driver.gd_Format = hr_mode.gm_Format;
	hr_driver.gd_Fill = f->gd_Fill;
	hr_driver.gd_Template = f->gd_Template;
	hr_driver.gd_Copy = f->gd_Copy;
	hr_driver.gd_ReadPixel = f->gd_ReadPixel;
	hr_driver.gd_WritePixel = f->gd_WritePixel;
	hr_driver.gd_Line = f->gd_Line;
	hr_driver.gd_ReadRow = f->gd_ReadRow;
	hr_driver.gd_WriteRow = f->gd_WriteRow;
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
