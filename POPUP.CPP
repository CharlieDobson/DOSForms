/*===========================================================================
 * POPUP.CPP  -  Popup window: screen save/restore, border, title, drop shadow
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Popup class implementation file

#include <graph.h>              // MSVC/Watcom text output + video config
#include <stdlib.h>             // required for NULL
#include <stdio.h>              // _snprintf
#include <string.h>             // strncpy
#include "popup.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

Popup::Popup( void ) : DOSForms()
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    fg_color = FG_BLACK;
    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    border_style = BORDER_NONE;
    title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;
}

Popup::Popup( const char* title ) : DOSForms()
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    fg_color = FG_BLACK;
    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    border_style = BORDER_NONE;
    this->title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;

    if ( title ) {
        strncpy( this->title, title, sizeof( this->title ) - 1 );
        this->title[sizeof( this->title ) - 1] = '\0';
    }
}

Popup::Popup( const coordinates_t* coordinates ) : DOSForms()
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    fg_color = FG_BLACK;
    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    border_style = BORDER_NONE;
    title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
}

Popup::Popup( const coordinates_t* coordinates, const char* title ) : DOSForms()
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    fg_color = FG_BLACK;
    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    border_style = BORDER_NONE;
    this->title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }

    if ( title ) {
        strncpy( this->title, title, sizeof( this->title ) - 1 );
        this->title[sizeof( this->title ) - 1] = '\0';
    }
}

Popup::Popup( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    border_style = BORDER_NONE;
    title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;
}

Popup::Popup( const foreground_color_t foreground, const background_color_t background, const char* title )
    : DOSForms( foreground, background )
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    border_style = BORDER_NONE;
    text_justification = LEFT;
    drop_shadow = TRUE;
    this->title[0] = '\0';

    if ( title ) {
        strncpy( this->title, title, sizeof( this->title ) - 1 );
        this->title[sizeof( this->title ) - 1] = '\0';
    }
}

Popup::Popup( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates )
    : DOSForms( foreground, background )
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    border_style = BORDER_NONE;
    title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
}

Popup::Popup( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates, const char* title )
    : DOSForms( foreground, background )
{
    screen_buffer = (unsigned short*)calloc( video.numtextrows * video.numtextcols + 1, sizeof( unsigned short ) );

    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    border_style = BORDER_NONE;
    this->title[0] = '\0';
    text_justification = LEFT;
    drop_shadow = TRUE;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
    if ( title ) {
        strncpy( this->title, title, sizeof( this->title ) - 1 );
        this->title[sizeof( this->title ) - 1] = '\0';
    }
}

Popup::~Popup( void )
{
    if ( screen_buffer ) free( screen_buffer );
}

void Popup::SetCoordinates( const unsigned short row, const unsigned short column, const unsigned short end_row, const unsigned short end_column )
{
    // ensure valid coordinates
    if ( end_row < row || end_column < column ) return;

    coordinates.row = row;
    coordinates.column = column;
    coordinates.end_row = end_row;
    coordinates.end_column = end_column;
}

boolean Popup::SaveWindow( void )
{
    if ( !IsTextMode() ) return FALSE;

    if ( !screen_buffer ) return FALSE;

    if ( coordinates.row == 0 && coordinates.column == 0
         && coordinates.end_row == 0 && coordinates.end_column == 0 ) return FALSE;

    // define save area
    unsigned short start_row = coordinates.row;
    unsigned short start_column = coordinates.column;
    unsigned short end_row = coordinates.end_row;
    unsigned short end_column = coordinates.end_column;

    // account for dropshadow
    if ( drop_shadow ) {
        ++end_row;
        end_column += 2;
    }

    // clamp to the screen so direct video access stays in bounds
    if ( end_row > (unsigned short)( video.numtextrows - 1 ) ) end_row = video.numtextrows - 1;
    if ( end_column > (unsigned short)( video.numtextcols - 1 ) ) end_column = video.numtextcols - 1;

    MouseHide guard( mouse );

    // save screen contents to buffer (direct video memory read)
    unsigned vseg = ( video.mode == _TEXTMONO ) ? 0xB000 : 0xB800;
#ifdef __386__
    // DOS/4GW flat model: text buffer is directly at linear 0xB8000 / 0xB0000
    unsigned short *vid = (unsigned short *)( (unsigned long)vseg << 4 );
#else
    unsigned short far *vid = (unsigned short far *)( (unsigned long)vseg << 16 );
#endif
    unsigned short *buf = screen_buffer;
    unsigned short r, c;
    for ( r = start_row; r <= end_row; ++r ) {
        unsigned off = (unsigned)r * video.numtextcols + start_column;
        for ( c = start_column; c <= end_column; ++c ) {
            *buf++ = vid[ off++ ];
        }
    }

    return TRUE;
}

boolean Popup::RestoreWindow( void )
{
    if ( !IsTextMode() ) return FALSE;

    if ( !screen_buffer ) return FALSE;

    if ( coordinates.row == 0 && coordinates.column == 0
         && coordinates.end_row == 0 && coordinates.end_column == 0 ) return FALSE;

    // define restore area
    unsigned short start_row = coordinates.row;
    unsigned short start_column = coordinates.column;
    unsigned short end_row = coordinates.end_row;
    unsigned short end_column = coordinates.end_column;

    // account for dropshadow
    if ( drop_shadow ) {
        ++end_row;
        end_column += 2;
    }

    // clamp to the screen so direct video access stays in bounds
    if ( end_row > (unsigned short)( video.numtextrows - 1 ) ) end_row = video.numtextrows - 1;
    if ( end_column > (unsigned short)( video.numtextcols - 1 ) ) end_column = video.numtextcols - 1;

    MouseHide guard( mouse );

    // restore screen contents from buffer (direct video memory write)
    unsigned vseg = ( video.mode == _TEXTMONO ) ? 0xB000 : 0xB800;
#ifdef __386__
    unsigned short *vid = (unsigned short *)( (unsigned long)vseg << 4 );
#else
    unsigned short far *vid = (unsigned short far *)( (unsigned long)vseg << 16 );
#endif
    unsigned short *buf = screen_buffer;
    unsigned short r, c;
    for ( r = start_row; r <= end_row; ++r ) {
        unsigned off = (unsigned)r * video.numtextcols + start_column;
        for ( c = start_column; c <= end_column; ++c ) {
            vid[ off++ ] = *buf++;
        }
    }

    return TRUE;
}

void Popup::Draw( void )
{
    if ( !IsTextMode() ) return;

    if ( coordinates.row == 0 && coordinates.column == 0
         && coordinates.end_row == 0 && coordinates.end_column == 0 ) return;

    MouseHide guard( mouse );   // cursor hidden across SaveWindow and the whole draw

    // save screen area
    if ( !SaveWindow() ) return;

    // 1-based box corners for graph.h
    short top = coordinates.row + 1;
    short left = coordinates.column + 1;
    short bottom = coordinates.end_row + 1;
    short right = coordinates.end_column + 1;

    if ( drop_shadow ) {
        // draw drop shadow (offset one row down, two cols right)
        FillBox( top + 1, left + 2, bottom + 1, right + 2, BG_BLACK );
    }

    // draw the border, then fill the interior inside it
    DrawBox( top, left, bottom, right, border_style );
    FillBox( top + 1, left + 1, bottom - 1, right - 1, bg_color );

    // draw title if specified
    if ( title[0] != '\0' ) {
        char temp[38] = { '\0' };
        short title_length = (short)strlen( title );
        short title_column = coordinates.column + 3;

        switch ( text_justification ) {
            case LEFT: {
                title_column = coordinates.column + 3;
                break;
            }
            case CENTER: {
                title_column = coordinates.column +
                               ( ( coordinates.end_column - coordinates.column ) / 2 ) -
                               ( title_length / 2 );
                break;
            }
            case RIGHT: {
                title_column = coordinates.end_column - title_length - 3;
                break;
            }
            default: {
                break;
            }
        }

        _snprintf( temp, sizeof( temp ), " %s ", title ); // ensure spaces surround title
        _settextcolor( fg_color );
        _setbkcolor( bg_color );
        _settextposition( coordinates.row + 1, title_column + 1 );  // 0-based -> 1-based
        _outtext( temp );
    }
}
