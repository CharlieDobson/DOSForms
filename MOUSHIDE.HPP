/*===========================================================================
 * MOUSHIDE.HPP  -  RAII guard: hide/restore mouse cursor across a draw scope
 * Target: MSVC 1.52c  DOS
 *===========================================================================*/

#ifndef MOUSEHIDE_HPP
#define MOUSEHIDE_HPP

class MSMouse;                  // see msmouse.hpp

// Scope guard. Construct one at the top of any routine that writes to the
// screen, passing the active MSMouse (or null if there is no mouse); the
// cursor is hidden while the guard is alive and restored when it goes out
// of scope, on every return path.
//
// A shared depth counter collapses nested guards into a single Hide/Show
// pair. This matters because MSMouse::Show/Hide track one visible flag and
// no-op on redundant calls: without the depth count, an inner guard's
// restore would re-show the cursor while an outer draw is still running.
class MouseHide
{
    MSMouse *mouse;
    static int depth;

    // non-copyable: a guard owns a hide/show pair
    MouseHide( const MouseHide & );
    MouseHide &operator=( const MouseHide & );

public:
    MouseHide( MSMouse *mouse );
    ~MouseHide( void );
};

#endif // MOUSEHIDE_HPP
