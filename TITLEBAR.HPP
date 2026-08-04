/*===========================================================================
 * TITLEBAR.HPP  -  Title bar UI control class definition (top-row heading)
 * Target: MSVC 1.52c / Open Watcom 1.9 DOS
 *===========================================================================*/
// class definition file for TitleBar

#ifndef TITLEBAR_HPP
#define TITLEBAR_HPP

#include "dosforms.hpp"
#include "structs.hpp"

class TitleBar : public DOSForms
{
public:
    TitleBar( void );
    TitleBar( const foreground_color_t foreground, const background_color_t background );
    TitleBar( const foreground_color_t foreground, const background_color_t background, const char* title );

    char Text[35];

    // methods
    virtual void Draw( void );
};

#endif
