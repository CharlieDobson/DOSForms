/*===========================================================================
 * VGAGFX.HPP  -  A VGA graphics renderer for DOSForms
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS - 16-bit real mode (large
 *         model) and 32-bit protected mode (DOS/4GW, DOS/32A)
 *
 * The same character grid the toolkit has always drawn, put up in VGA mode
 * 12h (640x480, 16 colours) with the card's own 8x16 font instead of its
 * text mode - which is what DOSSHELL's graphics mode was.  Widgets do not
 * change and do not know: they go on calling DfTextOut with cells.
 *
 * The grid is 80x30, not 80x25, because 480 pixels divided by a 16-pixel
 * font is thirty rows.  Nothing may assume otherwise; ask DfRows().
 *
 * WHAT IT COSTS.  A text cell is one word written to 0xB8000.  A cell here
 * is sixty-four bytes across four bit planes, so a full-screen repaint is
 * about 150,000 writes - and the widgets repaint whole rows for a one-row
 * change.  The renderer therefore paints into a shadow grid and blits only
 * the cells that DIFFER from what is on the screen, which is what makes a
 * cursor key cost two rows instead of a screen.  See DfFlush in SCREEN.HPP
 * for when that happens; it is automatic.
 *
 * BOTH BITNESSES.  XArchive's DOS TUI uses it 32-bit; DiskStat and REVEAL
 * use it from 16-bit real mode, large model, where the frame buffer is a far
 * pointer to A000:0000 and the two shadow grids - 2,400 cells of four bytes
 * each, about 19K - come off the far heap.  It adds about 6K of near data
 * (the font and the icon cache) whether or not graphics is ever started, so
 * a program with no use for it should not link it.  Nothing here is
 * reachable unless a program calls DfGraphicsStart.
 *===========================================================================*/

#ifndef VGAGFX_HPP
#define VGAGFX_HPP

#include "structs.hpp"

/* Switch the card to 640x480x16 and make the graphics renderer the active
 * one.  Returns FALSE and changes NOTHING if the adapter is not a VGA, if
 * the card's font could not be read, or if the shadow grid would not fit -
 * the caller simply stays in text mode.
 *
 * Call this BEFORE constructing the mouse: an INT 33h driver samples the
 * video mode when it is reset and reports its coordinates in that mode's
 * units, so a driver initialised in text mode hands back text-mode pixels
 * for a screen that is no longer in one.  A program switching at RUN time
 * has to reset the driver itself - see MSMouse::Reset. */
boolean DfGraphicsStart( void );

/* Back to the text renderer and to 80x25.  Safe to call when graphics was
 * never started. */
void DfGraphicsStop( void );

/* Is the graphics renderer the active one?  (DfIsGraphics answers the same
 * question through the renderer; this one exists so a program can ask
 * without a screen.hpp include.) */
boolean DfGraphicsActive( void );

/* Why the last DfGraphicsStart failed, for a message the user can act on.
 * Points at a static string; never null. */
const char *DfGraphicsError( void );

#endif /* VGAGFX_HPP */
