/*===========================================================================
 * PROGBOX.CPP - Progress box and "please wait" box implementation.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "progbox.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON + MSMouse status for the button
#include "moushide.hpp"         // MouseHide cursor guard
#include "screen.hpp"           // DfTextOut / DfProgressBar - text or graphics
#include <conio.h>              // kbhit, getch
#include <string.h>

/* Rows inside the box, 0-based from its top border:
 *   +0  border, with the title on it
 *   +1  blank
 *   +2  the name line
 *   +3  the bar
 *   +4  the hint, or the Cancel button
 *   +5  border
 */
#define PB_HEIGHT  6

/* Fit 'src' into 'room' columns, keeping the END of it when it will not fit.
 * A progress line is nearly always a path, and the part of a path that tells
 * you where you are is the last part of it. */
static void ShortenTail( char *dst, int room, const char *src )
{
    int n;

    if ( room < 1 ) { dst[0] = '\0'; return; }
    n = src ? (int)strlen( src ) : 0;
    if ( !src ) { dst[0] = '\0'; return; }

    if ( n <= room ) { strcpy( dst, src ); return; }
    if ( room <= 3 ) { memset( dst, '.', (size_t)room ); dst[room] = '\0'; return; }

    dst[0] = dst[1] = dst[2] = '.';
    strcpy( dst + 3, src + n - ( room - 3 ) );
}

/*===========================================================================*
 * ProgressBox
 *===========================================================================*/

ProgressBox::ProgressBox( const foreground_color_t foreground,
                          const background_color_t background,
                          const char *titleText )
    : Popup( foreground, background, titleText )
{
    border_style       = BORDER_DOUBLE_LINE;
    text_justification = CENTER;
    drop_shadow        = TRUE;
    shown_done  = 0;
    shown_total = 1;
    hasCancel   = 0;
    cancelled   = 0;
    opened      = 0;
    lastB       = 0;
}

ProgressBox::~ProgressBox( void )
{
    Close();            // a box left open by an early return still comes down
}

void ProgressBox::SetCancelButton( void )
{
    hasCancel = 1;
}

void ProgressBox::DrawFrame( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short bw   = PB_WIDTH;
    short left, top;

    if ( bw > cols ) bw = cols;
    left = (short)( ( cols - bw ) / 2 );
    top  = (short)( ( rows - PB_HEIGHT ) / 2 );
    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;

    SetCoordinates( top, left, (short)( top + PB_HEIGHT - 1 ),
                    (short)( left + bw - 1 ) );
    Draw();             // Popup: save the screen, border, title, fill

    {
        MouseHide guard( mouse );

        /* An empty bar from the start, so the box has its full shape before
         * the first item rather than growing a row late. */
        DfProgressBar( (short)( top + 4 ), (short)( left + 3 ), PB_ROOM,
                       0UL, 1UL, FG_BLUE, bg_color );

        if ( hasCancel )
        {
            btnCancel = Button( FG_WHITE, BG_CYAN );
            btnCancel.SetMouse( mouse );
            strcpy( btnCancel.label, "Cancel Esc" );
            btnCancel.SetSingleRow( (unsigned short)( top + 4 ),
                                    (unsigned short)( left + ( bw - 14 ) / 2 ) );
            btnCancel.Draw();
        }
        else
        {
            DfTextOut( (short)( top + 5 ), (short)( left + 3 ),
                       "Press Esc to cancel", fg_color, bg_color );
        }
    }
}

void ProgressBox::Open( void )
{
    if ( opened ) return;
    if ( !IsTextMode() ) return;

    cancelled = 0;
    DrawFrame();
    opened = 1;

    /* Seed the edge detector from the LIVE button state: the click that
     * started this job is usually still held, and starting at 0 would read it
     * as a fresh press on the Cancel button that has just appeared under the
     * pointer - cancelling the job before it began. */
    lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;
}

/* Non-blocking.  Esc always cancels; the button, when there is one, cancels
 * on a press edge inside it.  Other keys are swallowed rather than left in
 * the buffer to be read by whatever comes next. */
int ProgressBox::PollCancel( void )
{
    if ( kbhit() )
    {
        int k = getch();
        /* Extended: drop the pair - the second half only if it is there,
         * because getch() would wait for it (see App::PollEvent). */
        if ( k == 0 || k == 0xE0 ) { if ( kbhit() ) getch(); }
        else if ( k == _KB_ESC ) return 1;
    }

    if ( hasCancel && mouse && mouse->IsInited() )
    {
        int b = mouse->Status();
        if ( ( b & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
        {
            if ( btnCancel.HitTest( DfMouseCol( mouse->X() ),
                                    DfMouseRow( mouse->Y() ) ) )
            {
                lastB = b;
                return 1;
            }
        }
        lastB = b;
    }

    return 0;
}

boolean ProgressBox::Update( const char *text, unsigned long done,
                             unsigned long total )
{
    if ( cancelled ) return FALSE;      // answered once, answered for good
    if ( !opened ) Open();
    if ( !opened ) return TRUE;         // no screen to draw on: never cancels

    if ( total < 1 ) total = 1;
    if ( done > total ) done = total;

    {
        MouseHide guard( mouse );
        short row = coordinates.row;
        short col = coordinates.column;
        short room = (short)( coordinates.end_column - coordinates.column - 5 );

        if ( room > PB_ROOM ) room = PB_ROOM;
        if ( room < 1 ) room = 1;

        if ( text )
        {
            char line[PB_ROOM + 2];
            int  n;

            ShortenTail( line, room, text );
            /* Pad to the full width so the tail of the previous name is
             * overwritten in the same pass that writes this one. */
            for ( n = (int)strlen( line ); n < room; ++n ) line[n] = ' ';
            line[room] = '\0';
            DfTextOut( (short)( row + 3 ), (short)( col + 3 ),
                       line, fg_color, bg_color );
        }

        DfProgressBar( (short)( row + 4 ), (short)( col + 3 ), room,
                       done, total, FG_BLUE, bg_color );
    }

    shown_done  = done;
    shown_total = total;

    if ( PollCancel() ) cancelled = 1;
    return (boolean)( cancelled ? FALSE : TRUE );
}

void ProgressBox::Close( void )
{
    if ( !opened ) return;
    opened = 0;
    RestoreWindow();
    /* This box ate keystrokes and mouse edges the application's loop never
     * saw.  See DOSFORMS.HPP. */
    DfModalFinished();
}

/*===========================================================================*
 * WaitBox
 *===========================================================================*/

WaitBox::WaitBox( const foreground_color_t foreground,
                  const background_color_t background, const char *titleText )
    : Popup( foreground, background, titleText )
{
    border_style       = BORDER_DOUBLE_LINE;
    text_justification = CENTER;
    drop_shadow        = TRUE;
    opened             = 0;
}

WaitBox::~WaitBox( void )
{
    Close();
}

void WaitBox::Open( const char *message )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short bh   = 5;
    int   w    = message ? (int)strlen( message ) : 0;
    short bw, left, top;
    char  shown[128];

    if ( opened ) return;
    if ( !IsTextMode() ) return;

    if ( w > cols - 6 ) w = cols - 6;          // no wider than the screen
    if ( w > (int)sizeof( shown ) - 1 ) w = (int)sizeof( shown ) - 1;
    if ( w < 0 ) w = 0;

    bw = (short)( w + 6 );
    if ( bw < 24 ) bw = 24;
    if ( bw > cols ) bw = cols;
    left = (short)( ( cols - bw ) / 2 );
    top  = (short)( ( rows - bh ) / 2 );
    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;

    SetCoordinates( top, left, (short)( top + bh - 1 ), (short)( left + bw - 1 ) );
    Draw();
    opened = 1;

    {
        MouseHide guard( mouse );
        int mleft = left + ( bw - w ) / 2;

        if ( message ) strncpy( shown, message, (size_t)w );
        shown[w] = '\0';
        DfTextOut( (short)( top + 3 ), (short)( mleft + 1 ),   // middle interior row
                   shown, fg_color, bg_color );
    }
}

void WaitBox::Close( void )
{
    if ( !opened ) return;
    opened = 0;
    RestoreWindow();
}
