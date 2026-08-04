/*===========================================================================
 * MENU.CPP  -  Menu bar with dropdown lists
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// Menu class implementation file

#include <graph.h>              // MSVC/Watcom text output
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
    highlight = 0;
}

Menu::Menu( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    column_count = 0;
    bar_row = 0;
    active = -1;
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

    _settextcolor( fg_color );
    _setbkcolor( bg_color );
    for ( short i = 0; i < column_count; ++i ) {
        _settextposition( bar_row + 1, columns[i].x + 1 );   // 0-based -> 1-based
        _outtext( columns[i].label );
    }
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

        if ( i == highlight ) {                  // selected row: swap fg/bg
            _settextcolor( (short)bg_color );
            _setbkcolor( (long)fg_color );
        } else {
            _settextcolor( fg_color );
            _setbkcolor( bg_color );
        }
        _settextposition( top + 2 + i, column + 2 );   // inside the border, 1-based
        _outtext( line );
    }
}

void Menu::Open( short which )
{
    if ( which < 0 || which >= column_count ) return;
    if ( active >= 0 ) Close();

    active = which;
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
        if ( active >= 0 ) {
            if ( e.key == KEY_ESC ) { Close(); return TRUE; }

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
