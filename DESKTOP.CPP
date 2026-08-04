/*===========================================================================
 * DESKTOP.CPP  -  Desktop: fills the whole screen with its background color
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Desktop class implementation file

#include "desktop.hpp"

Desktop::Desktop( void ) : DOSForms()
{
}

Desktop::Desktop( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
}

void Desktop::Draw( void )
{
    ClearScreen();   // fill the entire screen with the desktop colors
}
