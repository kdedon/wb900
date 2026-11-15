/*
    Copyright (C) 2002-2026, The AROS Development Team. All rights reserved.
    Copyright (C) 2001-2003, The MorphOS Development Team. All Rights Reserved.
*/

#include <exec/memory.h>
#include <graphics/rastport.h>
#include <graphics/sprite.h>
#include <intuition/pointerclass.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include "intuition_intern.h"
#include "inputhandler.h"
#include "inputhandler_support.h"

/*
 * The pointer is graphics.library's software sprite, drawn into the
 * frontmost screen's bitmap at the screen's pointer.
 */
void SetPointerPos(struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Screen *scr = IntuitionBase->FirstScreen;
    struct SharedPointer *p;

    if (scr && (p = GetPrivScreen(scr)->Pointer))
        MoveSprite(&scr->ViewPort, &p->sprite->es_SimpleSprite,
            IntuitionBase->MouseX - scr->LeftEdge + p->xoffset,
            IntuitionBase->MouseY - scr->TopEdge + p->yoffset);
}

/* Put the pointer on the frontmost screen, or take it away */
void ShowPointer(struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct Screen *scr = IntuitionBase->FirstScreen;
    struct SharedPointer *p = scr ? GetPrivScreen(scr)->Pointer : NULL;

    ChangeExtSpriteA(scr ? &scr->ViewPort : NULL, NULL, p ? p->sprite : NULL, NULL);
    SetPointerPos(IntuitionBase);
}

/* Show the image of a pointer object on a screen. If that works the
   screen holds a reference to the shared pointer from then on and the
   window, if one is given, takes over its hot spot. */
void SetScreenPointer(struct IntScreen *scr, struct Window *window, Object *pointer,
                      struct IntuitionBase *IntuitionBase)
{
    struct GfxBase *GfxBase = GetPrivIBase(IntuitionBase)->GfxBase;
    struct SharedPointer *sp = NULL, *old = scr->Pointer;

    GetAttr(POINTERA_SharedPointer, pointer, (IPTR *)&sp);
    if (!sp || sp == old)
        return;

    if (&scr->Screen == IntuitionBase->FirstScreen &&
        !ChangeExtSpriteA(&scr->Screen.ViewPort, old ? old->sprite : NULL, sp->sprite, NULL))
        return;

    ObtainSharedPointer(sp, IntuitionBase);
    if (old)
        ReleaseSharedPointer(old, IntuitionBase);
    scr->Pointer = sp;
    if (window)
    {
        window->XOffset = sp->xoffset;
        window->YOffset = sp->yoffset;
    }
    if (&scr->Screen == IntuitionBase->FirstScreen)
        SetPointerPos(IntuitionBase);
}

struct RastPort *MyCreateRastPort(struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct RastPort *newrp = AllocMem(sizeof(*newrp), MEMF_PUBLIC);

    if (newrp)
    {
        InitRastPort(newrp);
    }

    return newrp;
}

struct RastPort *MyCloneRastPort(struct IntuitionBase *IntuitionBase, struct RastPort *rp)
{
    struct RastPort *newrp = NULL;

    if (rp)
    {
        newrp = AllocMem(sizeof(*newrp), MEMF_PUBLIC);
        if (newrp)
        {
            CopyMem(rp, newrp, sizeof(struct RastPort));
            /* graphics' private data stays with the original */
            newrp->RP_Extra = NULL;
        }
    }

    return newrp;
}

void MyFreeRastPort(struct IntuitionBase *IntuitionBase, struct RastPort *rp)
{
    /*
     * graphics' private data, when it belongs to this RastPort (its first
     * field is the RastPort): a copy of another RastPort shares the pointer.
     */
    if (rp->RP_Extra && (*(struct RastPort **)rp->RP_Extra == rp))
        FreeVec(rp->RP_Extra);

    FreeMem(rp, sizeof(*rp));
}

struct TextFont *SafeReopenFont(struct IntuitionBase *IntuitionBase,
                                struct TextFont **fontptr)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct TextFont *ret = NULL, *font;
    struct SignalSemaphore *fontsem;

    /*
     * Pin the font under the semaphore graphics.library itself uses for
     * the font list and tf_Accessors (OpenFont/CloseFont/RemFont), so it
     * cannot go away under us. Forbid() alone only stops this CPU.
     */
    if (!_intuitionBase->GfxFontSem)
    {
        Forbid();
        _intuitionBase->GfxFontSem = FindSemaphore("graphics.library/TextFonts");
        Permit();
    }
    fontsem = _intuitionBase->GfxFontSem;

    if (fontsem)
        ObtainSemaphore(fontsem);
    else
        Forbid();

    font = *fontptr;
    if (font)
    {
        struct TextAttr ta;

        font->tf_Accessors++;
        if (fontsem)
            ReleaseSemaphore(fontsem);
        else
            Permit();

        /* Now really open it
         */
        ta.ta_Name  = font->tf_Message.mn_Node.ln_Name;
        ta.ta_YSize = font->tf_YSize;
        ta.ta_Style = font->tf_Style;
        ta.ta_Flags = font->tf_Flags;

        ret = OpenFont(&ta);

        /* Unlock it
         */
        if (fontsem)
            ObtainSemaphore(fontsem);
        else
            Forbid();
        font->tf_Accessors--;
    }

    if (fontsem)
        ReleaseSemaphore(fontsem);
    else
        Permit();

    return ret;
}

Object *MakePointerFromData(struct IntuitionBase *IntuitionBase,
    const UWORD *source, int xOffset, int yOffset, int width, int height)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct TagItem pointertags[] = {
        {POINTERA_BitMap      , (IPTR)source},
        {POINTERA_XOffset     , xOffset      },
        {POINTERA_YOffset     , yOffset      },
        {SPRITEA_OldDataFormat, TRUE         },
        {SPRITEA_Width        , width        },
        {SPRITEA_OutputHeight , height       },
        {TAG_DONE                            }
    };

    return NewObjectA(_intuitionBase->pointerclass, NULL, pointertags);
}

Object *MakePointerFromPrefs(struct IntuitionBase *IntuitionBase, struct Preferences *prefs)
{
    SetPointerColors(IntuitionBase);

    return MakePointerFromData(IntuitionBase, prefs->PointerMatrix, prefs->XOffset, prefs->YOffset, 16, 16);
}

void InstallPointer(struct IntuitionBase *IntuitionBase, Object **old, Object *pointer)
{
    struct IntScreen    *scr;
    struct Window       *win;
    struct SharedPointer *oldpointer, *newpointer = NULL;
    Object *oldobject;
    ULONG lock = LockIBase(0);

    GetAttr(POINTERA_SharedPointer, *old, (IPTR *)&oldpointer);
    GetAttr(POINTERA_SharedPointer, pointer, (IPTR *)&newpointer);

    for (scr = GetPrivScreen(IntuitionBase->FirstScreen); scr; scr = GetPrivScreen(scr->Screen.NextScreen))
    {
        for (win = scr->Screen.FirstWindow; win; win = win->NextWindow)
        {
            if (((struct IntWindow *)win)->pointer == *old)
            {
                win->XOffset = newpointer->xoffset;
                win->YOffset = newpointer->yoffset;
            }
        }

        if (scr->Pointer == oldpointer)
            SetScreenPointer(scr, NULL, pointer, IntuitionBase);
    }

    oldobject = *old;
    *old = pointer;
    /* Dispose old pointer only after setting new one */
    DisposeObject(oldobject);

    UnlockIBase(lock);
}

void SetPointerColors(struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    struct Color32 *p;
    int            k;
    ULONG          lock = LockIBase(0);
    /* Probably this should apply to Workbench screen and not to currently active one? */
    struct Screen *scr = IntuitionBase->ActiveScreen;

    p = _intuitionBase->Colors;

    if (scr)
    {
        if (GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) < 9)
        {
            UWORD firstcol = scr->ViewPort.ColorMap->SpriteBase_Even;

            /* Translate bank number and offset to color number - see graphics/getcolormap.c */
            firstcol = (firstcol << 4) | (firstcol >> 8);
            for (k = 1; k < 4; ++k) {

                SetRGB32(&scr->ViewPort, k + firstcol, p[k+7].red, p[k+7].green, p[k+7].blue);
            }
        }
    }

    UnlockIBase(lock);

}

struct SharedPointer *CreateSharedPointer(struct ExtSprite *sprite, int x, int y,
                    struct IntuitionBase *IntuitionBase)
{
    struct SharedPointer *pointer;

    pointer = AllocMem(sizeof(*pointer), MEMF_PUBLIC);
    if (pointer)
    {
        pointer->sprite = sprite;
        pointer->xoffset = x;
        pointer->yoffset = y;
        pointer->ref_count = 1;
    }

    return pointer;
}

void ObtainSharedPointer(struct SharedPointer *pointer,
                         struct IntuitionBase *IntuitionBase)
{
    ULONG lock = LockIBase(0);
    ++pointer->ref_count;
    UnlockIBase(lock);
}

void ReleaseSharedPointer(struct SharedPointer *pointer,
                          struct IntuitionBase *IntuitionBase)
{
    struct IntIntuitionBase *_intuitionBase = GetPrivIBase(IntuitionBase);
    struct GfxBase *GfxBase = _intuitionBase->GfxBase;
    ULONG lock = LockIBase(0);
    if (--pointer->ref_count == 0)
    {
        FreeSpriteData(pointer->sprite);
        FreeMem(pointer, sizeof(*pointer));
    }
    UnlockIBase(lock);
}

