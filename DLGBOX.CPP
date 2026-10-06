/*===========================================================================
 * DLGBOX.CPP  -  Modal dialog box: message display, button layout, input loop
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// DlgBox class implementation file

#include <graph.h>              // MSVC/Watcom video config
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <stdlib.h>             // required for NULL
#include <stdio.h>              // _snprintf
#include <string.h>             // strncpy
#include <conio.h>              // kbhit, getch
#include <ctype.h>              // toupper
#include "dlgbox.hpp"
#include "msmouse.hpp"          // MSMouse class (Status/X/Y for the modal mouse loop)
#include "moushide.hpp"         // MouseHide cursor guard

DlgBox::DlgBox( const foreground_color_t foreground, const background_color_t background )
    : Popup( foreground, background )
{
    text_justification = CENTER;
    align_block = FALSE;
    message[0] = '\0';
    buttons = DB_OK;
    custom_count = 0;
}

DlgBox::DlgBox( const foreground_color_t foreground, const background_color_t background, const char* message )
    : Popup( foreground, background )
{
    text_justification = CENTER;
    align_block = FALSE;

    this->message[0] = '\0';
    if ( message && message[0] != '\0' ) {
        strncpy( this->message, message, sizeof( this->message ) - 1 );
        this->message[ sizeof( this->message ) - 1 ] = '\0';
    }

    buttons = DB_OK;
    custom_count = 0;
}

DlgBox::DlgBox( const foreground_color_t foreground, const background_color_t background, const dialog_buttons_t* buttons )
    : Popup( foreground, background )
{
    text_justification = CENTER;
    align_block = FALSE;
    message[0] = '\0';

    this->buttons = DB_OK;
    custom_count = 0;
    if ( buttons ) {
        this->buttons = *buttons;
    }
}

DlgBox::DlgBox( const foreground_color_t foreground, const background_color_t background, const dialog_buttons_t* buttons, const char* message )
    : Popup( foreground, background )
{
    text_justification = CENTER;
    align_block = FALSE;

    this->message[0] = '\0';
    if ( message && message[0] != '\0' ) {
        strncpy( this->message, message, sizeof( this->message ) - 1 );
        this->message[ sizeof( this->message ) - 1 ] = '\0';
    }

    this->buttons = DB_OK;
    custom_count = 0;
    if ( buttons ) {
        this->buttons = *buttons;
    }
}

/*---- The wrap ------------------------------------------------------------- *
 * One word-wrapper, used by Draw to paint and by AutoSize to measure, so the
 * box cannot end up sized to a different set of lines from the ones that get
 * painted into it.  It honours '\n' - the message is the place a caller says
 * where a paragraph ends - and breaks everything else at spaces, rewinding
 * to the last one so a word travels to the next line intact.  A word longer
 * than the line is cut where it stands, because there is nowhere else for it
 * to go.
 *
 * The line store is a file static rather than a member: a dialog object is a
 * local in somebody's stack frame, only one is ever painting at a time, and
 * every Draw re-wraps from 'message' - so two dialogs alive at once (the
 * overwrite prompt over a progress box does exactly that) cannot read each
 * other's lines.  It also keeps the object small enough to be a local on a
 * 16-bit program's stack, which 24 x 78 bytes of member would not be.
 *---------------------------------------------------------------------------*/
#define DLG_WRAP_LINES  24
#define DLG_WRAP_COLS   78

static char  dlg_line[DLG_WRAP_LINES][DLG_WRAP_COLS];
static short dlg_lines = 0;         // how many were filled
static short dlg_widest = 0;        // longest of them, in columns

static void DlgWrap( const char* text, short width, short max_lines )
{
    const char* p = text;

    dlg_lines  = 0;
    dlg_widest = 0;

    if ( width < 1 ) width = 1;
    if ( width > DLG_WRAP_COLS - 1 ) width = DLG_WRAP_COLS - 1;
    if ( max_lines > DLG_WRAP_LINES ) max_lines = DLG_WRAP_LINES;
    if ( max_lines < 1 ) max_lines = 1;
    if ( !text ) return;

    while ( *p && dlg_lines < max_lines ) {
        const char* q   = p;
        short       c   = 0;
        short       brk = -1;       // length of the line if broken at a space

        while ( *q && *q != '\n' && c < width ) {
            if ( *q == ' ' ) brk = c;
            dlg_line[dlg_lines][c++] = *q++;
        }

        // Filled up with more of the same word still to come: rewind to the
        // last space so the word goes to the next line whole.
        if ( *q && *q != '\n' && *q != ' ' && brk > 0 ) {
            c = brk;
            q = p + brk;
        }

        while ( c > 0 && dlg_line[dlg_lines][c-1] == ' ' ) --c;   // no ragged tails
        dlg_line[dlg_lines][c] = '\0';
        if ( c > dlg_widest ) dlg_widest = c;
        ++dlg_lines;

        while ( *q == ' ' ) ++q;            // the space we broke at, and its run
        if ( *q == '\n' ) ++q;
        p = q;
    }

    // An empty message is still one (blank) line, so a box built for it has
    // somewhere to put nothing and does not collapse onto its own border.
    if ( dlg_lines == 0 ) {
        dlg_line[0][0] = '\0';
        dlg_lines = 1;
    }
}

void DlgBox::SetMessage( const char* text )
{
    message[0] = '\0';
    if ( text && text[0] != '\0' ) {
        strncpy( message, text, sizeof( message ) - 1 );
        message[ sizeof( message ) - 1 ] = '\0';
    }
}

/*---- Sizing the box to the words ------------------------------------------ *
 * Six of the screen's columns are spoken for before a word is placed: the
 * text sits three columns in from the left border and the right border needs
 * its own column plus one of padding.  Two more are kept back so the box is
 * not flush with the screen edge and the drop shadow has somewhere to fall.
 *
 * Height is the same arithmetic downwards - border, blank row, the text, a
 * blank row, the button row, border - and the line count is capped at what
 * the screen can hold rather than at a constant, because the graphics
 * renderer's grid is thirty rows where text mode's is twenty-five.
 *---------------------------------------------------------------------------*/
void DlgBox::AutoSize( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short wrap, max_lines, w, h, top, left;

    wrap = (short)( cols - 8 );
    if ( wrap < 16 ) wrap = 16;

    max_lines = (short)( rows - 6 );
    if ( max_lines < 1 ) max_lines = 1;

    DlgWrap( message, wrap, max_lines );

    w = (short)( dlg_widest + 6 );
    if ( w < 24 ) w = 24;               // narrower than the button row needs
    if ( w > cols ) w = cols;

    // border + blank + text + blank + buttons + border
    h = (short)( dlg_lines + 5 );
    if ( h > rows ) h = rows;

    left = (short)( ( cols - w ) / 2 );
    top  = (short)( ( rows - h ) / 2 );
    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;

    SetCoordinates( top, left, (short)( top + h - 1 ), (short)( left + w - 1 ) );
}

void DlgBox::Draw( void )
{
    short inner_width, inner_top, button_area_top, inner_height, start_row, i;

    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    // let Popup draw the box, border, shadow, and title
    Popup::Draw();

    // nothing more to draw if no message
    if ( message[0] == '\0' ) return;

    // calculate usable width inside the border
    inner_width = (short)( coordinates.end_column - coordinates.column - 2 );
    if ( inner_width < 1 ) inner_width = 1;

    // vertically center the lines above the button area.  Show() puts its flat
    // one-row buttons on the bottom interior row (end_row - 1), so the text may
    // run down to the row above that.
    inner_top       = (short)( coordinates.row + 1 );
    button_area_top = (short)( coordinates.end_row - 2 );
    inner_height    = (short)( button_area_top - inner_top );
    if ( inner_height < 1 ) inner_height = 1;

    DlgWrap( message, inner_width, inner_height );

    start_row = (short)( inner_top + ( inner_height - dlg_lines ) / 2 );
    if ( start_row < inner_top ) start_row = inner_top;

    for ( i = 0; i < dlg_lines; i++ ) {
        // As a block, every line is placed as if it were as long as the
        // longest: they share a left edge, and the justification says where
        // the block goes rather than where each line does.
        short len = align_block ? dlg_widest : (short)strlen( dlg_line[i] );
        short col;

        switch ( text_justification ) {
            case CENTER:
                col = (short)( coordinates.column + 1 + ( inner_width - len ) / 2 );
                break;
            case RIGHT:
                col = (short)( coordinates.end_column - 1 - len );
                break;
            case LEFT:
            default:
                col = (short)( coordinates.column + 2 );
                break;
        }

        DfTextOut( (short)( start_row + i + 1 ), (short)( col + 1 ),   // 0-based -> 1-based
                   dlg_line[i], fg_color, bg_color );
    }
}

/*---- Which buttons, and what each one means ------------------------------- *
 * One place, because Show has to map an index back to an enum value and
 * ShowEx has to lay the same buttons out: two copies of this would be two
 * chances for the third button to mean different things to the two of them.
 *---------------------------------------------------------------------------*/
int DlgBox::BuildButtons( const char **labels, button_return_value_t *values )
{
    int i;

    for ( i = 0; i < DLG_MAX_BUTTONS; ++i ) {
        labels[i] = (const char*)0;
        values[i] = BUTTON_OK;
    }

    /* Buttons the caller named win: the enum is the shorthand, not the rule. */
    if ( custom_count > 0 ) {
        for ( i = 0; i < custom_count; ++i ) {
            labels[i] = custom[i];
            /* A caller using its own buttons reads the INDEX; the enum is
             * filled in only so Show has something to answer with. */
            values[i] = ( i == 0 ) ? BUTTON_OK : BUTTON_CANCEL;
        }
        return custom_count;
    }

    switch ( buttons ) {
        case DB_OKCANCEL:
            labels[0] = "OK";      values[0] = BUTTON_OK;
            labels[1] = "Cancel";  values[1] = BUTTON_CANCEL;
            return 2;
        case DB_YESNO:
            labels[0] = "Yes";     values[0] = BUTTON_YES;
            labels[1] = "No";      values[1] = BUTTON_NO;
            return 2;
        case DB_NEXTBACK:
            labels[0] = "Back";    values[0] = BUTTON_BACK;
            labels[1] = "Next";    values[1] = BUTTON_NEXT;
            return 2;
        case DB_INSTALLCANCEL:
            labels[0] = "Install"; values[0] = BUTTON_INSTALL;
            labels[1] = "Cancel";  values[1] = BUTTON_CANCEL;
            return 2;
        case DB_OK:
        default:
            labels[0] = "OK";      values[0] = BUTTON_OK;
            return 1;
    }
}

void DlgBox::SetButtons( const char * const *labels, int n )
{
    int i;

    custom_count = 0;
    if ( !labels || n < 1 ) return;
    if ( n > DLG_MAX_BUTTONS ) n = DLG_MAX_BUTTONS;

    for ( i = 0; i < n; ++i ) {
        custom[i][0] = '\0';
        if ( labels[i] ) {
            strncpy( custom[i], labels[i], DLG_LABEL_MAX - 1 );
            custom[i][DLG_LABEL_MAX - 1] = '\0';
        }
    }
    custom_count = n;
}

/*---- Shortcut letters ----------------------------------------------------- *
 * The first letter of each WORD of the label, in order, that no earlier
 * button has already claimed; failing that, any letter of it that is free.
 *
 * That rule is not arbitrary - it is what a person would pick.  Yes / No /
 * Yes All / No All comes out as Y, N, A, O: "Yes All" cannot have Y so it
 * takes the A of "All", and "No All" can have neither N nor A so it falls
 * back to the o in "No".  Those are exactly the four letters the dialog
 * this replaced used, and it had them typed in by hand.
 *---------------------------------------------------------------------------*/
static void DlgHotkeys( const char **labels, int n, char *keys )
{
    char used[DLG_MAX_BUTTONS + 1];
    int  nused = 0;
    int  i, j, k;

    for ( i = 0; i < n; ++i ) {
        keys[i] = 0;

        /* first letter of each word */
        for ( j = 0; labels[i][j] && !keys[i]; ++j ) {
            char c;
            if ( j > 0 && labels[i][j-1] != ' ' ) continue;
            c = (char)toupper( (unsigned char)labels[i][j] );
            if ( !isalnum( (unsigned char)c ) ) continue;
            for ( k = 0; k < nused; ++k ) if ( used[k] == c ) break;
            if ( k == nused ) keys[i] = c;
        }
        /* failing that, any letter at all */
        for ( j = 0; labels[i][j] && !keys[i]; ++j ) {
            char c = (char)toupper( (unsigned char)labels[i][j] );
            if ( !isalnum( (unsigned char)c ) ) continue;
            for ( k = 0; k < nused; ++k ) if ( used[k] == c ) break;
            if ( k == nused ) keys[i] = c;
        }
        if ( keys[i] ) used[nused++] = keys[i];
    }
}

button_return_value_t DlgBox::Show( void )
{
    const char           *labels[DLG_MAX_BUTTONS];
    button_return_value_t values[DLG_MAX_BUTTONS];
    int                   n = BuildButtons( labels, values );
    int                   i = ShowEx();

    if ( i < 0 || i >= n ) return BUTTON_CANCEL;
    return values[i];
}

int DlgBox::ShowEx( void )
{
    if ( !IsTextMode() ) return -1;

    // A dialog nobody gave a rectangle to sizes itself around its message.
    // Checked here rather than in the constructor because the message and the
    // title can both be set after construction, and because a caller that
    // DOES set coordinates (DiskStat's About box) must get exactly those.
    if ( coordinates.end_row <= coordinates.row ||
         coordinates.end_column <= coordinates.column ) {
        AutoSize();
    }

    // draw the dialog box with message
    Draw();

    const char*           button_labels[DLG_MAX_BUTTONS];
    button_return_value_t button_values[DLG_MAX_BUTTONS];
    char                  button_keys[DLG_MAX_BUTTONS];
    short                 button_count = (short)BuildButtons( button_labels, button_values );

    DlgHotkeys( button_labels, (int)button_count, button_keys );

    // Create and position the buttons on the bottom interior row of the dialog.
    // They are flat single-row "[ Label ]" controls (Button::SetSingleRow), all
    // on ONE row with two blank cells between them, so every dialog in the
    // toolkit gets the same look and the drawn cells are exactly the cells
    // HitTest accepts.
    Button dlg_buttons[DLG_MAX_BUTTONS];
    short button_widths[DLG_MAX_BUTTONS];
    short button_row = coordinates.end_row - 1;
    short inner_width = coordinates.end_column - coordinates.column - 2;
    short total_buttons_width = ( button_count - 1 ) * 2;   // the inter-button gaps
    short start_column;
    short i;

    for ( i = 0; i < button_count; ++i ) {
        button_widths[i] = (short)strlen( button_labels[i] ) + 4;   // "[ " + label + " ]"
        total_buttons_width += button_widths[i];
    }

    start_column = coordinates.column + 1 + ( inner_width - total_buttons_width ) / 2;
    if ( start_column < coordinates.column + 1 ) start_column = coordinates.column + 1;

    for ( i = 0; i < button_count; ++i ) {
        dlg_buttons[i] = Button( FG_BLACK, BG_WHITE );
        dlg_buttons[i].SetMouse( mouse );
        dlg_buttons[i].drop_shadow = FALSE;      // ignored on one row, but be explicit
        strncpy( dlg_buttons[i].label, button_labels[i], sizeof( dlg_buttons[i].label ) - 1 );
        dlg_buttons[i].label[sizeof( dlg_buttons[i].label ) - 1] = '\0';
        dlg_buttons[i].SetSingleRow( button_row, start_column );
        dlg_buttons[i].Draw();
        start_column += button_widths[i] + 2;
    }

    // modal input loop.  The answer is an INDEX; Show turns it back into an
    // enum value for the callers that want one.
    int result = 0;
    int done = 0;

    // Seed the edge detector from the LIVE button state, not from 0 and not
    // from a function static.  The click that opened this dialog is usually
    // still physically down when we get here; starting at 0 would read that
    // held button as a fresh press and fire whichever button happens to sit
    // under the pointer, so the dialog would vanish before it could be read.
    // A local (not static) also stops one dialog's exit state leaking into the
    // next dialog's first poll.
    int old_mouse_status = mouse ? mouse->Status() : 0;

    // Headless harness: draw, let it look, and leave as if the last button
    // had been pressed.  Nothing is typed into a dialog in a test run.
    if ( DfModalPeek() ) {
        DfModalPeek()();
        RestoreWindow();
        DfModalFinished();
        return (int)( button_count - 1 );
    }

    while ( !done ) {
        // check mouse input
        if ( mouse ) {
            int mouse_status = mouse->Status();
            short mouse_x = DfMouseCol( mouse->X() );   // driver pixels -> cell
            short mouse_y = DfMouseRow( mouse->Y() );

            // a press EDGE only: down now, up on the previous poll
            if ( ( mouse_status & LEFT_BUTTON ) && !( old_mouse_status & LEFT_BUTTON ) ) {
                for ( i = 0; i < button_count; ++i ) {
                    if ( dlg_buttons[i].HitTest( mouse_x, mouse_y ) ) {
                        result = i;
                        done = 1;
                        break;
                    }
                }
            }

            old_mouse_status = mouse_status;
        }

        // check keyboard input
        if ( !done && kbhit() ) {
            int key = getch();
            int upper_key = toupper( key );

            // An arrow or a function key is TWO bytes, and the second is a
            // scan code that reads as a letter: Ins is 'R', End is 'O', Up is
            // 'H'.  Left in the buffer it was taken as the next key typed,
            // so Ins pressed a Retry button.  Take it out, if it is there
            // (getch() would wait for it - see App::PollEvent), and do
            // nothing with the pair.
            if ( key == 0 || key == 0xE0 ) {
                if ( kbhit() ) getch();
            }
            // Enter always selects the first button
            else if ( key == '\r' || key == '\n' ) {
                result = 0;
                done = 1;
            }
            // Escape selects the last button (cancel/no/back)
            else if ( key == _KB_ESC ) {
                result = (int)( button_count - 1 );
                done = 1;
            }
            else {
                // the shortcut letters worked out above - not simply the
                // first letter of each label, which two buttons can share
                for ( i = 0; i < button_count; ++i ) {
                    if ( button_keys[i] && upper_key == button_keys[i] ) {
                        result = i;
                        done = 1;
                        break;
                    }
                }
            }
        }
    } // end modal loop

    // restore screen
    RestoreWindow();

    // This box ran its own mouse loop, so the application's event loop never
    // saw those press and release edges.  Resync, or its next poll turns the
    // leftover state into a phantom click on whatever is under the pointer.
    DfModalFinished();

    return result;
}

/*---- The one-call version ------------------------------------------------- *
 * Most dialogs are "say this, wait for OK", and every program that wanted one
 * was building the same six lines.  Note that nothing here sets coordinates,
 * so Show() sizes the box around the text.
 *---------------------------------------------------------------------------*/
button_return_value_t DlgBox::Message( const foreground_color_t foreground,
                                       const background_color_t background,
                                       const char* title, const char* text,
                                       const dialog_buttons_t buttons,
                                       MSMouse* mouse )
{
    DlgBox dlg( foreground, background );

    dlg.buttons            = buttons;
    dlg.border_style       = BORDER_DOUBLE_LINE;
    dlg.text_justification = CENTER;
    dlg.drop_shadow        = TRUE;
    dlg.SetMessage( text );
    dlg.SetMouse( mouse );

    if ( title ) {
        strncpy( dlg.title, title, sizeof( dlg.title ) - 1 );
        dlg.title[ sizeof( dlg.title ) - 1 ] = '\0';
    }

    return dlg.Show();
}
