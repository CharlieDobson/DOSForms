/*===========================================================================
 * MOUSEHIDE.CPP  -  RAII guard: hide/restore mouse cursor across a draw scope
 * Target: MSVC 1.52c  DOS
 *===========================================================================*/
// MouseHide guard class implementation

#include "moushide.hpp"
#include "msmouse.hpp"          // MSMouse::Hide / Show
#include "screen.hpp"           // DfDrawEnter / DfDrawLeave

int MouseHide::depth = 0;

MouseHide::MouseHide( MSMouse *mouse ) : mouse( mouse )
{
    // only the outermost guard with a mouse touches the driver
    if ( mouse && depth++ == 0 ) mouse->Hide();
    // ...but EVERY guard marks a draw in progress, mouse or no mouse.  A
    // deferring renderer flushes when the outermost one ends, and a program
    // built without USE_MOUSE would otherwise never reach a depth of zero to
    // flush from.  Counted separately from 'depth' for exactly that reason:
    // the two counts are not the same count.
    DfDrawEnter();
}

MouseHide::~MouseHide( void )
{
    // Flush BEFORE the cursor comes back.  The INT 33h driver saves the
    // pixels under its cursor when it draws one and puts them back when it
    // moves; showing it first would have it save a screen that has not been
    // painted yet, and the save would then be written back OVER the paint.
    DfDrawLeave();
    if ( mouse && --depth == 0 ) mouse->Show();
}
