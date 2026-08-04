/*===========================================================================
 * DOSFORMS.CPP  -  DOSForms UI base class: color conversion and screen clear
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// implementation for DOSForms base class

#include <stdlib.h>
#include "dosforms.hpp"
#include "moushide.hpp"

// shared video mode: one screen, one mode (POD, zero-initialized at startup)
videoconfig_t DOSForms::video;
boolean DOSForms::video_queried = FALSE;

void DOSForms::EnsureVideoConfig( void )
{
    // one-time display setup; the app must have set the mode before the first form
    if ( !video_queried ) {
        _getvideoconfig( &video );
        _wrapon( _GWRAPOFF );   // no line-wrap -> no bottom-right scroll on _outtext
        video_queried = TRUE;
    }
}

DOSForms::DOSForms( void )
{
    fg_color = FG_BLUE;
    bg_color = BG_WHITE;
    mouse = 0;
    EnsureVideoConfig();
}

DOSForms::DOSForms( const foreground_color_t foreground, const background_color_t background )
{
    fg_color = foreground;
    bg_color = background;
    mouse = 0;
    EnsureVideoConfig();
}

DOSForms::~DOSForms( void )
{
    // destructor code here
}

void DOSForms::SetMouse( MSMouse *m )
{
    mouse = m;
}

boolean DOSForms::IsTextMode( void ) const
{
    // text functions and attribute-cell drawing only behave in a text mode;
    // a graphics mode would mis-render (e.g. _setbkcolor changes meaning).
    switch ( video.mode ) {
        case _TEXTBW40:
        case _TEXTC40:
        case _TEXTBW80:
        case _TEXTC80:
        case _TEXTMONO:
            return TRUE;
        default:
            return FALSE;
    }
}

void DOSForms::ClearScreen( void )
{
    if ( !IsTextMode() ) return;   // app left us in a graphics mode; refuse silently

    MouseHide guard( mouse );   // cursor hidden until this scope exits

    _settextcolor( fg_color );
    _setbkcolor( bg_color );
    _clearscreen( _GCLEARSCREEN );
}

void DOSForms::ClearRow( short row )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    _settextcolor( fg_color );
    _setbkcolor( bg_color );
    // 1-row text window + clear: paints the whole row (corner included)
    // with no bottom-right scroll; full-screen window restored after
    _settextwindow( row, 1, row, video.numtextcols );
    _clearscreen( _GWINDOW );
    _settextwindow( 1, 1, video.numtextrows, video.numtextcols );
}

void DOSForms::FillBox( short top, short left, short bottom, short right, background_color_t bg )
{
    if ( !IsTextMode() ) return;
    if ( bottom < top || right < left ) return;

    MouseHide guard( mouse );

    // a colored space-fill of a rectangle == clear a text-window region
    _setbkcolor( bg );
    _settextwindow( top, left, bottom, right );
    _clearscreen( _GWINDOW );
    _settextwindow( 1, 1, video.numtextrows, video.numtextcols );
}

void DOSForms::DrawBox( short top, short left, short bottom, short right, border_style_t style )
{
    if ( !IsTextMode() ) return;
    if ( style == BORDER_NONE ) return;
    if ( bottom < top || right < left ) return;

    MouseHide guard( mouse );

    // CP437 line-drawing chars: top-left, top-right, bottom-left,
    // bottom-right, horizontal, vertical
    unsigned char tl, tr, bl, br, hz, vt;
    switch ( style ) {
        case BORDER_DOUBLE_LINE:
            tl = 201; tr = 187; bl = 200; br = 188; hz = 205; vt = 186;
            break;
        case BORDER_SOLID:
            tl = tr = bl = br = hz = vt = 219;   // full block
            break;
        case BORDER_DOUBLE_HORIZONTAL_SINGLE_VERTICAL:
            tl = 213; tr = 184; bl = 212; br = 190; hz = 205; vt = 179;
            break;
        case BORDER_SINGLE_HORIZONTAL_DOUBLE_VERTICAL:
            tl = 214; tr = 183; bl = 211; br = 189; hz = 196; vt = 186;
            break;
        case BORDER_SINGLE_LINE:
        default:
            tl = 218; tr = 191; bl = 192; br = 217; hz = 196; vt = 179;
            break;
    }

    _settextcolor( fg_color );
    _setbkcolor( bg_color );

    short width = right - left + 1;
    char edge[82];
    short i;

    if ( width > (short)sizeof( edge ) - 1 ) width = sizeof( edge ) - 1;

    // top edge
    edge[0] = (char)tl;
    for ( i = 1; i < width - 1; ++i ) edge[i] = (char)hz;
    edge[width - 1] = (char)tr;
    edge[width] = '\0';
    _settextposition( top, left );
    _outtext( edge );

    // vertical sides
    {
        char v[2];
        v[0] = (char)vt;
        v[1] = '\0';
        for ( short r = top + 1; r < bottom; ++r ) {
            _settextposition( r, left );
            _outtext( v );
            _settextposition( r, right );
            _outtext( v );
        }
    }

    // bottom edge
    edge[0] = (char)bl;
    for ( i = 1; i < width - 1; ++i ) edge[i] = (char)hz;
    edge[width - 1] = (char)br;
    edge[width] = '\0';
    _settextposition( bottom, left );
    _outtext( edge );
}

boolean DOSForms::HandleEvent( const Event & )
{
    return FALSE;   // base forms ignore input; widgets override to respond
}
