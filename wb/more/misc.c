/*
    Copyright (C) 1995-2019, The AROS Development Team. All rights reserved.
*/

/*********************************************************************************************/

#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/asl.h>

#include "global.h"
#include "version.h"

const char *versionstring = VERSIONSTR;

/*********************************************************************************************/

struct Library *AslBase;

/*********************************************************************************************/

static struct NewMenu nm[] =
{
    {NM_TITLE, "Project",           NULL, 0, 0, NULL                   },
     {NM_ITEM, "Open...",           "O",  0, 0, (APTR)MEN_OPEN         },
     {NM_ITEM, "Edit",              "E",  0, 0, (APTR)MEN_EDIT         },
     {NM_ITEM, NM_BARLABEL,         NULL, 0, 0, NULL                   },
     {NM_ITEM, "About...",          "?",  0, 0, (APTR)MEN_ABOUT        },
     {NM_ITEM, NM_BARLABEL,         NULL, 0, 0, NULL                   },
     {NM_ITEM, "Quit",              "Q",  0, 0, (APTR)MEN_QUIT         },
    {NM_TITLE, "Navigation",        NULL, 0, 0, NULL                   },
     {NM_ITEM, "Find...",           "F",  0, 0, (APTR)MEN_FIND         },
     {NM_ITEM, "Find Next",         "N",  0, 0, (APTR)MEN_FIND_NEXT    },
     {NM_ITEM, "Find Previous",     "P",  0, 0, (APTR)MEN_FIND_PREV    },
     {NM_ITEM, NM_BARLABEL,         NULL, 0, 0, NULL                   },
     {NM_ITEM, "Jump To Line...",   "J",  0, 0, (APTR)MEN_JUMP         },
    {NM_END,   NULL,                NULL, 0, 0, NULL                   }
};

/*********************************************************************************************/

BOOL MakeMenus(void)
{
    menus = CreateMenusA(nm, NULL);
    if (!menus) return Fail(MSG(MSG_CANT_CREATE_MENUS));

    if (!LayoutMenusA(menus, vi, NULL)) return Fail(MSG(MSG_CANT_CREATE_MENUS));

    return TRUE;
}

/*********************************************************************************************/

void KillMenus(void)
{
    if (win) ClearMenuStrip(win);
    if (menus) FreeMenus(menus);

    menus = NULL;
}

/*********************************************************************************************/

STRPTR GetFile(void)
{
    static UBYTE         pathbuffer[300];
    static UBYTE         filebuffer[108];
    struct FileRequester *req;
    struct TagItem       t[7];
    STRPTR               retval = NULL;

    AslBase = OpenLibrary("asl.library", 39);
    if (AslBase)
    {
        filebuffer[sizeof(filebuffer) - 1] = 0;
        pathbuffer[sizeof(pathbuffer) - 1] = 0;

        strncpy(filebuffer, FilePart(filenamebuffer), sizeof(filebuffer) - 1);
        CopyMem(filenamebuffer, pathbuffer, 300);
        *(FilePart(pathbuffer)) = 0;

        t[0].ti_Tag = ASLFR_TitleText;      t[0].ti_Data = (IPTR)MSG(MSG_OPEN_TITLE);
        t[1].ti_Tag = ASLFR_RejectIcons;    t[1].ti_Data = TRUE;
        t[2].ti_Tag = ASLFR_DoPatterns;     t[2].ti_Data = TRUE;
        t[3].ti_Tag = ASLFR_InitialDrawer;  t[3].ti_Data = (IPTR)pathbuffer;
        t[4].ti_Tag = ASLFR_InitialFile;    t[4].ti_Data = (IPTR)filebuffer;
        t[5].ti_Tag = ASLFR_Screen;         t[5].ti_Data = (IPTR)scr;
        t[6].ti_Tag = TAG_DONE;
        req = AllocAslRequest(ASL_FileRequest, t);
        if (req)
        {
            if (AslRequest(req, NULL))
            {
                strncpy(pathbuffer, req->fr_Drawer, sizeof pathbuffer - 1);
                AddPart(pathbuffer, req->fr_File, sizeof pathbuffer - 1);

                retval = pathbuffer;

            } /* if (AslRequest(req, NULL) */

            FreeAslRequest(req);

        } /* if (req) */

        CloseLibrary(AslBase);

    } /* if (AslBase) */

    return retval;
}

/*********************************************************************************************/

void About(void)
{
    struct EasyStruct   es;
    IPTR                args[5];

    es.es_StructSize   = sizeof(es);
    es.es_Flags        = 0;
    es.es_Title        = MSG(MSG_ABOUT_TITLE);
    es.es_TextFormat   = MSG(MSG_ABOUT);
    es.es_GadgetFormat = MSG(MSG_CONTINUE);

    args[0] = VERSION;
    args[1] = REVISION;
    args[2] = (IPTR)DATESTR;
    args[3] = (IPTR)"2005-2009";
    args[4] = (IPTR)"AROS Development Team";

    EasyRequestArgs(win, &es, NULL, args);

}

/*********************************************************************************************/
