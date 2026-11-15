#ifndef _REQUESTERS_H_
#define _REQUESTERS_H_

/*
    Copyright (C) 2002-2023 The AROS Development Team, All Rights Reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <proto/exec.h>
#include <exec/types.h>
#include <aros/asmcall.h>
#include "intuition_intern.h"

#define OUTERSPACING_X      4
//horiz space between win border and requester's box
#define OUTERSPACING_Y      4
//horiz space between win border and requester's box
#define GADGETGADGETSPACING 8
//space between gadgets
#define GADGETGADGETSPACING_Y 8
//vert space  between gadgets
#define TEXTGADGETSPACING   4
//space between textbox and gadgets row
#define TEXTBOXBORDER_X     16
//min space between text and textboxborder
#define TEXTBOXBORDER_Y     4
#define BUTTONBORDER_X      8
//min space between gadaget text and gadget border
#define BUTTONBORDER_Y      4
#define LOGOTEXTSPACING_X   8

/* I'm too lazy to open the font and query it. Anyway this is hardcoded. */
#define TOPAZ_8_BASELINE 6

struct IntReqDims
{
    UWORD           width;          /* width of the requester */
    UWORD           height;         /* height of the requester */
    UWORD           offx;
    UWORD           offy;
    UWORD           fontheight;     /* height of the default font */
    UWORD           fontxheight;    /* extra height */
    UWORD           textleft;
    UWORD           textheight;     /* Height of text frame */
    UWORD           disptextheight; /* visible text frame */
    UWORD           gadgets;        /* number of gadgets */
    UWORD           gadgetwidth;    /* width of a gadget */
};

struct IntRequestUserData
{
    ULONG           	        IDCMP;
    STRPTR          	        * GadgetLabels;
    struct Gadget               * Gadgets;
    struct IntuiText            * ReqBody;
    APTR                        RawBody;
    struct IntReqDims           ReqDims;
    void                        (*drawreq)(struct IntReqDims *dims, struct IntuiText *itext, struct Window *req,
                                                    struct Screen *scr, struct Gadget *gadgets, struct IntuitionBase *IntuitionBase);
};

/**********************************************************************************************/


/* Miscellaneous prototypes */
void intrequest_freelabels(STRPTR *gadgetlabels, struct IntuitionBase *IntuitionBase);
void intrequest_freegadgets(struct Gadget *gadgets, struct IntuitionBase *IntuitionBase);
void render_requester(struct Requester *requester, struct IntuitionBase *IntuitionBase);
void buildreq_draw(struct IntReqDims *dims, struct IntuiText *itext,
                             struct Window *req, struct Screen *scr,
                             struct Gadget *gadgets,
                             struct IntuitionBase *IntuitionBase);

struct Window *buildsysreq_intern(struct Window *window, STRPTR reqtitle, struct IntuiText *bodytext,
				  struct IntuiText *postext, struct IntuiText *negtext,
				  ULONG IDCMPFlags, WORD width, WORD height, struct IntuitionBase *IntuitionBase);
LONG sysreqhandler_intern(struct Window *window, ULONG *IDCMPFlagsPtr, BOOL WaitInput, struct IntuitionBase *IntuitionBase);
void freesysreq_intern(struct Window *window, struct IntuitionBase *IntuitionBase);

struct RawInfo
{
    int Len;
    int Lines;
};

AROS_UFP2(void, RequesterCountChar,
           AROS_UFPA(UBYTE, chr, D0),
           AROS_UFPA(struct RawInfo *,RawInfo,A3)
          );

AROS_UFP2(void, RequesterPutChar,
           AROS_UFPA(UBYTE, chr, D0),
           AROS_UFPA(UBYTE **,buffer,A3)
          );

#endif /* _REQUESTERS_H_ */
