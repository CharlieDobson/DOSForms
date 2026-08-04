/*===========================================================================
 * BUTTON.HPP  -  Push-button UI control class definition
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// class definition file for Buttons

#ifndef BUTTON_HPP
#define BUTTON_HPP

#include "dosforms.hpp"
#include "structs.hpp"

class Button : public DOSForms
{
public:
    Button( void );
    Button( const char* label );
    Button( const coordinates_t* coordinates, const char* label );
    Button( const foreground_color_t foreground, const background_color_t background );
    Button( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates );
    Button( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates, const char* label );
    Button( const Button& other );

    coordinates_t coordinates;
    char label[16];
    boolean drop_shadow;           // ignored for single-row buttons (see Draw)
    border_style_t border_style;   // BORDER_SINGLE_LINE (default); BORDER_NONE = borderless
    // NOTE: a single-row button (coordinates.row == coordinates.end_row) draws
    // as a flat "[ Label ]" control -- no shadow, no border -- for all apps.
    foreground_color_t hotkey_color;
    char hotkey;

    // click handling
    typedef void (*Action)( Button *button, void *user );
    Action on_click;            // invoked on a left-click within bounds, or 0
    void  *on_click_user;       // opaque context passed to on_click

    // methods
    virtual void Draw( void );
    virtual boolean HandleEvent( const Event &e );   // left-click in bounds -> on_click
    boolean HitTest( short x, short y ) const;        // is text cell (x,y) inside the button?
    void SetCoordinates( const unsigned short row, const unsigned short column, const unsigned short end_row, const unsigned short end_column );
    void SetCoordinates( const unsigned short row, const unsigned short column );
    Button& operator=( const Button& other );
};

#endif


