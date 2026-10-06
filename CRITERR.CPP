/*===========================================================================
 * CRITERR.CPP - Critical errors (INT 24h).  See CRITERR.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS   (16-bit and 32-bit)
 *===========================================================================*/
#include "criterr.hpp"
#include "dlgbox.hpp"
#include <dos.h>                // _harderr, _hardresume
#include <stdio.h>
#include <string.h>

/*---- What the handler is given -------------------------------------------- *
 * 'deverr' is AX as DOS passed it.  The top bit of AH is clear for a disk
 * error and AL is then the drive; the low bits of AH say what was being done
 * and which answers DOS will take.  'errcode' is DI, the low byte of which
 * says what went wrong.  The third is the address of the device's header.
 *---------------------------------------------------------------------------*/
#define CE_NOT_DISK     0x8000
#define CE_WRITING      0x0100
#define CE_MAY_FAIL     0x0800
#define CE_MAY_IGNORE   0x2000

#if !defined( __386__ ) && !defined( __FLAT__ )
#define CE_REAL_MODE
#endif

/* Written by the handler, read by everything else.  Plain words, written
 * whole: the handler runs between two instructions of whatever the program
 * was doing, but only ever inside a DOS call the program is waiting on.
 * They are what DOS passed, as it passed it; what it MEANS is worked out
 * afterwards, by the code that asks. */
static volatile int      crit_hits   = 0;
static volatile unsigned crit_deverr = 0;
static volatile unsigned crit_code   = 0;

#ifdef CE_REAL_MODE
static unsigned __far * volatile crit_header = 0;
#endif

static DfCritAskFn  crit_ask = 0;

/*---- The handler ---------------------------------------------------------- *
 * Two compilers, one shape.  Watcom's returns a value and Microsoft's does
 * not; both are left through _hardresume, which goes back to DOS with the
 * answer and does not come back here.
 *
 * THE HANDLER LEAVES DS ALONE, which means it follows no far pointer.
 * Watcom's INT 24h stub loads DS with the program's data segment before it
 * calls the handler, and _hardresume finds the stack pointer the stub saved
 * THROUGH DS.  A large-model compiler is free to point DS somewhere else to
 * follow a far pointer, and does; _hardresume then loads the stack pointer
 * from whatever is at that offset in someone else's segment, and the machine
 * stops.  So the device header DOS passes is not read here: its address is
 * kept, and read once the call is back (CritIsDisk).
 *
 * The loadds keyword is for Microsoft's library, which is not known to load
 * DS itself.
 *
 * FAIL, not Retry and not Abort: the call comes back to the program with an
 * error, and the program decides what happens next.  DOS says in AH whether
 * it will take Fail for this error; where it will not, Ignore is the nearest
 * thing, and where it will take neither there is only Abort left.
 *
 * NO STACK CHECK in the handler.  In real mode DOS puts the program's own
 * stack back before it calls INT 24h (MSCTRLC.ASM: MOV SS,User_SS), so the
 * check the compiler puts at the top of a function would pass.  Under an
 * extender the handler arrives on whatever stack the extender reflects
 * interrupts on, which is not the program's, and the same check measures the
 * stack pointer against the bottom of a stack it is not on.
 *---------------------------------------------------------------------------*/
#if defined( __386__ ) || defined( __FLAT__ )
/* Far even here: under an extender the handler is reached by a far call,
 * and Watcom's prototype for it says so. */
#define CE_HANDLER  __far
#define CE_FAR      __far
#elif defined( __WATCOMC__ )
#define CE_HANDLER  __far __loadds
#define CE_FAR      __far
#else
#define CE_HANDLER  __far _loadds
#define CE_FAR      __far
#endif

#ifdef __WATCOMC__
#pragma off( check_stack )
#else
#pragma check_stack( off )
#endif

#ifdef __WATCOMC__
static int CE_HANDLER CritHandler( unsigned deverr, unsigned errcode,
                                   unsigned CE_FAR *devhdr )
#else
static void CE_HANDLER CritHandler( unsigned deverr, unsigned errcode,
                                    unsigned CE_FAR *devhdr )
#endif
{
    int answer;

    crit_deverr = deverr;
    crit_code   = errcode & 0xFF;
#ifdef CE_REAL_MODE
    crit_header = devhdr;
#else
    (void)devhdr;
#endif
    ++crit_hits;

    if ( deverr & CE_MAY_FAIL )        answer = _HARDERR_FAIL;
    else if ( deverr & CE_MAY_IGNORE ) answer = _HARDERR_IGNORE;
    else                               answer = _HARDERR_ABORT;

    _hardresume( answer );
#ifdef __WATCOMC__
    return answer;
#endif
}

#ifdef __WATCOMC__
#pragma on( check_stack )
#else
#pragma check_stack()           // back to what the command line asked for
#endif

/*---- Installing it, and what has to be put back --------------------------- *
 * GUARDS NEST, and only the outermost does anything: it installs the
 * handler, and when it goes the vector it found goes back.  A widget that
 * reads the disk holds one for as long as it is up (FileBox does), which
 * costs nothing in a program that has its own in main and is the handler
 * in a program that has not.
 *
 * DOS keeps the INT 24h vector it found in the program's PSP and puts it
 * back when the program ends, so a guard that lasts as long as the program
 * would have nothing to undo in real mode.  One that lasts as long as a
 * dialog has: left in, the handler fails every disk error from then on, in
 * code that was never written to ask about one.
 *
 * Under an extender there are two tables.  The handler goes into the
 * protected-mode one, and the extender points the REAL-mode vector at a
 * stub of its own that gets there - and then, at exit, finds that vector
 * changed and says so:
 *
 *     DOS/32A warning (9003): real mode interrupt vector had been
 *                             modified: INT 24h
 *
 * So the real-mode vector is read before the handler is installed and
 * written back when the guard goes, the way DfVectorGuard does it for the
 * mouse's and the font's, and with the same two DPMI calls (0200h, 0201h).
 * Where the stub is, is kept: if installing the handler a second time does
 * not move the real-mode vector by itself, it is pointed there by hand.
 *---------------------------------------------------------------------------*/
static int crit_guards = 0;

#ifdef __386__

#include <i86.h>                // int386

static unsigned short crit_rm_seg    = 0;
static unsigned short crit_rm_off    = 0;
static int            crit_rm_held   = 0;
static unsigned short crit_stub_seg  = 0;
static unsigned short crit_stub_off  = 0;
static int            crit_stub_held = 0;

static int RealVectorGet( unsigned short *seg, unsigned short *off )
{
    union REGS r;

    memset( &r, 0, sizeof( r ) );
    r.x.eax = 0x0200;                       // get real mode vector
    r.x.ebx = 0x24;
    int386( 0x31, &r, &r );
    if ( r.x.cflag ) return 0;
    *seg = r.w.cx;
    *off = r.w.dx;
    return 1;
}

static void RealVectorSet( unsigned short seg, unsigned short off )
{
    union REGS r;

    memset( &r, 0, sizeof( r ) );
    r.x.eax = 0x0201;                       // set real mode vector
    r.x.ebx = 0x24;
    r.x.ecx = seg;
    r.x.edx = off;
    int386( 0x31, &r, &r );
}

static void VectorSave( void )
{
    crit_rm_held = RealVectorGet( &crit_rm_seg, &crit_rm_off );
}

static void VectorInstalled( void )
{
    unsigned short seg, off;

    if ( !crit_rm_held ) return;
    if ( !RealVectorGet( &seg, &off ) ) return;

    if ( seg != crit_rm_seg || off != crit_rm_off )
    {
        crit_stub_seg  = seg;
        crit_stub_off  = off;
        crit_stub_held = 1;
    }
    else if ( crit_stub_held )
    {
        RealVectorSet( crit_stub_seg, crit_stub_off );
    }
}

static void VectorRestore( void )
{
    if ( !crit_rm_held ) return;
    RealVectorSet( crit_rm_seg, crit_rm_off );
    crit_rm_held = 0;
}

#else

#ifdef __WATCOMC__
typedef void ( __interrupt __far *CritVector )();
#else
typedef void ( __cdecl __interrupt __far *CritVector )();
#endif

static CritVector crit_old_vector = 0;

static void VectorSave( void )
{
    crit_old_vector = _dos_getvect( 0x24 );
}

static void VectorInstalled( void ) {}

static void VectorRestore( void )
{
    if ( !crit_old_vector ) return;
    _dos_setvect( 0x24, crit_old_vector );
    crit_old_vector = 0;
}

#endif

DfCritGuard::DfCritGuard( void )
{
    if ( crit_guards++ > 0 ) return;

    DfCritClear();
    VectorSave();
    _harderr( CritHandler );
    VectorInstalled();
}

DfCritGuard::~DfCritGuard( void )
{
    if ( --crit_guards > 0 ) return;

    VectorRestore();
}

void DfCritClear( void )
{
    crit_hits   = 0;
    crit_deverr = 0;
    crit_code   = 0;
#ifdef CE_REAL_MODE
    crit_header = 0;
#endif
}

/*---- Was it a disk? ------------------------------------------------------- *
 * The top bit of AH clear says so.  Set, it is a character device - or a
 * disk whose FAT does not add up (FAT.ASM, HURTFAT: a chain that runs off
 * the end of the disk), and AL is the drive then too.  The device header
 * tells the two apart: the top bit of its attribute word, the third word,
 * is set for a character device.
 *
 * Real mode only.  Under an extender the header's address is a real-mode
 * one that this side cannot follow as it stands, and the error is reported
 * as a device's.
 *---------------------------------------------------------------------------*/
static int CritIsDisk( void )
{
    if ( !crit_hits ) return 0;
    if ( !( crit_deverr & CE_NOT_DISK ) ) return 1;

#ifdef CE_REAL_MODE
    {
        unsigned __far *header = crit_header;

        if ( header && !( header[2] & 0x8000 ) ) return 1;
    }
#endif
    return 0;
}

int DfCritHit( void )     { return crit_hits; }
int DfCritCode( void )    { return (int)crit_code; }
int DfCritDrive( void )   { return CritIsDisk() ? (int)( crit_deverr & 0xFF ) : -1; }
int DfCritWriting( void ) { return ( crit_deverr & CE_WRITING ) ? 1 : 0; }

void DfCritText( char *buf, int size )
{
    char line[80];
    int  drive  = DfCritDrive();
    int  code   = DfCritCode();
    char letter = (char)( 'A' + ( drive >= 0 ? drive : 0 ) );

    if ( size <= 0 ) return;

    if ( drive < 0 )
    {
        strcpy( line, "Device error." );
    }
    else if ( code == DF_CRIT_NOT_READY )
    {
        sprintf( line, "No disk in drive %c:.", letter );
    }
    else if ( code == DF_CRIT_WRITE_PROTECT )
    {
        sprintf( line, "The disk in drive %c: is write-protected.", letter );
    }
    else
    {
        sprintf( line, "Error %s drive %c:.",
                 DfCritWriting() ? "writing" : "reading", letter );
    }

    strncpy( buf, line, (size_t)size - 1 );
    buf[size - 1] = '\0';
}

unsigned DfFindFirst( const char *pattern, unsigned attrib, struct find_t *found )
{
    int      before = crit_hits;
    unsigned rc     = _dos_findfirst( pattern, attrib, found );

    if ( crit_hits != before ) rc = 1;
    return rc;
}

unsigned DfFindNext( struct find_t *found )
{
    int      before = crit_hits;
    unsigned rc     = _dos_findnext( found );

    if ( crit_hits != before ) rc = 1;
    return rc;
}

void DfSetCritAsk( DfCritAskFn fn )
{
    crit_ask = fn;
}

/* INT 21h AH=0Dh.  MISC.ASM, $DISK_RESET: "flushes and invalidates all
 * buffers", and takes no notice of an error while it does. */
static void ForgetBuffers( void )
{
    bdos( 0x0D, 0, 0 );
}

boolean DfCritAsk( MSMouse *mouse )
{
    static const char *labels[] = { "Retry", "Cancel" };
    char   text[80];
    int    picked;

    if ( crit_ask )
    {
        if ( !crit_ask( DfCritCode(), DfCritDrive() ) ) return FALSE;
        ForgetBuffers();
        return TRUE;
    }

    DfCritText( text, sizeof( text ) );

    {
        DlgBox box( FG_BLACK, BG_WHITE );

        strcpy( box.title, "Error" );
        box.border_style = BORDER_DOUBLE_LINE;
        box.drop_shadow  = TRUE;
        box.SetMouse( mouse );
        box.SetMessage( text );
        box.SetButtons( labels, 2 );
        picked = box.ShowEx();
    }

    if ( picked != 0 ) return FALSE;
    ForgetBuffers();
    return TRUE;
}
