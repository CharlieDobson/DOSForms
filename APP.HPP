/*===========================================================================
 * APP.HPP  -  App: composite top-level container (desktop backdrop + widgets)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// class definition file for App

#ifndef APP_HPP
#define APP_HPP

#include "desktop.hpp"
#include "structs.hpp"

// App is the whole screen: it IS-A Desktop (the backdrop) and HAS-A set of
// child forms drawn on top of it. One App owns the composition so the calling
// program builds a screen by adding widgets, not by wiring each one by hand.
class App : public Desktop
{
    enum { MAX_CHILDREN = 32 };

    DOSForms *children[MAX_CHILDREN];   // borrowed; App does not own/free them
    short child_count;
    boolean running;                    // Run() loop control
    int last_buttons;                   // previous mouse button state (edge detect)
    // Bare-Alt tracking (see PollEvent).  alt_down is the last state of the
    // BIOS Alt flag; alt_clean says no other key has been read since it went
    // down, which is what separates "Alt" from "Alt+F".
    boolean alt_down;
    boolean alt_clean;

    App( const App & );
    App &operator=( const App & );

    boolean PollEvent( Event &e );      // gather one keyboard/mouse event, or FALSE

public:
    App( void );
    App( const foreground_color_t foreground, const background_color_t background );
    virtual ~App( void );                 // deregisters the modal resync hook

    // methods
    boolean Add( DOSForms *child );       // register a child; draw order == add order (z-order)
    virtual void SetMouse( MSMouse *m );  // fan the mouse out to the backdrop and every child
    virtual void Draw( void );            // paint the backdrop, then the children in order
    virtual boolean HandleEvent( const Event &e );  // offer the event to children, topmost first
    void Run( void );                     // event loop: poll input + dispatch until Quit()/Esc
    void Quit( void );                    // stop the Run() loop
    // Re-read the mouse buttons into the edge detector.  Call this after a
    // MODAL dialog that ran its own mouse loop: the press/release edges that
    // happened inside it never reached PollEvent, so without a resync the next
    // poll can synthesize a phantom click from the leftover state.
    void SyncMouseButtons( void );
};

#endif
