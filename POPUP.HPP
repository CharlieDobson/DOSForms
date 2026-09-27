/*===========================================================================
 * POPUP.HPP  -  Popup window class definition (bordered box with save/restore)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// class definition file for Popup

#ifndef POPUP_HPP
#define POPUP_HPP

#include "dosforms.hpp"
#include "structs.hpp"
#include "screen.hpp"           // df_cell_t - what a saved screen cell is

class Popup : public DOSForms
{
public:
    Popup( void );
    Popup( const char* title );
    Popup( const coordinates_t* coordinates );
    Popup( const coordinates_t* coordinates, const char* title );
    Popup( const foreground_color_t foreground, const background_color_t background );
    Popup( const foreground_color_t foreground, const background_color_t background, const char* title );
    Popup( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates );
    Popup( const foreground_color_t foreground, const background_color_t background, const coordinates_t* coordinates, const char* title );
    ~Popup( void );

private:
    Popup( const Popup & );
    Popup &operator=( const Popup & );

public:
    coordinates_t coordinates;
    border_style_t border_style;
    char title[35];
    text_justify_t text_justification;
    boolean drop_shadow;

    // methods
    virtual void Draw( void );
    void SetCoordinates( const unsigned short row, const unsigned short column, const unsigned short end_row, const unsigned short end_column );
    boolean RestoreWindow( void );

protected:
    /* What was on the screen before this popup covered it.
     *
     * Sized to the RECTANGLE when it is saved rather than to the whole
     * screen when the popup is built.  A full-screen buffer was the old
     * shape and it cost the same 4K whether the popup was the text viewer or
     * a six-row progress box, which stopped being affordable when a cell
     * grew from two bytes to four to carry an icon id (see SCREEN.HPP).
     * Sizing it to the rectangle more than pays that back: the progress box
     * now saves 1.7K where it used to save 4K.
     *
     * The saved corners are remembered with the buffer, so a restore puts
     * the cells back where they came from even if the coordinates have moved
     * in between - the old code worked the rectangle out twice and would
     * have written the saved cells to the wrong place if it ever differed. */
    df_cell_t     *screen_buffer;
    unsigned long  screen_cells;    // capacity of screen_buffer, in cells
    boolean        screen_saved;    // is there anything to put back?
    unsigned short saved_top, saved_left, saved_bottom, saved_right;  // 0-based

    boolean SaveWindow( void );

private:
    void InitBuffer( void );        // shared by all eight constructors
};

#endif


