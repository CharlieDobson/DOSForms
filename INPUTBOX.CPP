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
    : Popup( foreground, background, titleText ),
      field( FG_BLACK, BG_WHITE )
{
    short cols = DfCols() ? DfCols() : 80;
    int   want, room;

    border_style   = BORDER_DOUBLE_LINE;
    drop_shadow    = TRUE;
    text_justification = CENTER;
    prompt[0] = '\0';
    hasSkip = 0;

    /* The only control in the box: the keyboard is never anywhere else. */
    field.SetFocus( TRUE );

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

void InputBox::SetText( const char *initial ) { field.SetText( initial ); }
void InputBox::SetMaxLength( int n )          { field.SetMaxLength( n ); }
void InputBox::SetSkipButton( void )          { hasSkip = 1; }
void InputBox::SetMasked( void )              { field.SetMasked( TRUE ); }

int InputBox::FieldWidth( void ) const
{
    int fw = boxW - 6;
    if ( fw < 8 ) fw = 8;
    return fw;
}

void InputBox::LayOutField( void )
{
    field.SetMouse( mouse );
    field.SetPosition( (short)( coordinates.row + 4 ),
                       (short)( coordinates.column + 3 ),
                       (short)FieldWidth() );
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
    {
        btnCancel.SetSingleRow( (unsigned short)row, (unsigned short)( oc + 10 ) );
    }
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
        LayOutField();
        field.Draw();
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
        LayOutField();
        field.Draw();
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
        Event ev;

        if ( kbhit() )
        {
            int key = getch();

            if ( key == 0 || key == 0xE0 )
            {
                /* The second half of an extended key, asked for only if it
                 * is there - see App::PollEvent. */
                if ( !kbhit() ) continue;
                key = 0x100 | ( getch() & 0xFF );
            }

            if ( hasSkip && key == ( 0x100 | _KB_F3 ) ) { result = IB_SKIP; break; }
            if ( key == KEY_ENTER ) { result = IB_OK;     break; }
            if ( key == KEY_ESC )   { result = IB_CANCEL; break; }

            ev.kind    = Event::EV_KEY;
            ev.key     = key;
            ev.x = ev.y = 0;
            ev.buttons = 0;
            field.HandleEvent( ev );
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
                ev.kind    = Event::EV_MOUSE;
                ev.key     = 0;
                ev.x       = mx;
                ev.y       = my;
                ev.buttons = b;
                field.HandleEvent( ev );
            }
            lastB = b;
        }
    }

    RestoreWindow();          // guarded internally; hides the cursor while copying
    DfModalFinished();        // resync the app's edge detector - see DOSFORMS.HPP
    return result;
}
