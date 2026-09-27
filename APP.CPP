/*===========================================================================
 * APP.CPP  -  App: composite top-level container implementation
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// App class implementation file

#include <conio.h>              // kbhit, getch
#include <stdlib.h>             // getenv (the bare-Alt escape hatch)
#include "app.hpp"
#include "msmouse.hpp"          // MSMouse Status/X/Y for the event loop
#include "screen.hpp"           // DfMouseCol / DfMouseRow
#include "moushide.hpp"         // MouseHide cursor guard

/* The App a modal dialog should resync when it closes.  One screen, one App,
 * so a file-static is the whole of the bookkeeping - and the toolkit is told
 * about it as a CALLBACK (DfSetModalSync) rather than reaching for App by
 * name, because a widget may not assume APP.CPP is linked at all: REVEAL's
 * DOSForms build leaves it out.  See DOSFORMS.HPP. */
static App *df_current_app = 0;

static void AppModalSync( void )
{
    if ( df_current_app ) df_current_app->SyncMouseButtons();
}

App::App( void ) : Desktop()
{
    child_count = 0;
    running = FALSE;
    last_buttons = 0;
    alt_down = FALSE;
    alt_clean = FALSE;
    df_current_app = this;
    DfSetModalSync( AppModalSync );
}

App::App( const foreground_color_t foreground, const background_color_t background )
    : Desktop( foreground, background )
{
    child_count = 0;
    running = FALSE;
    last_buttons = 0;
    alt_down = FALSE;
    alt_clean = FALSE;
    df_current_app = this;
    DfSetModalSync( AppModalSync );
}

App::~App( void )
{
    /* Only disarm if we are still the registered one: a program that built a
     * second App has already pointed this at the newer object. */
    if ( df_current_app == this ) {
        df_current_app = 0;
        DfSetModalSync( 0 );
    }
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

// The keyboard shift state.  Bit 3 is "an Alt key is held down".
//
// ASKED FOR, NOT READ OUT OF MEMORY.  This used to dereference 0x417 - the
// BIOS data area at its physical address - on the same reasoning that lets the
// library write the text buffer at 0xB8000 directly.  Two things are wrong
// with that here and neither applies to the screen:
//
//   - 0x417 is in the FIRST PAGE.  A DOS extender asked for null-pointer
//     checking (DOS/32A's NULLP, and the equivalent elsewhere) makes that page
//     inaccessible, and the read faults - in a poll loop, so immediately and
//     on every run.  0xB8000 is nowhere near page zero.
//
//   - a TSR that hooks the keyboard may keep its idea of the shift state
//     somewhere other than the BIOS byte, and expects to be asked.  Reading
//     the byte behind it gets an answer it did not give.
//
// INT 16h AH=02h is the question every BIOS and every keyboard TSR answers,
// and it costs about what kbhit() a few lines up already costs - the poll loop
// is paying for one real-mode transition per turn either way.
#include <bios.h>               // _bios_keybrd

#define DF_KB_FLAGS  ( _bios_keybrd( _KEYBRD_SHIFTSTATUS ) )
#define DF_ALT_HELD  0x08

// Set XARC_NOALT (or DOSFORMS_NOALT) in the environment and the bare-Alt watch
// below is not armed at all.  It exists because bare-Alt was the one new thing
// in the keyboard path in XArc 1.2, and a tester running a keyboard TSR
// reported trouble that started with that release: with this, the hypothesis
// can be tested in one run instead of by rebuilding.  Read once - getenv in a
// poll loop would be the slowest thing in it.
static int DfAltWatchWanted( void )
{
    static int decided = -1;
    if ( decided < 0 )
        decided = ( getenv( "XARC_NOALT" ) || getenv( "DOSFORMS_NOALT" ) )
                  ? 0 : 1;
    return decided;
}

boolean App::PollEvent( Event &e )
{
    // keyboard takes priority
    if ( kbhit() ) {
        int key = getch();
        if ( key == 0 || key == 0xE0 ) {
            // The second half of an extended key.  NEVER call getch() for it
            // unguarded: getch() BLOCKS, and the two halves are only reliably
            // adjacent when nothing else is touching the keyboard.  A TSR that
            // eats a keystroke, or injects a lone zero, leaves the program
            // stopped dead inside getch() until the user presses something
            // else - and then THAT key is swallowed as the missing half, so
            // the next few keystrokes do nothing or the wrong thing.  Which is
            // what "keys that don't work" looks like from the outside.
            if ( !kbhit() ) return FALSE;       // half a key: drop it
            key = 0x100 | ( getch() & 0xFF );   // arrows, F-keys, ...
        }
        alt_clean = FALSE;      // a key came through: this Alt is a combination
        e.kind = Event::EV_KEY;
        e.key  = key;
        return TRUE;
    }

    // Bare Alt: pressed and released with nothing typed in between, which is
    // how a menu bar is reached without the mouse.  Alt is a modifier, so no
    // scan code is ever queued for it and kbhit() above will never see one -
    // the press and release have to be watched for in the BIOS flag byte.
    // Reported on the RELEASE edge, because Alt held down is the start of an
    // Alt+letter and only the letter that never arrives makes it a bare Alt.
    if ( DfAltWatchWanted() )
    {
        boolean now = ( DF_KB_FLAGS & DF_ALT_HELD ) ? TRUE : FALSE;

        if ( now && !alt_down ) {
            alt_down  = TRUE;
            alt_clean = TRUE;
        } else if ( !now && alt_down ) {
            alt_down = FALSE;
            if ( alt_clean ) {
                alt_clean = FALSE;
                e.kind = Event::EV_KEY;
                e.key  = KEY_ALT;
                return TRUE;
            }
        }
    }

    // mouse: emit only on a button-state change (press or release)
    if ( mouse && mouse->IsInited() ) {
        int buttons = mouse->Status();
        if ( buttons != last_buttons ) {
            e.kind       = Event::EV_MOUSE;
            // driver pixels -> cell.  NOT a literal 8: a graphics renderer's
            // cell is 16 pixels tall and the driver reports real pixels there
            e.x          = DfMouseCol( mouse->X() );
            e.y          = DfMouseRow( mouse->Y() );
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
    // Seed the Alt tracker from the LIVE flag, the same reasoning as the mouse
    // edge detector in the modal dialogs: starting at "up" while the user is
    // still holding the Alt that launched something would read the release as
    // a bare Alt tap and open the menu unasked.
    alt_down  = ( DfAltWatchWanted() && ( DF_KB_FLAGS & DF_ALT_HELD ) )
                ? TRUE : FALSE;
    alt_clean = FALSE;

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

void App::SyncMouseButtons( void )
{
    // A modal dialog polls the driver itself, so the presses and releases that
    // happened while it was up are invisible to PollEvent.  Adopting the
    // CURRENT state means the next poll compares against reality: no phantom
    // press gets delivered to the widget under the dialog that just closed.
    last_buttons = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;
}
