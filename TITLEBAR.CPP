/*===========================================================================
 * TITLEBAR.CPP  -  Title bar: centered text on row 0 with optional mouse indicator
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Titlebar class implementation file

#include <graph.h>              // MSVC/Watcom video config
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <stdio.h>              // _snprintf
#include <string.h>             // strncpy
#include "titlebar.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

TitleBar::TitleBar( void ) : DOSForms( FG_WHITE, BG_BLUE )
{
    Text[0] = '\0';
}

TitleBar::TitleBar( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    Text[0] = '\0';
}

TitleBar::TitleBar( const foreground_color_t foreground, const background_color_t background, const char* title )
    : DOSForms( foreground, background )
{
    Text[0] = '\0';
    if ( title && title[0] != '\0' ) {
        strncpy( Text, title, sizeof( Text ) - 1 );
        Text[sizeof( Text ) - 1] = '\0';
    }
}

void TitleBar::Draw( void )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    ClearRow( 1 );

    if ( Text[0] != '\0' ) {
        short len = (short)strlen( Text );
        // 1-based column; clamp so an over-long title doesn't underflow
        short col = ( len < video.numtextcols ) ? ( video.numtextcols - len ) / 2 + 1 : 1;
        DfTextOut( 1, col, Text, fg_color, bg_color );
    }

    if ( mouse ) {
        char mouse_control[4];
        _snprintf( mouse_control, sizeof( mouse_control ), "[%c]", 196 );
        DfTextOut( 1, 1, mouse_control, fg_color, bg_color );
    }
}
