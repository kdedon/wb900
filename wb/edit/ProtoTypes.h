/*** ProtoTypes.h : Load standard Amiga prototypes declaration
**** Written by T.Pierron 27/5/2000
***/
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/gadtools.h>
#include <proto/keymap.h>
#include <proto/asl.h>

/** The C library functions the editor uses (Utility.c) **/
ULONG  strlen  (CONST_STRPTR);
STRPTR strcpy  (STRPTR, CONST_STRPTR);
STRPTR strcat  (STRPTR, CONST_STRPTR);
int    strncmp (CONST_STRPTR, CONST_STRPTR, ULONG);
APTR   memset  (APTR, int, ULONG);

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase       *GfxBase;
extern struct Library       *GadToolsBase, *KeymapBase, *UtilityBase, *AslBase;
