#include <intuition/extensions.h>
#include <intuition/windecorclass.h>
#include <intuition/scrdecorclass.h>

struct IntuitionCustomize
{
    struct Image *submenu;
    /* TODO: Move more decoration-specific data here */
};

BOOL int_LoadDecorator(const char *name, struct IntScreen *screen, struct IntuitionBase *IntuitionBase);
void int_UnloadDecorator(struct NewDecorator *tnd, struct IntuitionBase *IntuitionBase);
BOOL int_InitDecorator(struct Screen *screen);
void int_CalcSkinInfo(struct Screen *screen, struct IntuitionBase *IntuitionBase);

void int_ExitDecorator(struct Screen *screen);
