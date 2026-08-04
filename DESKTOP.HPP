/*===========================================================================
 * DESKTOP.HPP  -  Desktop: full-screen background (concrete DOSForms backdrop)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// class definition file for Desktop

#ifndef DESKTOP_HPP
#define DESKTOP_HPP

#include "dosforms.hpp"
#include "structs.hpp"

class Desktop : public DOSForms
{
public:
    Desktop( void );
    Desktop( const foreground_color_t foreground, const background_color_t background );

    // methods
    virtual void Draw( void );
};

#endif
