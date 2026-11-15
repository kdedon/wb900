#include <intuition/preferences.h>

/*
** The intuition default preferences structure
*/

CONST struct Preferences IntuitionDefaultPreferences =
{
    8,      // FontHeight
    0,      // PrinterPort
    5,      // BaudRate

    {
	{ 0 },{ 40000 }
    }
    ,   // timeval KeyRptSpeed

    {
	{ 0 },{ 500000 }
    }
    ,   // timeval KeyRptDelay

    {
	{ 0 },{ 500000 }
    }
    ,   // timeval DoubleClick

    /*
     * The pointer: plane 0 and plane 1 words of each row, a white arrow
     * (colour 3) with a black rim (colour 2) all round, tip at 0,0
     */
    {
        0,12,
        0x0000, 0x8000,
        0x0000, 0xc000,
        0x4000, 0xe000,
        0x6000, 0xf000,
        0x7000, 0xf800,
        0x7800, 0xfc00,
        0x7c00, 0xfe00,
        0x7e00, 0xff00,
        0x7f00, 0xff80,
        0x7f80, 0xffc0,
        0x7c00, 0xffc0,
        0x6c00, 0xfe00,
        0x4600, 0xef00,
        0x0600, 0xcf00,
        0x0300, 0x0780,
        0x0000, 0x0300,
        0,0
    }
    ,   // PointerMatrix (36 entries)
    0,      // XOffset
    0,      // YOffset
    0x0e44, // color17
    0x0000, // color18
    0x0eec, // color19
    1,      // PointerTicks

    0x0fff, // color0-4
    0x0000,
    0x0aaa,
    0x000a,

    0,      // ViewXOffset
    0,      // ViewYOffset
    0,      // ViewInitX
    0,      // ViewINitY

    (BOOL)MOUSE_ACCEL,      // EnableCLI

    0,      // PointerType
    {
        'g','e','n','e','r','i','c',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
    ,// PrinterFilename (30 chars)

    0,      // PrintPitch
    0,      // PrintQuality
    0,      // PrintSpacing
    5,      // PrintLeftMargin
    75,     // PrintRightMargin
    0,      // PrintImage
    0,      // PrintAspect
    1,      // PrintShade
    2,      // PrintTreshold

    32,     // PaperSize
    66,     // PaperLength
    0,      // PaperType

    0,      // SerRWBits
    0,      // SerStopBuf
    0,      // SerParShk

    0,      // LaceWb

    {
        0,0,0,0,0,0,0,0,0,0,0,0
    }
    ,       // pad (12 bytes)
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    }
    ,   // PrtDevName (16 chars)

    0,      // DefaultPrtUnit
    0,      // DefaultSerUnit

    0,      // RowSizeChange
    0,      // ColumnSizeChange

    0,      // PrintFlags
    0,      // PrintMaxWidth
    0,      // PrintMaxHeight
    0,      // PrintDensity
    0,      // PrintXOffset

    AROS_NOMINAL_WIDTH,  // wb_Width
    AROS_NOMINAL_HEIGHT, // wb_Height
    AROS_NOMINAL_DEPTH,  // wb_Depth

    0       // ext size
};
