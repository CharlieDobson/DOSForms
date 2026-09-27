/*===========================================================================
 * FILEBOX.HPP - Modal file / folder chooser.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * A centered Popup showing the path being browsed, a scrolling list of
 * drives, subdirectories and files, and [ OK ] / [ Cancel ].  Show() saves
 * the screen, runs its own event loop, restores the screen, and returns TRUE
 * when something was chosen; Path() is then the full path of it.
 *
 *   - Up/Down/PgUp/PgDn/Home/End move the highlight
 *   - Enter, a double-click, or OK on a folder or drive opens it
 *   - Enter, a double-click, or OK on a file accepts it
 *   - Esc or Cancel aborts
 *
 * THREE MODES, because "pick a file" is not the only question a program
 * asks:
 *
 *   FB_OPEN   choose a file that exists.  The default.
 *   FB_SAVE   choose a file that need not exist: there is a NAME FIELD as
 *             well as the list, the list fills it in when a row is picked,
 *             and OK accepts whatever the field says.
 *   FB_DIR    choose a FOLDER.  No files are listed at all and OK accepts
 *             the directory currently open, which is the one thing a folder
 *             picker has to get right - you are choosing where you ARE.
 *
 * IT IS BUILT OUT OF THE TOOLKIT, not out of its own copies of things: a
 * DirScan for the model, a ListBox for the list, Buttons for the buttons,
 * and an InputBox' worth of field handling for the name.  A build that
 * includes FILEBOX.CPP needs DIRSCAN.CPP and LISTBOX.CPP too.
 *
 * WHERE YOU WERE IS PUT BACK.  Browsing changes the process's drive and
 * directory - that is how DirScan works, and it is what makes "where am I?"
 * answerable - so Show() remembers both on the way in and restores them on
 * the way out.  A dialog that moved the program's own idea of the current
 * directory would be a surprising thing to open.
 *===========================================================================*/
#ifndef FILEBOX_HPP
#define FILEBOX_HPP

#include "popup.hpp"
#include "button.hpp"
#include "listbox.hpp"
#include "dirscan.hpp"
#include "structs.hpp"

#define FB_PATHLEN  260
#define FB_NAMELEN   80

class FileBox : public Popup
{
public:
    enum { FB_OPEN = 0, FB_SAVE = 1, FB_DIR = 2 };

private:
    DirScan scan;
    ListBox list;

    char curPath[FB_PATHLEN];      // directory currently listed
    char result[FB_PATHLEN];       // what Path() answers after an accept
    char startDir[FB_PATHLEN];     // where to open the browser
    char name[FB_NAMELEN];         // FB_SAVE: the typed file name
    int  nameLen;
    int  nameCur;                  // cursor within 'name'

    int  mode;
    int  boxW, boxH;
    int  done;                     // 1 = accept, -1 = cancel, 0 = keep going
    int  pending;                  // a row the list asked us to act on, or -1

    Button btnOk;
    Button btnCancel;

    FileBox( const FileBox & );
    FileBox &operator=( const FileBox & );

    void Place( void );
    void LayOut( void );           // list region + button positions
    void DrawPathRow( void );
    void DrawNameRow( void );
    void Refill( void );           // rescan -> feed the list
    void Accept( int row );        // Enter / double-click / OK on a row
    void AcceptName( void );       // FB_SAVE: take whatever the field says
    void AcceptDir( void );        // FB_DIR: take the directory we are in
    void NameInsert( char c );
    void NameDelete( int at );

    static void RowText( void *user, int index, char *buf, int buflen );
    static void RowChosen( void *user, int index );

public:
    FileBox( const foreground_color_t foreground,
             const background_color_t background, const char *titleText );

    void SetMode( int m );                      // FB_OPEN / FB_SAVE / FB_DIR
    void SetFilter( const char *patterns );     // "*.ZIP;*.7Z", or 0 for all
    void SetStartDir( const char *dir );
    void SetName( const char *initial );        // FB_SAVE: the suggested name

    const char *Path( void ) const { return result; }

    boolean Show( void );          // TRUE = something was chosen

    /* Self-test only: draw what Show() draws, minus the modal loop, so a
     * headless harness can snapshot it (then call RestoreWindow). */
    void TestOpen( void );
};

#endif
