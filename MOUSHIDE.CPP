/*===========================================================================
 * MOUSEHIDE.CPP  -  RAII guard: hide/restore mouse cursor across a draw scope
 * Target: MSVC 1.52c  DOS
 *===========================================================================*/
// MouseHide guard class implementation

#include "moushide.hpp"
#include "msmouse.hpp"          // MSMouse::Hide / Show

int MouseHide::depth = 0;

MouseHide::MouseHide( MSMouse *mouse ) : mouse( mouse )
{
    // only the outermost guard with a mouse touches the driver
    if ( mouse && depth++ == 0 ) mouse->Hide();
}

MouseHide::~MouseHide( void )
{
    if ( mouse && --depth == 0 ) mouse->Show();
}
