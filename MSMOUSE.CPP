/*===========================================================================
 * MSMOUSE.CPP  -  MSMouse class: INT 33h mouse driver interface
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// msmouse.cpp : class implementation file

#include "msmouse.hpp"

#ifndef __NT__

#include <dos.h>		// REGS

// 16-bit uses int86()/REGS.x.ax; 32-bit DOS extenders (DOS/4GW) use int386()
// and the 16-bit register view REGS.w.ax (REGS.x is the 32-bit eax view there).
#ifdef __386__
  #define MINT   int386
  #define MINTX  int386x
  #define REGW   w
#else
  #define MINT   int86
  #define MINTX  int86x
  #define REGW   x
#endif

/*---------------------------------------------------------------------------
 * MSMouse::MSMouse
 * Detect and initialise the mouse driver via INT 33h.  Reads version,
 * hardware type, and IRQ number if available.
 *---------------------------------------------------------------------------*/
MSMouse::MSMouse( void )
{
    mouse_inited = 0;
    mouse_visible = 0;
    mouse_position_x = 0;
    mouse_position_y = 0;

    union REGS regs;
#ifndef __386__
    struct SREGS sregs;

    // 16-bit real mode: probe the INT 33h vector first (a zero vector means no
    // driver).  NOT done on a 32-bit DOS extender: int386x here would load the
    // UNINITIALISED SREGS as real segment selectors and GP-fault, and the
    // driver reset below is a sufficient presence test anyway.
    regs.h.ah = 0x35;
    regs.h.al = 33;
    MINTX( 0x21, &regs, &regs, &sregs );
    if ( ( sregs.es | regs.REGW.bx ) == 0 ) {
        return;
    }
#endif

    regs.REGW.ax = 0x0000; // reset mouse driver
    MINT( 0x33, &regs, &regs );
    if ( regs.REGW.ax == 0 ) {
        return;             // AX==0 => no driver installed
    }

    // A successful reset (AX==0xFFFF) is the authoritative "mouse present"
    // signal, so mark it inited here.  We do NOT gate on the enable call
    // below: function 0x0020 is non-standard and some hosts (notably NTVDM,
    // the Windows NT DOS box) return something other than 0xFFFF from it even
    // though the mouse works fine -- gating on it there wrongly disabled the
    // mouse.  DOSBox / real DOS still work because the reset already reported
    // presence.
    mouse_inited = 1;

    regs.REGW.ax = 0x0020;	// enable mouse driver (best-effort; result ignored)
    MINT( 0x33, &regs, &regs );
}

/*---------------------------------------------------------------------------
 * MSMouse::~MSMouse
 * Hide the cursor and reset the driver on destruction.
 *---------------------------------------------------------------------------*/
MSMouse::~MSMouse( void )
{
    if ( mouse_inited != 0 ) {
		if ( mouse_visible != 0 ) {
			Hide();
		}

		union REGS regs;
		regs.REGW.ax = 0x0000;	// reset mouse driver
		MINT( 0x33, &regs, &regs );
    }
}

/*---------------------------------------------------------------------------
 * MSMouse::Reset
 * Re-run the driver reset after a video mode change.  The cursor comes back
 * hidden, which is what INT 33h AX=0000h does, so our visible flag is put
 * back in step with it rather than left claiming a cursor that is gone.
 *---------------------------------------------------------------------------*/
void MSMouse::Reset( void )
{
    union REGS regs;

    if ( mouse_inited == 0 ) return;

    regs.REGW.ax = 0x0000;
    MINT( 0x33, &regs, &regs );
    mouse_visible = 0;

    regs.REGW.ax = 0x0020;      // enable (best-effort, as in the constructor)
    MINT( 0x33, &regs, &regs );
}

/*---------------------------------------------------------------------------
 * MSMouse::Show / Hide
 * Show or hide the hardware mouse cursor via INT 33h AX=0001h / 0002h.
 *---------------------------------------------------------------------------*/
void MSMouse::Show( void )
{
	if ( mouse_visible != 0 || mouse_inited == 0 ) {
		return;
	}
	
    union REGS regs;
    regs.REGW.ax = 0x0001;
    MINT( 0x33, &regs, &regs );
    mouse_visible = 1;
}

void MSMouse::Hide( void )
{
	if ( mouse_visible == 0 || mouse_inited == 0 ) {
		return;
	}
	
    union REGS regs;
    regs.REGW.ax = 0x0002;
    MINT( 0x33, &regs, &regs );
    mouse_visible = 0;
}

/*---------------------------------------------------------------------------
 * MSMouse::IsInited
 * Return non-zero if the mouse driver was successfully initialised.
 *---------------------------------------------------------------------------*/
int MSMouse::IsInited( void ) const
{
    return mouse_inited;
}

// Function: Status()
// return value is state of buttons as bit pattern
// bit 0: left button ( 1 == down, 0 == up )
// bit 1: right button ( 1 == down, 0 == up )
// bit 2: center button ( 1 == down, 0 == up )
// e.g.: 0000000000000011 = mouse left and right pressed down
// e.g.: 0000000000000010 = right button pressed down
// other bits should be ignored
int MSMouse::Status( void )
{
	if ( mouse_inited == 0 ) {
		return -1;
	}
	
    union REGS regs;
    regs.REGW.ax = 0x0003;
    MINT( 0x33, &regs, &regs );
    mouse_position_x = regs.REGW.cx;
    mouse_position_y = regs.REGW.dx;
    return regs.REGW.bx;
}

// Function: X() / Y()
// last cursor position cached by Status() (pixels; divide by 8 for text cells)
int MSMouse::X( void ) const
{
    return mouse_position_x;
}

int MSMouse::Y( void ) const
{
    return mouse_position_y;
}

/*---------------------------------------------------------------------------
 * MSMouse::Position
 * Set the mouse cursor to the given (x, y) position via INT 33h AX=0004h.
 *---------------------------------------------------------------------------*/
void MSMouse::Position( const unsigned x, const unsigned y )
{
	if ( mouse_inited == 0 ) {
		return;
	}

    union REGS regs;
    regs.REGW.ax = 0x0004;
    regs.REGW.cx = x;
    regs.REGW.dx = y;
    MINT( 0x33, &regs, &regs );
    mouse_position_x = x;
    mouse_position_y = y;
}

// Function: Press()
// return value is state of buttons as bit pattern
// bit 0: left button ( 1 == down, 0 == up )
// bit 1: right button ( 1 == down, 0 == up )
// bit 2: center button ( 1 == down, 0 == up )
// other bits should be ignored
// button = button to query
// count = number of presses since last call to Press() (output parameter)
// x = column position of last press (output parameter)
// y = row position of last press (output parameter)
int MSMouse::Press( unsigned button, unsigned &count, unsigned &x, unsigned &y )
{
	if ( mouse_inited == 0 ) {
		count = 0;
		x = 0;
		y = 0;
		return -1;
	}

    if ( button > 2 ) {
		count = 0;
		x = 0;
		y = 0;
        return -2;
    }

    union REGS regs;
    regs.REGW.ax = 0x0005;
    regs.REGW.bx = button;
    MINT( 0x33, &regs, &regs );
    count = regs.REGW.bx;
    x = regs.REGW.cx;
    y = regs.REGW.dx;
    return regs.REGW.ax;
}

// Function: Release()
// return value is state of buttons as bit pattern
// bit 0: left button ( 1 == down, 0 == up )
// bit 1: right button ( 1 == down, 0 == up )
// bit 2: center button ( 1 == down, 0 == up )
// other bits should be ignored
// button = button to query
// count = number of releases since last call to Release() (output parameter)
// x = column position of last release (output parameter)
// y = row position of last release (output parameter)
int MSMouse::Release( unsigned button, unsigned &count, unsigned &x, unsigned &y )
{
	if ( mouse_inited == 0 ) {
		count = 0;
		x = 0;
		y = 0;
		return -1;
	}

    if ( button > 2 ) {
		count = 0;
		x = 0;
		y = 0;
        return -2;
    }

    union REGS regs;
    regs.REGW.ax = 0x0006;
    regs.REGW.bx = button;
    MINT( 0x33, &regs, &regs );
    count = regs.REGW.bx;
    x = regs.REGW.cx;
    y = regs.REGW.dx;
    return regs.REGW.ax;
}

// Function: AreaX
// defines min and max mouse position on the X axis
void MSMouse::AreaX( const unsigned min, const unsigned max )
{
	if ( mouse_inited == 0 ) {
		return;
	}

    union REGS regs;
    regs.REGW.ax = 0x0007;
    regs.REGW.cx = min;
    regs.REGW.dx = max;
    MINT( 0x33, &regs, &regs );
}

// Function: AreaY
// defines min and max mouse position on the Y axis
void MSMouse::AreaY( const unsigned min, const unsigned max )
{
	if ( mouse_inited == 0 ) {
		return;
	}

    union REGS regs;
    regs.REGW.ax = 0x0008;
    regs.REGW.cx = min;
    regs.REGW.dx = max;
    MINT( 0x33, &regs, &regs );
}

// Function: ReadCounters
// returns x and y of mouse motion in mickeys
void MSMouse::ReadCounters( int &x, int &y )
{
	if ( mouse_inited == 0 ) {
		x = 0;
		y = 0;
		return;
	}
	
    union REGS regs;
    regs.REGW.ax = 0x000b;
    MINT( 0x33, &regs, &regs );
    x = regs.REGW.cx;
    y = regs.REGW.dx;
}

#endif // __NT__