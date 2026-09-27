/*===========================================================================
 * INPUTBOX.CPP - Modal single-line text input implementation.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "inputbox.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON + MSMouse Status/X/Y for the buttons
#include "moushide.hpp"         // MouseHide cursor guard
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <conio.h>
#include <string.h>

#define BOX_H      8

// The prompt is drawn three columns in from the left border, and the right
// border needs its own column plus one of padding: this is what a box of a
// given width can actually show.
#define PROMPT_ROOM( w ) ( (w) - 6 )

InputBox::InputBox( const foreground_color_t foreground, const background_color_t background,
                    const char *titleText, const char *promptText )
    : Popup( foreground, background, titleText )
{
    short cols = DfCols() ? DfCols() : 80;
    int   want, room;

    border_style   = BORDER_DOUBLE_LINE;
    drop_shadow    = TRUE;
    text_justification = CENTER;
    buffer[0] = '\0';
    len = 0;
    cur = 0;
    scroll = 0;
    maxlen = (int)sizeof( buffer ) - 1;
    prompt[0] = '\0';
    hasSkip = 0;
    masked  = 0;

    // Grow the box to fit the prompt, but never past the screen (two columns
    // spare on the right so the drop shadow has somewhere to fall).
    want = promptText ? (int)strlen( promptText ) + 6 : IB_W_MIN;
    if ( want < IB_W_MIN ) want = IB_W_MIN;
    if ( want > cols - 2 )  want = cols - 2;
    if ( want < 24 )        want = 24;          // a screen narrower than we plan for
    boxW = want;

    // Then cut the prompt to what that box can hold.  This is the belt to the
    // sizing's braces: on an 80-column screen a prompt over about 72
    // characters cannot be shown whole however the box is sized, and the
    // border is not the place to find that out.
    room = PROMPT_ROOM( boxW );
    if ( room > (int)sizeof( prompt ) - 1 ) room = (int)sizeof( prompt ) - 1;
    if ( promptText && room > 0 )
    {
        strncpy( prompt, promptText, room );
        prompt[room] = '\0';
    }
}

// Centre the box on the screen at its chosen width.  Both entry points need
// this and both used to work it out for themselves.
void InputBox::Place( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short left = (short)( ( cols - boxW ) / 2 );
    short top  = (short)( ( rows - BOX_H ) / 2 );

    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;
    SetCoordinates( top, left, (short)( top + BOX_H - 1 ),
                    (short)( left + boxW - 1 ) );
}

void InputBox::SetText( const char *initial )
{
    if ( !initial ) { buffer[0] = '\0'; len = 0; cur = 0; scroll = 0; return; }
    strncpy( buffer, initial, maxlen );
    buffer[maxlen] = '\0';
    len = (int)strlen( buffer );
    cur = len;                  // ready to add to it, which is the usual intent
    scroll = 0;
}

void InputBox::SetMaxLength( int n )
{
    if ( n < 1 ) n = 1;
    if ( n > (int)sizeof( buffer ) - 1 ) n = (int)sizeof( buffer ) - 1;
    maxlen = n;
    if ( len > maxlen ) { len = maxlen; buffer[len] = '\0'; }
    ClampCursor();
}

void InputBox::SetSkipButton( void ) { hasSkip = 1; }
void InputBox::SetMasked( void )     { masked  = 1; }

int InputBox::FieldWidth( void ) const
{
    int fw = boxW - 6;
    if ( fw < 8 ) fw = 8;
    return fw;
}

/* Keep the cursor inside the text and the text scrolled so the cursor is on
 * screen.  One column of the field is kept free at the right so the cursor
 * standing at the END of the text has a cell of its own to sit in. */
void InputBox::ClampCursor( void )
{
    int fw = FieldWidth();

    if ( cur < 0 )   cur = 0;
    if ( cur > len ) cur = len;

    if ( cur < scroll )           scroll = cur;
    if ( cur > scroll + fw - 2 )  scroll = cur - ( fw - 2 );
    if ( scroll < 0 )             scroll = 0;
}

void InputBox::InsertChar( char c )
{
    int i;

    if ( len >= maxlen ) return;
    for ( i = len; i > cur; --i ) buffer[i] = buffer[i-1];
    buffer[cur] = c;
    ++len;
    ++cur;
    buffer[len] = '\0';
}

void InputBox::DeleteAt( int index )
{
    int i;

    if ( index < 0 || index >= len ) return;
    for ( i = index; i < len - 1; ++i ) buffer[i] = buffer[i+1];
    --len;
    buffer[len] = '\0';
}

/* Draw the reverse-video input field, scrolled so the cursor stays visible,
 * with the cursor cell drawn in inverse.  Two calls rather than one: the
 * whole field, then the single cell the cursor is on.  A block character
 * would be wrong now that the cursor can stand in the middle of the text -
 * it would hide the character it is on. */
void InputBox::DrawField( void )
{
    int  fldRow = coordinates.row + 4;         // 0-based screen row
    int  fldCol = coordinates.column + 3;
    int  fw     = FieldWidth();
    char out[IB_TEXT_MAX];
    char cell[2];
    int  i;

    if ( fw > (int)sizeof( out ) - 1 ) fw = (int)sizeof( out ) - 1;

    MouseHide guard( mouse );                   // cursor off while we paint

    ClampCursor();

    for ( i = 0; i < fw; ++i )
    {
        int src = scroll + i;
        if ( src < len )
            /* Masked: the shape of the password is still useful feedback -
             * you can see how many characters went in - but the characters
             * themselves are not put on a screen somebody may be behind. */
            out[i] = masked ? (char)250 : buffer[src];
        else
            out[i] = ' ';
    }
    out[fw] = '\0';

    DfTextOut( (short)( fldRow + 1 ), (short)( fldCol + 1 ),  // 0-based -> 1-based
               out, FG_BLACK, BG_WHITE );

    cell[0] = ( cur < len ) ? ( masked ? (char)250 : buffer[cur] ) : ' ';
    cell[1] = '\0';
    DfTextOut( (short)( fldRow + 1 ), (short)( fldCol + 1 + ( cur - scroll ) ),
               cell, FG_WHITE, BG_BLACK );
}

/* Position and draw the flat "[ OK ]" / "[ Cancel ]" buttons on ONE row, using
 * the toolkit's own single-row Button so the painted cells and the cells
 * HitTest accepts are the same by construction. */
void InputBox::LayOutButtons( void )
{
    int row = coordinates.row + BOX_H - 2;         // bottom interior row
    // "[ OK ]" is 6 cells, "[ Skip F3 ]" 11, "[ Cancel ]" 10, plus four
    // columns of gap: centre whichever row we are actually laying out.
    int wide = hasSkip ? 33 : 20;
    int oc   = coordinates.column + ( boxW - wide ) / 2;

    if ( oc < coordinates.column + 2 ) oc = coordinates.column + 2;

    btnOk     = Button( FG_WHITE, BG_CYAN );
    btnCancel = Button( FG_WHITE, BG_CYAN );
    btnSkip   = Button( FG_WHITE, BG_CYAN );
    btnOk.SetMouse( mouse );
    btnCancel.SetMouse( mouse );
    btnSkip.SetMouse( mouse );
    strcpy( btnOk.label,     "OK" );        // -> "[ OK ]",      6 cells
    strcpy( btnSkip.label,   "Skip F3" );   // -> "[ Skip F3 ]",11 cells
    strcpy( btnCancel.label, "Cancel" );    // -> "[ Cancel ]", 10 cells

    btnOk.SetSingleRow( (unsigned short)row, (unsigned short)oc );
    if ( hasSkip )
    {
        btnSkip.SetSingleRow(   (unsigned short)row, (unsigned short)( oc + 8 ) );
        btnCancel.SetSingleRow( (unsigned short)row, (unsigned short)( oc + 21 ) );
    }
    else
        btnCancel.SetSingleRow( (unsigned short)row, (unsigned short)( oc + 10 ) );
}

void InputBox::DrawButtons( void )
{
    btnOk.Draw();
    if ( hasSkip ) btnSkip.Draw();
    btnCancel.Draw();
}

void InputBox::TestOpen( int *okRow, int *okCol, int *okEndCol,
                         int *cancelCol, int *cancelEndCol )
{
    Place();

    {
        MouseHide guard( mouse );
        Draw();
        DfTextOut( (short)( coordinates.row + 3 ), (short)( coordinates.column + 4 ),
                   prompt, fg_color, bg_color );
        LayOutButtons();
        DrawButtons();
        DrawField();
    }

    *okRow        = btnOk.coordinates.row;
    *okCol        = btnOk.coordinates.column;
    *okEndCol     = btnOk.coordinates.end_column;
    *cancelCol    = btnCancel.coordinates.column;
    *cancelEndCol = btnCancel.coordinates.end_column;
}

boolean InputBox::Show( void )
{
    return ( ShowEx() == IB_OK ) ? TRUE : FALSE;
}

int InputBox::ShowEx( void )
{
    int result = IB_CANCEL;
    int lastB;

    Place();

    // Everything that paints goes inside one MouseHide guard.  Calling
    // mouse->Hide()/Show() by hand here was wrong: Popup::Draw takes its OWN
    // guard, and that guard's destructor calls Show(), so the cursor came back
    // mid-paint and could sit on top of the buttons.
    {
        MouseHide guard( mouse );

        Draw();   // Popup: save screen, draw border/title/fill

        // prompt line
        DfTextOut( (short)( coordinates.row + 3 ), (short)( coordinates.column + 4 ),
                   prompt, fg_color, bg_color );

        LayOutButtons();
        DrawButtons();
        DrawField();
    }

    // Headless harness: it has nobody to type, so it looks and leaves.
    if ( DfModalPeek() )
    {
        DfModalPeek()();
        RestoreWindow();
        DfModalFinished();
        return IB_CANCEL;
    }

    // Seed the edge detector from the LIVE button state: the menu click that
    // opened this dialog is usually still held, and starting at 0 would read it
    // as a fresh press on whatever sits under the pointer.
    lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;

    // Modal loop: type in the field; Enter / click OK accept, Esc / click Cancel.
    for ( ;; )
    {
        if ( kbhit() )
        {
            int k = getch();
            if ( k == 0 || k == 0xE0 )
            {
                int ext = getch();                              // extended keys
                if ( hasSkip && ext == _KB_F3 ) { result = IB_SKIP; break; }
                switch ( ext )
                {
                case _KB_LEFT:  if ( cur > 0 )   --cur;      DrawField(); break;
                case _KB_RIGHT: if ( cur < len ) ++cur;      DrawField(); break;
                case _KB_HOME:  cur = 0;                     DrawField(); break;
                case _KB_END:   cur = len;                   DrawField(); break;
                case _KB_DEL:   DeleteAt( cur );             DrawField(); break;
                default: break;
                }
                continue;
            }
            if ( k == 13 ) { result = IB_OK;     break; }
            if ( k == 27 ) { result = IB_CANCEL; break; }
            if ( k == 8 )
            {
                if ( cur > 0 ) { DeleteAt( cur - 1 ); --cur; }
                DrawField();
                continue;
            }
            if ( k >= 32 && k < 127 )
            {
                InsertChar( (char)k );
                DrawField();
            }
            continue;
        }
        if ( mouse && mouse->IsInited() )
        {
            int b = mouse->Status();
            if ( ( b & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
            {
                short mx = DfMouseCol( mouse->X() ), my = DfMouseRow( mouse->Y() );
                if ( btnOk.HitTest( mx, my ) )     { result = IB_OK;     break; }
                if ( hasSkip && btnSkip.HitTest( mx, my ) )
                                                   { result = IB_SKIP;   break; }
                if ( btnCancel.HitTest( mx, my ) ) { result = IB_CANCEL; break; }

                /* A click IN the field puts the cursor where it was clicked,
                 * which is the one mouse gesture a text field owes anyone. */
                if ( my == coordinates.row + 4 &&
                     mx >= coordinates.column + 3 &&
                     mx <  coordinates.column + 3 + FieldWidth() )
                {
                    cur = scroll + ( mx - ( coordinates.column + 3 ) );
                    if ( cur > len ) cur = len;
                    DrawField();
                }
            }
            lastB = b;
        }
    }

    RestoreWindow();          // guarded internally; hides the cursor while copying
    DfModalFinished();        // resync the app's edge detector - see DOSFORMS.HPP
    return result;
}
