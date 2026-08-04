/*===========================================================================
 * MENU.HPP  -  Menu bar with dropdown lists (top row, or 2nd row under a TitleBar)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/

#ifndef MENU_HPP
#define MENU_HPP

#include "dosforms.hpp"
#include "structs.hpp"
#include "popup.hpp"

// callback fired when a dropdown item is chosen
typedef void (*MenuAction)( void *user );

class Menu : public DOSForms
{
public:
    // Sizing constants.  These live in the public section deliberately: under
    // MSVC 1.52c's pre-standard access rules a nested class (Item / Column
    // below) gets NO access to the enclosing class's private members, so a
    // private enum here fails with C2248 when the nested structs size their
    // arrays with it.  Public sidesteps that and is harmless (they are just
    // dimensions).  Open Watcom follows the modern rule and accepts either.
    enum { MAX_COLUMNS = 8, MAX_ITEMS = 16, LABEL_LEN = 24 };

private:
    struct Item {
        char       label[LABEL_LEN];
        char       hotkey;
        MenuAction action;
        void      *user;
    };

    struct Column {
        char  label[LABEL_LEN];
        char  hotkey;
        short x;            // 0-based start column on the bar (computed)
        short width;        // label length in cells
        Item  items[MAX_ITEMS];
        short item_count;
    };

    Column columns[MAX_COLUMNS];
    short  column_count;
    short  bar_row;         // 0 (top) or 1 (under a TitleBar)
    short  active;          // open column index, or -1
    short  highlight;       // highlighted item index while a dropdown is open
    Popup  dropdown;        // frame + screen save/restore for the open list

    Menu( const Menu & );
    Menu &operator=( const Menu & );

    void Open( short which );
    void Close( void );
    void DrawItems( void );

public:
    Menu( void );
    Menu( const foreground_color_t foreground, const background_color_t background );

    short AddMenu( const char *label, char hotkey );   // returns the column index, or -1
    boolean AddItem( short menu, const char *label, char hotkey, MenuAction action, void *user );
    void SetRow( short row );    // 0 normally; 1 when a TitleBar occupies the top row

    virtual void SetMouse( MSMouse *m );          // also hands the mouse to the dropdown frame
    virtual void Draw( void );                    // draw the bar (the dropdown opens on demand)
    virtual boolean HandleEvent( const Event &e );
};

#endif
