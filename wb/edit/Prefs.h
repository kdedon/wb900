/**************************************************************
**** Prefs.h: internal representation of preferences.      ****
**** Free software under GNU license, started on 16.2.2000 ****
**** © T.Pierron, C.Guillaume.                             ****
**************************************************************/

#ifndef PREFS_H
#define PREFS_H

#ifndef	GUI_H
#include "Gui.h"
#endif

#include <graphics/text.h>
#include "Version.h"

/*** Global preferences ***/
typedef struct prefs
{
	char   auto_indent;        /* TRUE if auto indent mode */
	char   xtend;              /* TRUE if use PC-like numeric keypad */
	char   matchcase;          /* TRUE if search is case insensitive */
	char   wholewords;         /* TRUE if only whole words match in a search */
	char   tabsize;            /* Tabulation size */
	STRPTR wordssep;           /* List of characters used to separate words */
	struct TextAttr attrtxt;   /* Font used to edit text */
	struct TextAttr attrscr;   /* Font used for graphical interface */
	WORD   left,top;           /* Left corner of the window */
	WORD   width,height;       /* Dimensions */
	struct pens pen;           /* Pens offset used */

	/* Information allocated at run-time */
	struct TextFont *txtfont;  /* Font usable with rastport */
	struct TextFont *scrfont;  /* Font used to render gui */
	struct Screen   *parent;   /* Screen where window remains */
}	PREFS;

/** Character types to split words **/
#define	ALPHA          0
#define	SEPARATOR      1
#define	SPACE          2
#define	MAX_SPLIT      40    /* Maximal string separator length */

/** Table where character type can be found **/
extern UBYTE TypeChar[256];

extern PREFS prefs;

/** Set preference to default settings **/
void set_default_prefs( PREFS *, struct Screen * );

#endif /* PREFS_H */
