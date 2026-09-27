/*===========================================================================
 * MSMOUSE.HPP  -  MSMouse class: INT 33h mouse driver interface
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// msmouse.hpp : class definition file

#ifndef MSMOUSE_HPP
#define MSMOUSE_HPP 1

#ifndef __NT__


#ifdef __WATCOMC__
#pragma warning( disable: 14 )
#endif

// INT 33h button-status bits (the return value of Status())
#define LEFT_BUTTON   0x01
#define RIGHT_BUTTON  0x02
#define CENTER_BUTTON 0x04

class MSMouse
{
    int mouse_inited;
    int mouse_visible;
    int mouse_position_x;
    int mouse_position_y;

    MSMouse( const MSMouse & );
    MSMouse &operator=( const MSMouse & );

public:
    MSMouse( void );
    ~MSMouse( void );

    void Show( void );
    void Hide( void );
    // Re-initialise the driver.  Needed after the VIDEO MODE changes under a
    // running program: INT 33h samples the mode when it is reset and reports
    // its coordinates and its cursor shape in that mode's terms, so a driver
    // that was reset in 80x25 text keeps handing back text-mode pixels for a
    // 640x480 screen - the pointer then reaches only the top-left quarter of
    // it.  A program that sets its mode once, before constructing this, never
    // needs to call it.
    void Reset( void );
    int IsInited( void ) const;
    int Status( void );
    int X( void ) const;        // last cursor X read by Status() (pixels; /8 for column)
    int Y( void ) const;        // last cursor Y read by Status() (pixels; /8 for row)
    void Position( const unsigned x, const unsigned y );
    int Press( unsigned button, unsigned &count, unsigned &x, unsigned &y );
    int Release( unsigned button, unsigned &count, unsigned &x, unsigned &y );
    void AreaX( const unsigned min, const unsigned max );
    void AreaY( const unsigned min, const unsigned max );
    void ReadCounters( int &x, int &y );
};

#ifdef __WATCOMC__
#pragma warning( default: 14 )
#endif

#endif // __NT__

#endif // MSMOUSE_HPP