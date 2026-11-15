/*
 * The calls of AROS's amiga.lib that lib/ has. They need no library
 * base: a method goes straight to the class's dispatcher hook.
 */
#ifndef PROTO_ALIB_H
#define PROTO_ALIB_H

#include <aros/types.h>
#include <intuition/classes.h>
#include <graphics/gfx.h>

#define CALLHOOKPKT(h, o, m)	((*(h)->h_Entry)((h), (o), (m)))

IPTR DoMethodA(Object *obj, Msg message);
IPTR DoMethod(Object *obj, IPTR MethodID, ...);
IPTR DoSuperMethodA(Class *cl, Object *obj, Msg message);
IPTR DoSuperMethod(Class *cl, Object *obj, IPTR MethodID, ...);
IPTR CoerceMethodA(Class *cl, Object *obj, Msg message);
IPTR CoerceMethod(Class *cl, Object *obj, IPTR MethodID, ...);

BOOL AndRectRect(struct Rectangle *rect1, struct Rectangle *rect2,
    struct Rectangle *intersect);
LONG TimeDelay(LONG Unit, ULONG Seconds, ULONG MicroSeconds);

#endif
