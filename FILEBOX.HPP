/*===========================================================================
 * FILEBOX.HPP - Modal file / folder chooser.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * A centered Popup with a TREE of the machine's drives and folders, a LIST
 * of the files in whichever folder the tree's highlight is on, a FIELD
 * holding a path, and [ OK ] / [ Cancel ].  Show() returns TRUE on accept
 * and Path() is then what the field said.
 *
 * ONE DIALOG, because choosing a file and choosing a folder are the same
 * walk with a different last step.  There used to be two - a FileBox that
 * listed one directory at a time, and a FolderBox with the tree - and the
 * only real difference between them was whether files were on show.
 *
 * THREE MODES, which say what is being chosen:
 *
 *   FB_OPEN   a file that exists.  The default.
 *   FB_SAVE   a file that need not exist: SetName suggests one.
 *   FB_DIR    a folder, which need not exist either - walk to where it
 *             should go and type "\NEW" on the end.
 *
 * AND THE FILE LIST CAN BE TURNED OFF.  It is on for a file and off for a
 * folder unless ShowFiles says otherwise; a folder chooser that shows the
 * files as well lets the user see what is in a folder before picking it,
 * and they are only there to be looked at.
 *
 * THE FIELD IS THE ANSWER, the tree and the list are ways of filling it in.
 * Enter and OK take what the field says.  When that is a FOLDER and a file
 * was wanted, the tree goes there instead - so a path can be typed as a way
 * of getting about.  Enter never opens the node under the highlight: a
 * dialog whose Enter did not mean OK would be a trap for anyone accepting
 * what it offered.
 *
 * KEYS
 *   Tab, Shift+Tab      round the tree, the file list and the field
 *   Up/Down/PgUp/PgDn   move in the tree or the list, whichever had the
 *                       keyboard last
 *   + or Right          (tree) open the folder under the highlight
 *   - or Left           (tree) shut it     Left  (list) back to the tree
 *   Home/End            top and bottom
 *   typing, Backspace   go to the field, and take the keyboard with them
 *   Enter               accept        Esc   cancel
 * With the keyboard in the field, Left/Right/Home/End/Del edit it.  The
 * field shows a cursor, and a list a solid bar, only while it has the
 * keyboard.
 *
 * MOUSE
 *   a folder            moves the highlight    [+] / [-]   open and shut
 *   a folder, twice     opens or shuts it      the field   puts the cursor
 *   a file              picks it               a file, twice   accepts it
 *
 * A DRIVE IS NOT READ UNTIL IT IS OPENED - not its folders and not its
 * files - so nothing happens to a floppy drive the highlight only passes
 * over.  When a drive that is opened has no disk in it the box says so and
 * offers Retry.  Show() holds a DfCritGuard (CRITERR.HPP) while the box is
 * up, so that is so whether or not the program has one of its own - though
 * a program without gets DOS's message for whatever it then does with the
 * path.  TestOpen and TestKey hold none: the harness has one in main.
 *
 * THE CURRENT DIRECTORY IS NOT MOVED.  Everything is read by full path.
 *
 * Built out of the toolkit: DirTree and DirScan for the models, ListBox for
 * both panes, TextField, Button, and DfCritAsk.  A build that includes
 * FILEBOX.CPP needs DIRTREE, DIRSCAN, LISTBOX, TEXTFLD, CRITERR, DLGBOX,
 * POPUP and BUTTON as well.
 *===========================================================================*/
#ifndef FILEBOX_HPP
#define FILEBOX_HPP

#include "popup.hpp"
#include "button.hpp"
#include "listbox.hpp"
#include "textfld.hpp"
#include "dirtree.hpp"
#include "dirscan.hpp"
#include "structs.hpp"

#define FB_PATHLEN  260
#define FB_NAMELEN   80

class FileBox : public Popup
{
public:
    enum { FB_OPEN = 0, FB_SAVE = 1, FB_DIR = 2 };
    enum { FB_AT_TREE = 0, FB_AT_FILES = 1, FB_AT_FIELD = 2 };

private:
    DirTree   tree;
    DirScan   files;
    ListBox   treeList;
    ListBox   fileList;
    TextField field;

    char prompt[128];
    char startPath[FB_PATHLEN];    // where to open the browser
    char name[FB_NAMELEN];         // FB_SAVE: the file name, without a folder
    char treePath[FB_PATHLEN];     // the folder the highlight is on
    char result[FB_PATHLEN];       // what Path() answers after an accept

    coordinates_t treeBox;         // the two panes, for the mouse
    coordinates_t fileBox;

    int  mode;
    int  wantFiles;                // -1 = as the mode says, else 0 / 1
    int  boxW, boxH;
    int  done;                     // 1 = accept, -1 = cancel, 0 = keep going
    int  pendingNode;              // a tree row to open or shut, or -1
    int  pendingFile;              // a file row to accept, or -1
    int  focus;                    // FB_AT_...
    int  lastList;                 // the pane the arrows go to from the field
    int  fileSel;                  // the file row the field was filled from
    int  driveOpen;                // was the highlighted drive open, last look
    int  asked;                    // a box of our own has been up since
    int  iconDrive, iconFolder, iconFile;

    Button btnOk;
    Button btnCancel;

    FileBox( const FileBox & );
    FileBox &operator=( const FileBox & );

    void Init( const char *promptText );
    boolean HasFiles( void ) const;
    const char *FieldLabel( void ) const;

    void Place( void );
    void LayOut( void );           // pane, field and button positions
    void Fill( void );             // build the tree and open it to startPath
    void Paint( void );            // everything inside the frame

    int  RevealPath( const char *path );    // the tree, asking on an error
    int  OpenNode( int row );
    void RefillFiles( void );
    void ComposeField( void );     // what the field says after the tree moves
    void KeepTypedName( void );
    void SetFocusTo( int where );
    void PaintFocus( void );
    void SyncFromTree( boolean force );
    void SyncFromFiles( boolean force );
    int  Look( const char *path, int *isFolder );
    void GoTo( const char *path );
    void Accept( void );
    void Key( int key );
    void Click( short x, short y, int buttons );
    void RunPending( void );

    static int  TreeDepthOf( void *user, int row );
    static int  TreeHasKids( void *user, int row );
    static int  TreeIsOpen( void *user, int row );
    static int  TreeToggled( void *user, int row );
    static void TreeName( void *user, int row, char *buf, int buflen );
    static void TreeTail( void *user, int row, char *buf, int buflen );
    static int  TreeIconOf( void *user, int row );
    static void TreeTwice( void *user, int row );
    static void FileText( void *user, int row, char *buf, int buflen );
    static int  FileIconOf( void *user, int row );
    static void FileTwice( void *user, int row );
    static int  NoMark( void *user, int row );

public:
    FileBox( const foreground_color_t foreground,
             const background_color_t background, const char *titleText );
    FileBox( const foreground_color_t foreground,
             const background_color_t background,
             const char *titleText, const char *promptText );

    void SetMode( int m );                      // FB_OPEN / FB_SAVE / FB_DIR
    void ShowFiles( boolean on );               // the file list, on or off
    void SetFilter( const char *patterns );     // "*.ZIP;*.7Z", or 0 for all

    /* Where the tree opens.  A path that does not exist all the way down is
     * fine: the tree opens as far as the part that does, and in FB_DIR the
     * field says all of it.  With none given, the current directory. */
    void SetStartDir( const char *dir );
    void SetPath( const char *dir ) { SetStartDir( dir ); }

    void SetName( const char *initial );        // FB_SAVE: the suggested name

    /* Icons for the rows, as DfIconRegister ids, on a screen that can draw
     * them.  Never asked for in text mode. */
    void SetIcons( int driveIcon, int folderIcon, int fileIcon );

    const char *Path( void ) const { return result; }

    boolean Show( void );          // TRUE = something was chosen

    /* Self-test only.  TestOpen draws what Show() draws, minus the modal
     * loop, so a headless harness can snapshot it (then call RestoreWindow).
     * TestKey hands one key to the code the loop hands keys to - ASCII, or
     * 0x100 | scan code, as in an Event - and TestClick one press of the
     * left button on a cell, so the harness can walk the tree and type in
     * the field and read back what that did. */
    void TestOpen( void );
    void TestKey( int key );
    void TestClick( short x, short y );
    const char *FieldText( void ) const { return field.Text(); }
    boolean FieldHasFocus( void ) const { return field.HasFocus(); }
    int  Focus( void ) const            { return focus; }
    int  TreeRow( void ) const          { return treeList.Selected(); }
    int  TreeRows( void ) const         { return treeList.Count(); }
    int  FileRow( void ) const          { return fileList.Selected(); }
    int  FileRows( void ) const         { return fileList.Count(); }
    int  Finished( void ) const         { return done; }
};

#endif
