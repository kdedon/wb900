#ifndef MENUTASK_H
#define MENUTASK_H

/*
    Copyright(C) 1995-2018, The AROS Development Team. All rights reserved.
    Copyright(C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#define MENUTASK_NAME 	    	"Intuition menu handler"
/* All task stacks share one small pool */
#define MENUTASK_STACKSIZE  	3072
#define MENUTASK_PRIORITY   	10

#define ITEM_ITEM    	    	1
#define ITEM_SUBITEM 	    	2

#define AMIGAKEY_KEY_SPACING    4 /* GadTools assumes this, too */
#define AMIGAKEY_BORDER_SPACING 2

/* Structure passed to the DefaultMenuHandler task when it's initialized */

struct MenuTaskParams
{
    struct IntuitionBase    *intuitionBase;
    struct Task             *Caller;
    struct MsgPort          *MenuHandlerPort; /* filled in by MenuHandler task */
    BOOL                     success;
};

struct MenuHandlerData
{
    struct Window   	*win;
    struct Screen   	*scr;
    struct DrawInfo 	*dri;
    struct Window   	*menubarwin;
    struct Window   	*menuwin;
    struct Window   	*submenuwin;
    struct Menu     	*menu;
    struct Menu     	*activemenu;
    struct MenuItem 	*activeitem;
    struct MenuItem 	*activesubitem;
    struct Rectangle     submenubox;
    struct Image    	*checkmark;
    struct Image    	*amigakey;
    struct Image    	*submenuimage;
    ULONG                DecorUserBufferSize;
    IPTR                 DecorUserBuffer;
    IPTR                 SubDecorUserBuffer;
    IPTR		 BarDecorUserBuffer;
    WORD            	 menubarwidth;
    WORD            	 menubarheight;
    WORD            	 menubaritemwidth;
    WORD            	 menubaritemheight;
    WORD            	 nummenubaritems;
    WORD            	 activemenunum;
    WORD            	 activeitemnum;
    WORD            	 activesubitemnum;
    WORD            	 maxcommkeywidth_menu;
    WORD            	 maxcommkeywidth_submenu;
    WORD            	 scrmousex;
    WORD            	 scrmousey;
    WORD		 innerleft;
    WORD		 innertop;
    WORD		 innerright;
    WORD		 innerbottom;
    WORD		 menuinnerleft;
    WORD		 menuinnertop;
    WORD		 menuinnerright;
    WORD		 menuinnerbottom;
    WORD		 iteminnerleft;
    WORD		 iteminnertop;
    WORD		 iteminnerright;
    WORD		 iteminnerbottom;
    UWORD           	 firstmenupick;
    UWORD           	 lastmenupick;
    BOOL            	 active;
    BOOL            	 keepmenuup;
    BOOL                 TrueColor;
    ULONG           	 openseconds;
    ULONG           	 openmicros;
};

BOOL InitDefaultMenuHandler(struct IntuitionBase *IntuitionBase);
BOOL EnsureDefaultMenuHandler(struct IntuitionBase *IntuitionBase);
void DefaultMenuHandler(void);

#endif /* MENUTASK_H */
