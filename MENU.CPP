/*===========================================================================
 * MENU.CPP  -  Menu bar with dropdown lists
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Menu class implementation file

#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>             // strncpy, strlen
#include <ctype.h>              // toupper
#include "menu.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON
#include "moushide.hpp"         // MouseHide cursor guard

Menu::Menu( void ) : DOSForms( FG_BLACK, BG_WHITE )
{
    column_count = 0;
    bar_row = 0;
    active = -1;
    bar_focus = -1;
    highlight = 0;
}

Menu::Menu( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    column_count = 0;
    bar_row = 0;
    active = -1;
    bar_focus = -1;
    highlight = 0;
}

short Menu::AddMenu( const char *label, char hotkey )
{
    if ( column_count >= MAX_COLUMNS || !label ) return -1;

    Column *c = &columns[column_count];
    strncpy( c->label, label, LABEL_LEN - 1 );
    c->label[LABEL_LEN - 1] = '\0';
    c->hotkey = hotkey;
    c->item_count = 0;
    c->width = (short)strlen( c->label );
    // lay the column out after the previous one, with a gap
    c->x = ( column_count == 0 )
           ? 1
           : columns[column_count - 1].x + columns[column_count - 1].width + 2;
    return column_count++;
}

boolean Menu::AddItem( short menu, const char *label, char hotkey, MenuAction action, void *user )
{
    if ( menu < 0 || menu >= column_count || !label ) return FALSE;

    Column *c = &columns[menu];
    if ( c->item_count >= MAX_ITEMS ) return FALSE;

    Item *it = &c->items[c->item_count];
    strncpy( it->label, label, LABEL_LEN - 1 );
    it->label[LABEL_LEN - 1] = '\0';
    it->hotkey = hotkey;
    it->action = action;
    it->user = user;
    c->item_count++;
    return TRUE;
}

boolean Menu::SetItemLabel( short menu, short item, const char *label )
{
    if ( menu < 0 || menu >= column_count || !label ) return FALSE;

    Column *c = &columns[menu];
    if ( item < 0 || item >= c->item_count ) return FALSE;

    strncpy( c->items[item].label, label, LABEL_LEN - 1 );
    c->items[item].label[LABEL_LEN - 1] = '\0';
    return TRUE;
}

short Menu::ItemCount( short menu ) const
{
    if ( menu < 0 || menu >= column_count ) return 0;
    return columns[menu].item_count;
}

void Menu::SetRow( short row )
{
    bar_row = row;
}

void Menu::SetMouse( MSMouse *m )
{
    DOSForms::SetMouse( m );
    dropdown.SetMouse( m );      // so the dropdown hides the cursor while drawing too
}

void Menu::Draw( void )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    ClearRow( bar_row + 1 );     // clear the bar row to the menu colors (1-based)

    // The column with the keyboard is drawn in reverse video, whether it has
    // a dropdown open under it or is only focused: without that, a bare Alt
    // would put the keyboard somewhere the user cannot see.
    short lit = ( active >= 0 ) ? active : bar_focus;

    for ( short i = 0; i < column_count; ++i ) {
        // the lit column is the colour pair swapped: reverse video
        foreground_color_t f = ( i == lit ) ? (foreground_color_t)bg_color : fg_color;
        background_color_t b = ( i == lit ) ? (background_color_t)fg_color : bg_color;
        DfTextOut( (short)( bar_row + 1 ), (short)( columns[i].x + 1 ),   // 0-based -> 1-based
                   columns[i].label, f, b );
    }
}

// Highlight a bar column with no dropdown under it.  Redraws the bar rather
// than just the two columns that changed: the bar is one row, and one _outtext
// per column is cheaper than working out which two moved.
void Menu::FocusBar( short which )
{
    if ( column_count <= 0 ) return;
    if ( which < 0 ) which = (short)( column_count - 1 );
    if ( which >= column_count ) which = 0;
    if ( active >= 0 ) Close();
    bar_focus = which;
    Draw();
}

void Menu::LeaveBar( void )
{
    if ( bar_focus < 0 ) return;
    bar_focus = -1;
    Draw();
}

void Menu::ActivateBar( void )
{
    // A second bare Alt while the bar already has the keyboard puts it back,
    // which is what every other menu bar does and what makes Alt safe to
    // press by accident.
    if ( active >= 0 )    { Close(); Draw(); return; }
    if ( bar_focus >= 0 ) { LeaveBar(); return; }
    FocusBar( 0 );
}

void Menu::DrawItems( void )
{
    MouseHide guard( mouse );

    short top      = dropdown.coordinates.row;       // 0-based box top
    short column   = dropdown.coordinates.column;
    short interior = dropdown.coordinates.end_column - column - 1;   // cells between borders

    char line[LABEL_LEN + 8];
    if ( interior > (short)sizeof( line ) - 1 ) interior = sizeof( line ) - 1;

    for ( short i = 0; i < columns[active].item_count; ++i ) {
        const char *lbl = columns[active].items[i].label;
        short j;

        // a full-width row (leading pad + label) so the highlight bar fills it
        for ( j = 0; j < interior; ++j ) line[j] = ' ';
        line[interior] = '\0';
        for ( j = 0; lbl[j] != '\0' && j + 1 < interior; ++j ) line[j + 1] = lbl[j];

        // selected row: swap fg/bg
        foreground_color_t f = ( i == highlight ) ? (foreground_color_t)bg_color : fg_color;
        background_color_t b = ( i == highlight ) ? (background_color_t)fg_color : bg_color;
        DfTextOut( (short)( top + 2 + i ), (short)( column + 2 ),   // inside the border, 1-based
                   line, f, b );
    }
}

void Menu::Open( short which )
{
    if ( which < 0 || which >= column_count ) return;
    if ( active >= 0 ) Close();

    active = which;
    bar_focus = -1;         // 'active' is now what the bar highlights
    highlight = 0;
    Column *c = &columns[which];

    // widest item label sets the box width
    short w = 1;
    for ( short i = 0; i < c->item_count; ++i ) {
        short len = (short)strlen( c->items[i].label );
        if ( len > w ) w = len;
    }

    short top    = bar_row + 1;                 // row just below the bar (0-based)
    short left   = c->x;
    short right  = left + w + 3;                // 2 borders + 2 pad
    short bottom = top + c->item_count + 1;     // items + top/bottom border

    dropdown.fg_color = fg_color;
    dropdown.bg_color = bg_color;
    dropdown.border_style = BORDER_SINGLE_LINE;
    dropdown.drop_shadow = TRUE;
    dropdown.title[0] = '\0';
    dropdown.SetCoordinates( top, left, bottom, right );
    dropdown.Draw();        // saves the screen underneath + draws the frame
    DrawItems();            // list the items inside
}

void Menu::Close( void )
{
    if ( active < 0 ) return;
    dropdown.RestoreWindow();   // put back whatever was under the dropdown
    active = -1;
    // The bar row is above the dropdown, so RestoreWindow never touches it:
    // the highlight on the column that was open has to be painted out by
    // hand, or it stays lit under a menu that is no longer there.
    Draw();
}

// Map App::Run's extended encoding (0x100 | scancode) for an Alt+letter
// keystroke to its uppercase letter; returns 0 for anything else.
static char AltKeyToChar( int key )
{
    switch ( key & 0xFF ) {
        case 0x1E: return 'A';  case 0x30: return 'B';  case 0x2E: return 'C';
        case 0x20: return 'D';  case 0x12: return 'E';  case 0x21: return 'F';
        case 0x22: return 'G';  case 0x23: return 'H';  case 0x17: return 'I';
        case 0x24: return 'J';  case 0x25: return 'K';  case 0x26: return 'L';
        case 0x32: return 'M';  case 0x31: return 'N';  case 0x18: return 'O';
        case 0x19: return 'P';  case 0x10: return 'Q';  case 0x13: return 'R';
        case 0x1F: return 'S';  case 0x14: return 'T';  case 0x16: return 'U';
        case 0x2F: return 'V';  case 0x11: return 'W';  case 0x2D: return 'X';
        case 0x15: return 'Y';  case 0x2C: return 'Z';
        default:   return 0;
    }
}

boolean Menu::HandleEvent( const Event &e )
{
    if ( e.kind == Event::EV_KEY ) {
        // A bare Alt reaches the bar from anywhere - it is the one keystroke
        // whose whole purpose is to take the keyboard off whatever has it.
        if ( e.key == KEY_ALT ) { ActivateBar(); return TRUE; }

        // The bar has the keyboard but nothing is dropped yet.  Checked before
        // the open-dropdown arm below because the two states are exclusive and
        // this one is the shorter answer.
        if ( bar_focus >= 0 && active < 0 ) {
            if ( e.key == KEY_ESC ) { LeaveBar(); return TRUE; }
            if ( e.key == KEY_LEFT ) {
                FocusBar( (short)( bar_focus - 1 ) );
                return TRUE;
            }
            if ( e.key == KEY_RIGHT ) {
                FocusBar( (short)( bar_focus + 1 ) );
                return TRUE;
            }
            if ( e.key == KEY_ENTER || e.key == KEY_DOWN ) {
                Open( bar_focus );
                return TRUE;
            }
            // A letter picks the column with that hotkey and opens it, the
            // same letters Alt+letter uses - once the bar has the keyboard the
            // Alt is no longer needed to say where a letter is aimed.
            if ( e.key > 0 && e.key < 256 ) {
                for ( short i = 0; i < column_count; ++i ) {
                    if ( columns[i].hotkey != '\0'
                         && toupper( e.key ) == toupper( columns[i].hotkey ) ) {
                        Open( i );
                        return TRUE;
                    }
                }
            }
            // Alt+letter is still Alt+letter with the bar focused, and opens
            // its column as it does from anywhere else.  It used to fall
            // through to "anything else" below, so Alt+D, Esc, Alt+F left
            // the bar and opened nothing.
            if ( e.key >= 0x100 ) {
                char alt = AltKeyToChar( e.key );
                if ( alt != 0 ) {
                    for ( short i = 0; i < column_count; ++i ) {
                        if ( columns[i].hotkey != '\0'
                             && (char)toupper( columns[i].hotkey ) == alt ) {
                            Open( i );
                            return TRUE;
                        }
                    }
                }
            }
            // Anything else gives the bar up and is passed on, so a user who
            // pressed Alt by mistake is not trapped in a menu.
            LeaveBar();
            return FALSE;
        }

        if ( active >= 0 ) {
            // Esc backs out one step: the dropdown closes and the bar keeps
            // the keyboard, so the next Esc is what leaves the menu.
            if ( e.key == KEY_ESC ) {
                short was = active;
                Close();
                FocusBar( was );
                return TRUE;
            }

            // Left/Right move to the adjacent dropdown (wrapping), the usual
            // menu-bar behaviour once a menu is open
            if ( e.key == KEY_LEFT ) {
                Open( ( active > 0 ) ? active - 1 : column_count - 1 );
                return TRUE;
            }
            if ( e.key == KEY_RIGHT ) {
                Open( ( active < column_count - 1 ) ? active + 1 : 0 );
                return TRUE;
            }

            if ( e.key == KEY_UP ) {
                highlight = ( highlight > 0 ) ? highlight - 1
                                              : columns[active].item_count - 1;
                DrawItems();
                return TRUE;
            }
            if ( e.key == KEY_DOWN ) {
                highlight = ( highlight < columns[active].item_count - 1 ) ? highlight + 1 : 0;
                DrawItems();
                return TRUE;
            }
            if ( e.key == KEY_ENTER ) {
                if ( highlight >= 0 && highlight < columns[active].item_count ) {
                    Item it = columns[active].items[highlight];
                    Close();
                    if ( it.action ) it.action( it.user );
                }
                return TRUE;
            }

            // a plain-letter item hotkey selects it
            if ( e.key > 0 && e.key < 256 ) {
                for ( short i = 0; i < columns[active].item_count; ++i ) {
                    char hk = columns[active].items[i].hotkey;
                    if ( hk != '\0' && toupper( e.key ) == toupper( hk ) ) {
                        Item it = columns[active].items[i];
                        Close();
                        if ( it.action ) it.action( it.user );
                        return TRUE;
                    }
                }
            }
        }

        // Alt+letter opens (or switches to) the matching column, open or not
        if ( e.key >= 0x100 ) {
            char alt = AltKeyToChar( e.key );
            if ( alt != 0 ) {
                for ( short i = 0; i < column_count; ++i ) {
                    if ( columns[i].hotkey != '\0'
                         && (char)toupper( columns[i].hotkey ) == alt ) {
                        if ( active == i ) Close(); else Open( i );
                        return TRUE;
                    }
                }
            }
        }
        return FALSE;
    }

    // Any non-left button pressed while a dropdown is open dismisses it, just
    // as a left click outside does.  Without this the dropdown stays up while
    // the click goes to the widget underneath - which then draws over it, and
    // the eventual RestoreWindow puts the STALE content back.  (Reachable now
    // that a widget can act on the right button: XArchive marks list rows with
    // it.)  Left is left to the arms below, which have work to do first.
    if ( e.kind == Event::EV_MOUSE && active >= 0 &&
         ( e.buttons & ( RIGHT_BUTTON | CENTER_BUTTON ) ) ) {
        Close();
        return TRUE;
    }

    // The bar has the keyboard with nothing dropped and the user reached for
    // the mouse instead: give the keyboard up, but let the click through to
    // whatever it was aimed at (returning TRUE here would eat the first click
    // after every stray Alt).
    if ( e.kind == Event::EV_MOUSE && bar_focus >= 0 && active < 0 &&
         e.buttons != 0 && e.y != bar_row ) {
        LeaveBar();
    }

    if ( e.kind == Event::EV_MOUSE && ( e.buttons & LEFT_BUTTON ) ) {
        // click on a column label in the bar
        if ( e.y == bar_row ) {
            for ( short i = 0; i < column_count; ++i ) {
                if ( e.x >= columns[i].x && e.x < columns[i].x + columns[i].width ) {
                    if ( active == i ) Close(); else Open( i );
                    return TRUE;
                }
            }
        }

        // click inside an open dropdown
        if ( active >= 0 ) {
            short top   = dropdown.coordinates.row;
            short left  = dropdown.coordinates.column;
            short right = dropdown.coordinates.end_column;
            for ( short i = 0; i < columns[active].item_count; ++i ) {
                if ( e.y == top + 1 + i && e.x > left && e.x < right ) {
                    Item it = columns[active].items[i];
                    Close();
                    if ( it.action ) it.action( it.user );
                    return TRUE;
                }
            }
            Close();        // a click anywhere else dismisses the menu
            return TRUE;
        }
    }
    return FALSE;
}
