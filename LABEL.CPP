/*===========================================================================
 * LABEL.CPP  -  Static text label: positioned text output with printf support
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Label class implementation file

#include <graph.h>              // MSVC/Watcom text output + video config
#include <stdio.h>              // _vsnprintf
#include <string.h>             // strncpy, strlen
#include "label.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

Label::Label( void ) : DOSForms( FG_BLACK, BG_WHITE )
{
    row = 0;
    column = 0;
    text[0] = '\0';
}

Label::Label( const char* text ) : DOSForms( FG_BLACK, BG_WHITE )
{
    row = 0;
    column = 0;
    this->text[0] = '\0';

    if ( text && text[0] != '\0' ) {
        strncpy( this->text, text, sizeof( this->text ) - 1 );
        this->text[sizeof( this->text ) - 1] = '\0';
    }
}

Label::Label( const unsigned short row, const unsigned short column, const char* text )
    : DOSForms( FG_BLACK, BG_WHITE )
{
    this->row = row;
    this->column = column;
    this->text[0] = '\0';

    if ( text && text[0] != '\0' ) {
        strncpy( this->text, text, sizeof( this->text ) - 1 );
        this->text[sizeof( this->text ) - 1] = '\0';
    }
}

Label::Label( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    row = 0;
    column = 0;
    text[0] = '\0';
}

Label::Label( const foreground_color_t foreground, const background_color_t background, const char* text )
    : DOSForms( foreground, background )
{
    row = 0;
    column = 0;
    this->text[0] = '\0';

    if ( text && text[0] != '\0' ) {
        strncpy( this->text, text, sizeof( this->text ) - 1 );
        this->text[sizeof( this->text ) - 1] = '\0';
    }
}

Label::Label( const foreground_color_t foreground, const background_color_t background, const unsigned short row, const unsigned short column, const char* text )
    : DOSForms( foreground, background )
{
    this->row = row;
    this->column = column;
    this->text[0] = '\0';

    if ( text && text[0] != '\0' ) {
        strncpy( this->text, text, sizeof( this->text ) - 1 );
        this->text[sizeof( this->text ) - 1] = '\0';
    }
}

void Label::SetPosition( const unsigned short row, const unsigned short column )
{
    this->row = row;
    this->column = column;
}

void Label::SetText( const char* format, ... )
{
    this->text[0] = '\0';

    if ( !format || format[0] == '\0' ) return;

    va_list args;
    va_start( args, format );
    _vsnprintf( this->text, sizeof( this->text ) - 1, format, args );
    this->text[sizeof( this->text ) - 1] = '\0';
    va_end( args );
}

void Label::Draw( void )
{
    if ( !IsTextMode() ) return;

    if ( text[0] == '\0' ) return;

    MouseHide guard( mouse );

    _settextcolor( fg_color );
    _setbkcolor( bg_color );
    _settextposition( row + 1, column + 1 );   // stored 0-based -> graph.h 1-based
    _outtext( text );
}
