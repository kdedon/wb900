/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/
#include "gadtools_intern.h"
#include <exec/memory.h>
#include <exec/types.h>

#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <utility/tagitem.h>


#include <proto/gadtools.h>

/*********************************************************************

    NAME */

        AROS_LH2(struct Menu *, CreateMenusA,

/*  SYNOPSIS */
        AROS_LHA(struct NewMenu *, newmenu, A0),
        AROS_LHA(struct TagItem *, tagList, A1),

/*  LOCATION */
        struct Library *, GadToolsBase, 8, GadTools)

/*  FUNCTION
        CreateMenusA() creates a complete menu or parts of a menu.

    INPUTS
        newmenu - pointer to struct NewMenu
        taglist - additional tags

    RESULT
        A pointer to a menu structure.

    NOTES
        CreateMenusA() stores no position information in the menu structure.
        You need to call LayoutMenusA() to retrieve them.
        The strings supplied for the menu are not copied into a private
        buffer. Therefore they must be preserved, until FreeMenus() was
        called.

    EXAMPLE

    BUGS

    SEE ALSO
        FreeMenus(), LayoutMenusA()

    INTERNALS

    HISTORY

***************************************************************************/
{
/*
 * We need to allocate the whole menu in one block as we can't just
 * go through the menustructure in freemenus as programs may do dynamic
 * menu links which would be illegally freeed that way.
 * As barlabels are dynamic Boopsi image objects we need to store them
 * in a menu pregap so we can free them through DisposeObject() before
 * we free the whole menublock.
 *
 * Menu Memory structure:
 *  void *BarLabelTable[BarLabelCount];
 *  ULONG BarLabelCount;
 *  Menu(returned Ptr):
 *   MenuStructure
 *   .
 *   .
 */


    AROS_LIBFUNC_INIT

    struct Menu         * firstmenu = NULL, * curmenu = NULL;
    struct MenuItem     * newitem, * firstitem = NULL, * curitem = NULL;
    struct MenuItem     * newsubitem, * firstsubitem = NULL, * cursubitem = NULL;
    struct TagItem      * ti;
    ULONG               menu_ctr = 0;
    ULONG               item_ctr = 0;
    ULONG               subitem_ctr = 0;
    BOOL                end;
    BOOL                fullmenu = GetTagData(GTMN_FullMenu, FALSE, tagList);
    UBYTE               prevtype = (UBYTE)-1;
    ULONG               BarLabels;
    ULONG               MenuMemSize;
    struct NewMenu      *menuentry;
    UBYTE               *MyMenuMemory;
    UBYTE               *MyMenuMemPtr;
    struct Image        **MyBarTablePtr;
    ULONG               err = 0;



    BarLabels   =       0;
    MenuMemSize =       0;
    menuentry   =       newmenu;

    end = FALSE;
    while (!end)
    {
        BOOL is_image = FALSE;
        switch (menuentry->nm_Type)
        {
            case NM_TITLE:
                MenuMemSize += getmenutitlesize(newmenu,tagList);
                break;

            case IM_ITEM:
                is_image = TRUE;
                /* Fall through */

            case NM_ITEM: /* also the BARLABEL */
                /*
                ** There has to be a menu structure available
                ** to create an item, unless GTMN_FullMenu is FALSE.
                */
                if (menuentry->nm_Label == NM_BARLABEL)
                {
                        BarLabels++;
                }
                MenuMemSize += getmenuitemsize(newmenu,
                                               is_image,
                                               tagList,
                                               GTB(GadToolsBase));

                break;

            case IM_SUB:
                is_image = TRUE;
                /* Fall through */

            case NM_SUB:
                /*
                ** There has to be an item menu structure available
                ** to create a sub item, unless GTMN_FullMenu == FALSE.
                */
                if (menuentry->nm_Label == NM_BARLABEL)
                {
                        BarLabels++;
                }
                MenuMemSize += getmenuitemsize(newmenu,
                                               is_image,
                                               tagList,
                                               GTB(GadToolsBase));
                break;

            case NM_IGNORE:
                /*
                ** Nothing to do in this case
                */
                break;

            case NM_END:
                /*
                ** The end.
                */
                end = TRUE;
                break;

        } /* switch (menuentry->nm_Type) */
        menuentry++;
    } /* while (!end) */

    /*
     * Add space for barlabel count+area which must come at the beginning.
     */
    MenuMemSize += sizeof(ULONG) + (BarLabels * sizeof(void*));


    if (!(MyMenuMemory=AllocVec(MenuMemSize,MEMF_ANY | MEMF_CLEAR)))
    {
        ULONG   *Ptr;
        /*
        ** Set the secondary error value if requested.
        */
        if ((Ptr =(ULONG*) GetTagData(GTMN_SecondaryError, (IPTR) NULL, tagList)))
        {
            *Ptr = GTMENU_NOMEM;
        }
        return(NULL);
    }
    MyBarTablePtr =(struct Image**) MyMenuMemory;
    MyMenuMemPtr =(UBYTE*) &MyBarTablePtr[BarLabels];
    *((ULONG*) MyMenuMemPtr) = BarLabels;
    MyMenuMemPtr += sizeof(ULONG);


    end = FALSE;
    while (!end)
    {
        BOOL is_image = FALSE;


        switch (newmenu->nm_Type)
        {
            case NM_TITLE:
                curmenu = makemenutitle(newmenu,
                                        &MyMenuMemPtr,
                                        tagList);

                if (NULL == curmenu)
                {
                    err = GTMENU_NOMEM;
                    goto failexit;
                }

                /*
                ** Append it to the list of menus or make it the
                ** first entry.
                */
                if (NULL == firstmenu)
                    firstmenu = curmenu;
                else
                    appendmenu(firstmenu, curmenu);

                menu_ctr ++;
                item_ctr = 0;
                curitem = firstitem = cursubitem = firstsubitem = NULL;
                break;

            case IM_ITEM:
                is_image = TRUE;
                /* Fall through */

            case NM_ITEM: /* also the BARLABEL */
                /*
                ** There has to be a menu structure available
                ** to create an item, unless GTMN_FullMenu is FALSE.
                */

                if (fullmenu && (NULL == curmenu))
                {
                    err = GTMENU_INVALID;
                    goto failexit;
                }

                if (item_ctr >= 64)
                {
                    /* CreateMenusA must still return success here, only that the
                       menus will be trimmed */
                    err = GTMENU_TRIMMED;
                    break;
                }

                newitem = makemenuitem(newmenu,
                                       &MyMenuMemPtr,
                                       &MyBarTablePtr,
                                       is_image,
                                       tagList,
                                       GTB(GadToolsBase));

                if (NULL == newitem)
                {
                    err = GTMENU_NOMEM;
                    goto failexit;
                }

                /*
                ** Append this item to the current menu title
                */

                if (curitem)
                {
                    curitem->NextItem = newitem;
                }
                else
                {
                    firstitem = newitem;
                    if (curmenu)
                    {
                        curmenu->FirstItem = newitem;
                    }
                }


                item_ctr ++;
                subitem_ctr = 0;
                cursubitem = firstsubitem = NULL;

                curitem = newitem;
                break;

            case IM_SUB:
                is_image = TRUE;
                /* Fall through */

            case NM_SUB:
                /*
                ** There has to be an item menu structure available
                ** to create a sub item, unless GTMN_FullMenu == FALSE.
                */
                if ( (fullmenu && (NULL == curitem)) ||
                     (prevtype == NM_TITLE) )
                {
                    err = GTMENU_INVALID;
                    goto failexit;
                }

                if (subitem_ctr >= 32)
                {
                    err = GTMENU_TRIMMED;
                    /* CreateMenusA must still return success here, only that the
                       menus will be trimmed */
                    break;
                }

                newsubitem = makemenuitem(newmenu,
                                          &MyMenuMemPtr,
                                          &MyBarTablePtr,
                                          is_image,
                                          tagList,
                                          GTB(GadToolsBase));

                if (NULL == newsubitem)
                {
                    err = GTMENU_NOMEM;
                    goto failexit;
                }

                /*
                ** Append this item to the current sub menu item
                */

                if (cursubitem)
                {
                    cursubitem->NextItem = newsubitem;
                }
                else
                {
                    firstsubitem = newsubitem;
                    if (curitem)
                    {
                        curitem->SubItem = newsubitem;


                        /* Add the ">>" mark. Hmm ... maybe if would be better if this
                           was done in LayoutMenus() ??? */

                        curitem->Flags &= ~COMMSEQ;
                        if (curitem->Flags & ITEMTEXT)
                        {
                            struct IntuiText *it = (struct IntuiText *)curitem->ItemFill;

                            if (!it->NextText)
                            {
                                struct IntuiText *it2 = it + 1;

                                it->NextText = it2;
                                it2->IText = "\xBB";  /* right guillemet */
                            }
                        }

                    }
                }

                cursubitem = newsubitem;
                subitem_ctr ++;
                break;

            case NM_IGNORE:
                /*
                ** Nothing to do in this case
                */
                break;

            case NM_END:
                /*
                ** The end.
                */
                end = TRUE;
                break;

        } /* switch (newmenu->nm_Type) */


        prevtype = newmenu->nm_Type;

        /*
        ** Advance to the next item in the array
        */
        newmenu = &newmenu[1];

    } /* while (FALSE == end) */

    /*
    ** Set the secondary error value if requested.
    */
    ti = FindTagItem(GTMN_SecondaryError, tagList);

    if (NULL != ti)
        ti->ti_Data = err;

    if (!firstmenu)
    {
        firstmenu = (struct Menu *)firstitem;
    }
    if (!firstmenu)
    {
        firstmenu = (struct Menu *)firstsubitem;
    }



    return firstmenu;


failexit:


    /*
    ** Free all memory
    */
    FreeMenus(firstmenu);

    /*
    ** Set the secondary error value if requested.
    */
    ti = FindTagItem(GTMN_SecondaryError, tagList);

    if (NULL != ti)
        ti->ti_Data = err;

    return NULL;

    AROS_LIBFUNC_EXIT

} /* CreateMenusA */


