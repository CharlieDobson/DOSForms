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
    boolean IsTextMode( void ) const;   // is the adapter in a text mode we can draw into?
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

#endif
