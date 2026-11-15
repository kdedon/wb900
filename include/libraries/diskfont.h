/*
 * The AvailFonts() records of diskfont.library, which asl also uses for
 * the fonts in memory.
 */
#ifndef LIBRARIES_DISKFONT_H
#define LIBRARIES_DISKFONT_H

#include <exec/types.h>
#include <graphics/text.h>

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
