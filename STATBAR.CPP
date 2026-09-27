/*===========================================================================
 * STATBAR.CPP  -  Status bar: Print and Printf output to the last screen row
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Statusbar class implementation file

#include <stdlib.h>             // calloc, free
#include <graph.h>              // MSVC/Watcom video config
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <stdio.h>              // _vsnprintf, _snprintf
#include <string.h>             // strncpy
#include "statbar.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

StatusBar::StatusBar( void ) : DOSForms( FG_LIGHT_CYAN, BG_CYAN )
{
    text[0] = '\0';
}

StatusBar::StatusBar( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    text[0] = '\0';
}

/* Clear the row and put back whatever it says.  Draw is what App calls when
 * the whole screen is repainted and what a dialog closing leads to, so a
 * Draw that only cleared was a status line that went blank every time
 * anything else happened. */
void StatusBar::Draw( void )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    ClearRow( video.numtextrows );
    if ( text[0] )
        DfTextOut( video.numtextrows, 1, text, fg_color, bg_color );
}

/* Remember it, then draw it.  An empty message CLEARS the line rather than
 * being ignored: "say nothing" is a thing a caller means. */
void StatusBar::Print( const char* message )
{
    int room;

    if ( !IsTextMode() ) return;

    /* numtextcols - 3, which is what this always clipped to: one column for
     * the left margin the text starts at and two kept off the right edge.
     * Not widened while making it remember its text - a status line that
     * suddenly showed one more character would change what every existing
     * program's screen looks like. */
    room = video.numtextcols - 3;
    if ( room > (int)sizeof( text ) - 1 ) room = (int)sizeof( text ) - 1;
    if ( room < 1 ) room = 1;

    text[0] = '\0';
    if ( message )
    {
        strncpy( text, message, (size_t)room );
        text[room] = '\0';
    }

    Draw();
}

void StatusBar::Printf( const char* format, ... )
{
    char    buffer[SB_TEXT_MAX];
    va_list args;

    if ( !IsTextMode() ) return;
    if ( !format || format[0] == '\0' ) return;

    va_start( args, format );
    _vsnprintf( buffer, sizeof( buffer ) - 1, format, args );
    buffer[sizeof( buffer ) - 1] = '\0';
    va_end( args );

    Print( buffer );
}
