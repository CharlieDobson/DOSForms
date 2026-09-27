/*===========================================================================
 * DOSFORMS.HPP  -  DOSForms UI base class definition (colors, mouse, draw)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// DOSForms base class definition

#ifndef DOSFORMS_HPP
#define DOSFORMS_HPP

// DOSForms runs on 16-bit real-mode DOS and on 32-bit DOS extenders (DOS/4GW).
// On 16-bit it reaches video memory through real-mode paragraph far pointers
// and calls INT 33h via int86(); on 32-bit (__386__) it uses a flat near
// pointer to linear 0xB8000 and int386().  Only tiny (.COM) is refused.
#if defined( M_I86TM ) || defined( __TINY__ )
#error "DOSForms needs small model or larger; tiny (.COM) is not supported."
#endif

#include "structs.hpp"
#include "event.hpp"            // Event type for HandleEvent
#include <graph.h>              // _getvideoconfig + video-mode struct (full def needed: by-value member)

// MSVC names the tag struct _videoconfig; Open Watcom names it struct videoconfig.
#ifdef __WATCOMC__
typedef struct videoconfig videoconfig_t;
#else
typedef struct _videoconfig videoconfig_t;
#endif

class MSMouse;                  // see msmouse.hpp

/*---------------------------------------------------------------------------
 * Modal plumbing - the two things every modal widget in the toolkit has to
 * do, in one place rather than copied into each of them.
 *
 * THE PEEK is for headless testing.  A modal widget runs its own input loop,
 * which a test harness has nobody to type into: with a peek installed the
 * widget draws itself, calls the peek (which snapshots the screen), and then
 * returns exactly as if the box had been cancelled.  Real runs never set one.
 *
 * THE RESYNC is the bug that bites every modal.  A widget that runs its own
 * mouse loop consumes press and release edges App::Run never sees, so the
 * next poll after the box closes can synthesize a phantom click on whatever
 * the pointer happens to be over - which, the moment a dialog closes, is
 * usually the thing that opened it.  Every Show() here ends with
 * DfModalFinished().  It is routed through a registered callback rather than
 * calling App directly because not every DOSForms program links APP.CPP
 * (REVEAL does not), and a toolkit widget may not drag one in.
 *-------------------------------------------------------------------------*/
typedef void (*DfPeekFn)( void );

void     DfSetModalPeek( DfPeekFn fn );   // 0 to disarm
DfPeekFn DfModalPeek( void );
void     DfSetModalSync( DfPeekFn fn );   // App registers itself here
void     DfModalFinished( void );         // called by every modal Show()

class DOSForms
{
public:
    DOSForms( void );
    DOSForms( const foreground_color_t foreground, const background_color_t background );
    virtual ~DOSForms( void );

    foreground_color_t fg_color;
    background_color_t bg_color;
    MSMouse *mouse;             // mouse to hide while drawing, or null
    static videoconfig_t video; // current video mode (shared: one screen, one mode)

    // methods
    virtual void SetMouse( MSMouse *m );  // virtual so containers can fan it out (see App)
    boolean IsTextMode( void ) const;   // can the active renderer paint at all?
    // Adopt a renderer's grid as the screen size every widget lays out to.
    // Called by DfSetScreen; a graphics renderer may be a different shape
    // from the text mode the card reports (80x30 against 80x25).
    static void SyncGrid( short rows, short cols );
    void ClearScreen( void );           // shared utility: clear screen to this form's colors
    void ClearRow( short row );         // shared utility: clear one row to this form's colors
    void FillBox( short top, short left, short bottom, short right, background_color_t bg ); // fill a rect (1-based)
    void DrawBox( short top, short left, short bottom, short right, border_style_t style );   // CP437 border (1-based)
    virtual void Draw( void ) = 0;      // each widget renders itself; makes the base abstract
    virtual boolean HandleEvent( const Event &e );  // respond to input; FALSE = not consumed

private:
    static boolean video_queried;       // has video been read yet?
    static void EnsureVideoConfig( void ); // query the mode once, on first construction
};

/*---------------------------------------------------------------------------
 * DfVectorGuard - put the real-mode interrupt vectors back as we found them
 *
 * A DOS extender snapshots the real-mode interrupt table when it loads and
 * compares it when the program ends.  DOS/32A says so out loud:
 *
 *     DOS/32A warning (9003): real mode interrupt vector had been
 *                             modified: INT 33h
 *
 * printed once per vector, after the screen has been handed back, which is
 * exactly where a user will read it as a crash report.  Two of ours move,
 * and neither is moved by code in this toolkit:
 *
 *   INT 43h  the character generator pointer.  The video BIOS repoints it on
 *            EVERY mode set, so simply calling _setvideomode twice - once to
 *            take the screen and once to give it back - is enough.  Putting
 *            it back matters beyond the warning: on a machine with a codepage
 *            loaded it pointed into DISPLAY.SYS's font, and the BIOS has just
 *            aimed it at ROM.
 *   INT 33h  the mouse.  The driver hooks INT 10h to watch for mode changes
 *            and re-enters itself when it sees one.
 *
 * Construct one at the top of main, BEFORE the first mode set and before the
 * MSMouse - destructors run in reverse, so declaring it first is what makes
 * it the last thing to run, after the mouse driver's own reset.  On 16-bit
 * real mode there is no extender and nothing to reconcile, so it compiles to
 * a pair of empty bodies.
 *-------------------------------------------------------------------------*/
class DfVectorGuard
{
public:
    DfVectorGuard( void );
    ~DfVectorGuard( void );

private:
    DfVectorGuard( const DfVectorGuard & );
    DfVectorGuard &operator=( const DfVectorGuard & );
};

#endif
