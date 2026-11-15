/***************************************************************
**** prefs.c: Default preferences.                         ****
****          © T.Pierron, C.Guillaume                      ****
**** Free software under GNU license, started on 16/4/2000  ****
***************************************************************/

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <dos/dos.h>
#include "Jed.h"
#include "Utility.h"
#include "ProtoTypes.h"

PREFS prefs;

/** Special table for word separation **/
UBYTE WordsSep[MAX_SPLIT] = "!-/:-?[-]^`{-\277\327\367";
UBYTE SpaceType[]         = "\t\n\r \177";
UBYTE TypeChar[256];

/** Default pens number **/
struct pens DefaultPens = {
	BACKGROUNDPEN, TEXTPEN, FILLPEN, HIGHLIGHTTEXTPEN, SHINEPEN, TEXTPEN,
	SHINEPEN, SHADOWPEN, BACKGROUNDPEN, TEXTPEN, -4, BACKGROUNDPEN
};

/*** Convert a TextFont struct into a TextAttr ***/
static void text_to_attr(struct TextFont *src, struct TextAttr *dest)
{
	dest->ta_Name  = src->tf_Message.mn_Node.ln_Name;
	dest->ta_YSize = src->tf_YSize;
	dest->ta_Style = FS_NORMAL;
	dest->ta_Flags = src->tf_Flags;
}

/*** Unpack separators description string ***/
void unpack_separators(UBYTE *Fmt)
{
	UBYTE *src,a,b;
	memset(TypeChar,ALPHA,sizeof(TypeChar));
	for(src=Fmt; *src; src++)
	{
		/* Avoid special character meaning */
		if(*src == '\\' && src[1]) src++;
		TypeChar[*src] = SEPARATOR;
		/* Range of char specifier? */
		if(src[1] == '-' && *src<src[2])
			for(a=*src,b=src[2]; a<=b; a++)
				TypeChar[a] = SEPARATOR;
	}
	/* Force space char type */
	for(src=SpaceType; *src; src++)
		TypeChar[*src] = SPACE;
}

/*** Set preference to default settings ***/
void set_default_prefs( PREFS *prefs, struct Screen *def )
{
	memset(prefs, 0, sizeof(*prefs));
	CopyMem(&DefaultPens, &prefs->pen, sizeof(prefs->pen));
	prefs->auto_indent = 1;
	prefs->matchcase   = 0;
	prefs->tabsize = 8;
	prefs->scrfont = def->RastPort.Font;
	prefs->txtfont = GfxBase->DefaultFont;
	prefs->width   = prefs->txtfont->tf_XSize * 80;
	prefs->top     = def->BarHeight + 1;
	prefs->height  = def->Height - prefs->top;

	/* Convert dynamic struct to static one */
	text_to_attr(prefs->scrfont, &prefs->attrscr);
	text_to_attr(prefs->txtfont, &prefs->attrtxt);
	unpack_separators(prefs->wordssep = WordsSep);
	prefs->parent  = def;
	init_tabstop(8);
}

