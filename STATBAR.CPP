/*===========================================================================
 * STATBAR.CPP  -  Status bar: Print and Printf output to the last screen row
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Statusbar class implementation file

#include <stdlib.h>             // calloc, free
#include <graph.h>              // MSVC/Watcom text output + video config
#include <stdio.h>              // _vsnprintf, _snprintf
#include <string.h>             // strncpy
#include "statbar.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

StatusBar::StatusBar( void ) : DOSForms( FG_LIGHT_CYAN, BG_CYAN )
{

}

StatusBar::StatusBar( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{

}

void StatusBar::Draw( void )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    ClearRow( video.numtextrows );
}

void StatusBar::Print( const char* message )
{
    if ( !IsTextMode() ) return;

    if ( !message || message[0] == '\0' ) return;

    // create buffer to hold status message
    // and ensure it doesn't exceed screen width
    char* buffer = (char*)calloc( video.numtextcols - 2, sizeof( char ) );

    if ( !buffer ) return;

    strncpy( buffer, message, video.numtextcols - 3 );
    buffer[video.numtextcols - 3] = '\0';

    MouseHide guard( mouse );

    ClearRow( video.numtextrows );

    _settextposition( video.numtextrows, 1 );
    _outtext( buffer );
    free( buffer );
}

void StatusBar::Printf( const char* format, ... )
{
    if ( !IsTextMode() ) return;

    if ( !format || format[0] == '\0' ) return;

    // create buffer to hold formatted status message
    char* buffer = (char*)calloc( video.numtextcols - 2, sizeof( char ) );

    if ( !buffer ) return;

    va_list args;
    va_start( args, format );
    _vsnprintf( buffer, video.numtextcols - 2, format, args );
    buffer[video.numtextcols - 3] = '\0';
    va_end( args );

    MouseHide guard( mouse );

    ClearRow( video.numtextrows );

    _settextposition( video.numtextrows, 1 );
    _outtext( buffer );
    free( buffer );
}
