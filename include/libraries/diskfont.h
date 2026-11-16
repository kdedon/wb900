/* diskfont.library: font contents files, font files and AvailFonts() */
#ifndef LIBRARIES_DISKFONT_H
#define LIBRARIES_DISKFONT_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <dos/dos.h>
#include <graphics/text.h>

#define MAXFONTPATH	256
#define MAXFONTNAME	32

/* FONTS:NAME.font: a FontContentsHeader, then its entries */
struct FontContentsHeader
{
    UWORD		fch_FileID;
    UWORD		fch_NumEntries;
};

#define FCH_ID		0x0f00
#define TFCH_ID		0x0f02
#define OFCH_ID		0x0f03

struct FontContents
{
    char		fc_FileName[MAXFONTPATH];
    UWORD		fc_YSize;
    UBYTE		fc_Style;
    UBYTE		fc_Flags;
};

/* A tagged entry: its tags end at tfc_TagCount */
struct TFontContents
{
    char		tfc_FileName[MAXFONTPATH - 2];
    UWORD		tfc_TagCount;
    UWORD		tfc_YSize;
    UBYTE		tfc_Style;
    UBYTE		tfc_Flags;
};

/* A font file's first hunk, after its first long */
struct DiskFontHeader
{
    struct Node		dfh_DF;
    UWORD		dfh_FileID;
    UWORD		dfh_Revision;
    BPTR		dfh_Segment;
    char		dfh_Name[MAXFONTNAME];
    struct TextFont	dfh_TF;
};

#define dfh_TagList	dfh_Segment
#define DFH_ID		0x0f80

#define AFB_MEMORY	0
#define AFF_MEMORY	(1L << AFB_MEMORY)
#define AFB_DISK	1
#define AFF_DISK	(1L << AFB_DISK)
#define AFB_SCALED	2
#define AFF_SCALED	(1L << AFB_SCALED)
#define AFB_BITMAP	3
#define AFF_BITMAP	(1L << AFB_BITMAP)
#define AFB_TAGGED	16
#define AFF_TAGGED	(1L << AFB_TAGGED)

struct AvailFonts
{
    UWORD		af_Type;
    struct TextAttr	af_Attr;
};

struct TAvailFonts
{
    UWORD		taf_Type;
    struct TTextAttr	taf_Attr;
};

struct AvailFontsHeader
{
    UWORD		afh_NumEntries;
};

#endif
