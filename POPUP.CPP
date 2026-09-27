/*===========================================================================
 * POPUP.CPP  -  Popup window: screen save/restore, border, title, drop shadow
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Popup class implementation file

#include <graph.h>              // MSVC/Watcom video config
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <stdlib.h>             // required for NULL
#include <stdio.h>              // _snprintf
#include <string.h>             // strncpy
#include "popup.hpp"
#include "moushide.hpp"         // MouseHide cursor guard

/* Every constructor starts the save area empty; SaveWindow allocates it to
 * the rectangle it is actually asked to save.  A private helper rather than
 * four lines repeated eight times. */
void Popup::InitBuffer( void )
{
    screen_buffer = 0;
    screen_cells  = 0;
    screen_saved  = FALSE;
    saved_top = saved_left = saved_bottom = saved_right = 0;
}

Popup::Popup( void ) : DOSForms()
{
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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
    InitBuffer();

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

    // clamp to the screen so the save stays in bounds
    if ( end_row > (unsigned short)( video.numtextrows - 1 ) ) end_row = video.numtextrows - 1;
    if ( end_column > (unsigned short)( video.numtextcols - 1 ) ) end_column = video.numtextcols - 1;
    if ( end_row < start_row || end_column < start_column ) return FALSE;

    // grow the buffer to this rectangle if what we have is too small
    {
        unsigned long want = (unsigned long)( end_row - start_row + 1 ) *
                             (unsigned long)( end_column - start_column + 1 );
        if ( want > screen_cells ) {
            df_cell_t *grown = (df_cell_t *)calloc( (size_t)want, sizeof( df_cell_t ) );
            if ( !grown ) return FALSE;     // leave the old buffer alone
            if ( screen_buffer ) free( screen_buffer );
            screen_buffer = grown;
            screen_cells  = want;
        }
    }

    MouseHide guard( mouse );

    // 0-based coordinates here; the renderer's grid is 1-based
    DfScreen()->CellsRead( (short)( start_row + 1 ), (short)( start_column + 1 ),
                           (short)( end_row + 1 ), (short)( end_column + 1 ),
                           screen_buffer );

    saved_top    = start_row;
    saved_left   = start_column;
    saved_bottom = end_row;
    saved_right  = end_column;
    screen_saved = TRUE;

    return TRUE;
}

boolean Popup::RestoreWindow( void )
{
    if ( !IsTextMode() ) return FALSE;

    // Nothing was ever saved - a popup that was never drawn, or whose save
    // could not be allocated.  Putting an empty buffer back would paint a
    // black hole over whatever is there.
    if ( !screen_buffer || !screen_saved ) return FALSE;

    MouseHide guard( mouse );

    DfScreen()->CellsWrite( (short)( saved_top + 1 ), (short)( saved_left + 1 ),
                            (short)( saved_bottom + 1 ), (short)( saved_right + 1 ),
                            screen_buffer );

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
        DfShade( top + 1, left + 2, bottom + 1, right + 2 );
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
        DfTextOut( (short)( coordinates.row + 1 ), (short)( title_column + 1 ),  // 0-based -> 1-based
                   temp, fg_color, bg_color );
    }
}
