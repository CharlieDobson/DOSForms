/*===========================================================================
 * STATBAR.HPP  -  Status bar UI control class definition (bottom-row message)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// class definition file for StatusBar

#ifndef STATUSBAR_HPP
#define STATUSBAR_HPP

#include "dosforms.hpp"
#include "structs.hpp"
#include <stdarg.h>

#define SB_TEXT_MAX 160

class StatusBar : public DOSForms
{
    /* What it currently says.  Print used to paint straight at the screen
     * and remember nothing, so Draw - which is what App calls when the
     * screen is repainted, and what every dialog closing leads to - cleared
     * the row and left it blank.  Every program using one had grown its own
     * "now put the status line back" call to work around that.  It keeps
     * its text now, and a repaint repaints it. */
    char text[SB_TEXT_MAX];

public:
    StatusBar( void );
    StatusBar( const foreground_color_t foreground, const background_color_t background );

    // methods
    virtual void Draw( void );
    void Print( const char* message );
    void Printf( const char* format, ... );

    /* What it is showing, for a caller that wants to put back what was there
     * before it borrowed the line for a moment. */
    const char *Text( void ) const { return text; }
};

#endif


