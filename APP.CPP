/*===========================================================================
 * APP.CPP  -  App: composite top-level container implementation
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// App class implementation file

#include <conio.h>              // kbhit, getch
#include "app.hpp"
#include "msmouse.hpp"          // MSMouse Status/X/Y for the event loop
#include "moushide.hpp"         // MouseHide cursor guard

App::App( void ) : Desktop()
{
    child_count = 0;
    running = FALSE;
    last_buttons = 0;
}

App::App( const foreground_color_t foreground, const background_color_t background )
    : Desktop( foreground, background )
{
    child_count = 0;
    running = FALSE;
    last_buttons = 0;
}

boolean App::Add( DOSForms *child )
{
    if ( !child || child_count >= MAX_CHILDREN ) return FALSE;
    children[child_count++] = child;
    return TRUE;
}

void App::SetMouse( MSMouse *m )
{
    DOSForms::SetMouse( m );     // the backdrop's own mouse (and our cursor-hide context)
    for ( short i = 0; i < child_count; ++i ) {
        children[i]->SetMouse( m );   // virtual: a nested container fans out in turn
    }
}

void App::Draw( void )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );    // cursor hidden across the entire composite paint

    ClearScreen();               // paint the desktop backdrop in App's colors
    for ( short i = 0; i < child_count; ++i ) {
        children[i]->Draw();     // each child renders itself on top, in add order
    }
}

boolean App::HandleEvent( const Event &e )
{
    // offer the event to children topmost-first; first to consume it wins
    for ( short i = child_count - 1; i >= 0; --i ) {
        if ( children[i]->HandleEvent( e ) ) return TRUE;
    }
    return FALSE;
}

boolean App::PollEvent( Event &e )
{
    // keyboard takes priority
    if ( kbhit() ) {
        int key = getch();
        if ( key == 0 || key == 0xE0 ) {
            key = 0x100 | ( getch() & 0xFF );   // extended key (arrows, F-keys, ...)
        }
        e.kind = Event::EV_KEY;
        e.key  = key;
        return TRUE;
    }

    // mouse: emit only on a button-state change (press or release)
    if ( mouse && mouse->IsInited() ) {
        int buttons = mouse->Status();
        if ( buttons != last_buttons ) {
            e.kind       = Event::EV_MOUSE;
            e.x          = (short)( mouse->X() / 8 );   // pixels -> text column
            e.y          = (short)( mouse->Y() / 8 );   // pixels -> text row
            e.buttons    = buttons;
            last_buttons = buttons;
            return TRUE;
        }
    }

    return FALSE;   // nothing happened this tick
}

void App::Run( void )
{
    Draw();                      // initial paint
    running = TRUE;
    last_buttons = 0;

    while ( running ) {
        Event e;
        if ( PollEvent( e ) ) {
            // let the widget tree handle it; Esc quits if nobody did
            if ( !HandleEvent( e ) ) {
                if ( e.kind == Event::EV_KEY && e.key == KEY_ESC ) {
                    running = FALSE;
                }
            }
        }
    }
}

void App::Quit( void )
{
    running = FALSE;
}
