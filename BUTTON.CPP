/*===========================================================================
 * BUTTON.CPP  -  Push-button UI control: drawing, label, hotkey, drop shadow
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Button class implementation

#include <stdlib.h>             // calloc, free
#include <graph.h>              // MSVC/Watcom video config
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>             // strlen
#include <ctype.h>              // toupper
#include "button.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON
#include "moushide.hpp"         // MouseHide cursor guard

Button::Button( void ) : DOSForms( FG_YELLOW, BG_GREEN )
{
    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    label[0] = '\0';
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;
}

Button::Button( const char* label ) : DOSForms( FG_YELLOW, BG_GREEN )
{
    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    this->label[0] = '\0';
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;

    if ( label && label[0] != '\0' ) {
        strncpy( this->label, label, sizeof( this->label ) - 1 );
        this->label[sizeof( this->label ) - 1] = '\0';
    }
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
}

Button::Button( const coordinates_t* coordinates, const char* label ) : DOSForms( FG_YELLOW, BG_GREEN )
{
    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    this->label[0] = '\0';
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
    if ( label && label[0] != '\0' ) {
        strncpy( this->label, label, sizeof( this->label ) - 1 );
        this->label[sizeof( this->label ) - 1] = '\0';
    }
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
}

Button::Button( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    coordinates.row = 0;
    coordinates.column = 0;
    coordinates.end_row = 0;
    coordinates.end_column = 0;
    label[0] = '\0';
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;
}

Button::Button( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates )
    : DOSForms( foreground, background )
{
    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    label[0] = '\0';
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
}

Button::Button( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates, const char* label )
    : DOSForms( foreground, background )
{
    this->coordinates.row = 0;
    this->coordinates.column = 0;
    this->coordinates.end_row = 0;
    this->coordinates.end_column = 0;
    this->label[0] = '\0';
    hotkey_color = FG_BLACK;
    hotkey = '\0';
    on_click = 0;
    on_click_user = 0;

    if ( coordinates ) {
        this->coordinates = *coordinates;
    }
    if ( label && label[0] != '\0' ) {
        strncpy( this->label, label, sizeof( this->label ) - 1 );
        this->label[sizeof( this->label ) - 1] = '\0';
    }
    drop_shadow = TRUE;
    border_style = BORDER_SINGLE_LINE;
}

Button::Button( const Button& other ) : DOSForms( other.fg_color, other.bg_color )
{
    // carry the mouse over: a copy that silently lost it would still draw, but
    // its Draw would stop hiding the cursor and callers that rely on the copy
    // being wired up would get a control the mouse cannot reach
    mouse = other.mouse;
    coordinates = other.coordinates;
    strncpy( label, other.label, sizeof( label ) - 1 );
    label[sizeof( label ) - 1] = '\0';
    drop_shadow = other.drop_shadow;
    border_style = other.border_style;
    hotkey_color = other.hotkey_color;
    hotkey = other.hotkey;
    on_click = other.on_click;
    on_click_user = other.on_click_user;
}

Button& Button::operator=( const Button& other )
{
    if ( this != &other ) {
        fg_color = other.fg_color;
        bg_color = other.bg_color;
        mouse = other.mouse;        // see the copy constructor
        coordinates = other.coordinates;
        strncpy( label, other.label, sizeof( label ) - 1 );
        label[sizeof( label ) - 1] = '\0';
        drop_shadow = other.drop_shadow;
        border_style = other.border_style;
        hotkey_color = other.hotkey_color;
        hotkey = other.hotkey;
        on_click = other.on_click;
        on_click_user = other.on_click_user;
    }
    return *this;
}

void Button::SetCoordinates( const unsigned short row, const unsigned short column, const unsigned short end_row, const unsigned short end_column )
{
    // ensure valid coordinates
    if ( end_row < row || end_column < column ) return;

    coordinates.row = row;
    coordinates.column = column;
    coordinates.end_row = end_row;
    coordinates.end_column = end_column;
}

void Button::SetCoordinates( const unsigned short row, const unsigned short column )
{
    // default button size to 3 rows and size of label + 2 columns, or 10 columns if no label
    label[0] == '\0' ? SetCoordinates( row, column, row + 2, column + 10 ) :
                      SetCoordinates( row, column, row + 2, column + (unsigned short)strlen( label ) + 2 );
}

void Button::SetSingleRow( const unsigned short row, const unsigned short column )
{
    // "[ Label ]" = 1 bracket + 1 pad + label + 1 pad + 1 bracket, so the last
    // cell is column + strlen(label) + 3.  Draw() takes the flat one-row path
    // because row == end_row, and HitTest then covers exactly those cells.
    unsigned short width = ( label[0] == '\0' )
                           ? 4
                           : (unsigned short)strlen( label ) + 4;
    SetCoordinates( row, column, row, column + width - 1 );
}

void Button::Draw( void )
{
    if ( !IsTextMode() ) return;

    // don't draw button if coordinates are not set
    if ( coordinates.row == 0 && coordinates.column == 0
         && coordinates.end_row == 0 && coordinates.end_column == 0 ) return;

    // don't draw button if coordinate values are incorrect
    if ( coordinates.end_row < coordinates.row
         || coordinates.end_column < coordinates.column ) return;

    MouseHide guard( mouse );   // restores on every exit, incl. the early return below

    // 1-based box corners for graph.h
    short top = coordinates.row + 1;
    short left = coordinates.column + 1;
    short bottom = coordinates.end_row + 1;
    short right = coordinates.end_column + 1;

    // A single-row button (top == bottom) renders as a flat bracketed control
    // -- [ Label ] -- with no drop shadow and no box border (there is no room
    // for a border on one row), regardless of the drop_shadow/border settings.
    // Taller buttons keep the classic bordered, optionally shadowed look below.
    if ( coordinates.row == coordinates.end_row ) {
        short inner = right - left - 1;         // cells between the two brackets
        FillBox( top, left, top, right, bg_color );
        DfTextOut( top, left,  "[", fg_color, bg_color );
        DfTextOut( top, right, "]", fg_color, bg_color );
        if ( label[0] != '\0' && inner > 0 ) {
            char temp[24];
            short len = (short)strlen( label );
            if ( len > inner ) len = inner;
            strncpy( temp, label, len );
            temp[len] = '\0';
            DfTextOut( top, (short)( left + 1 + ( inner - len ) / 2 ), temp,
                       fg_color, bg_color );
        }
        return;
    }

    // draw drop shadow if specified (offset one row down, two cols right)
    if ( drop_shadow ) {
        DfShade( top + 1, left + 2, bottom + 1, right + 2 );
    }

    // fill the button face, then draw the border on top (unless borderless)
    FillBox( top, left, bottom, right, bg_color );
    if ( border_style != BORDER_NONE ) {
        DrawBox( top, left, bottom, right, border_style );
    }

    // draw button label if specified
    if ( label[0] != '\0' ) {
        short button_width = coordinates.end_column - coordinates.column;
        char* temp;

        // strncpy below asks for button_width - 2 chars; a 2-cell-or-narrower
        // button would wrap that to a huge size_t and run off the allocation
        if ( button_width < 3 ) return;

        temp = (char*)calloc( button_width, sizeof( char ) );
        if ( !temp ) return;

        strncpy( temp, label, button_width - 2 );
        temp[button_width - 1] = '\0';

        short label_length = (short)strlen( temp );
        short label_row = coordinates.row +
                          ( ( coordinates.end_row - coordinates.row ) / 2 );
        short label_column = coordinates.column +
                          ( button_width / 2 ) - ( label_length / 2 );

        DfTextOut( (short)( label_row + 1 ), (short)( label_column + 1 ), temp,
                   fg_color, bg_color );
        free( temp );
    }
}

boolean Button::HitTest( short x, short y ) const
{
    return ( y >= coordinates.row && y <= coordinates.end_row
             && x >= coordinates.column && x <= coordinates.end_column )
           ? TRUE : FALSE;
}

boolean Button::HandleEvent( const Event &e )
{
    boolean hit = FALSE;

    // a left-click inside the button
    if ( e.kind == Event::EV_MOUSE && ( e.buttons & LEFT_BUTTON )
         && HitTest( e.x, e.y ) ) {
        hit = TRUE;
    }
    // or the button's hotkey (case-insensitive)
    else if ( e.kind == Event::EV_KEY && hotkey != '\0'
              && e.key > 0 && e.key < 256
              && toupper( e.key ) == toupper( hotkey ) ) {
        hit = TRUE;
    }

    if ( hit ) {
        if ( on_click ) on_click( this, on_click_user );
        return TRUE;   // consumed
    }
    return FALSE;
}
