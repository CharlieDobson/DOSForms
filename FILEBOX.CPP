/*===========================================================================
 * FILEBOX.CPP - Modal file / folder chooser implementation.  See FILEBOX.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "filebox.hpp"
#include "criterr.hpp"          // DfCritClear / DfCritHit / DfCritAsk
#include "msmouse.hpp"          // LEFT_BUTTON + MSMouse status for clicks
#include "moushide.hpp"         // hide the cursor while painting
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>             // _fullpath
#include <dos.h>                // _dos_getfileattr

#define FB_H            20
#define FB_W_TREE       60      /* the tree on its own                       */
#define FB_W_BOTH       76      /* the tree and the file list                */
#define FB_TREE_COLS    30      /* the tree's share of that, border included */

static int IsSep( char c )
{
    return ( c == '\\' || c == '/' );
}

/* What was typed, made fit to hand to DOS: no blanks round it, and no two
 * separators together - "C:\" with "\NEW" typed after it is "C:\\NEW", and
 * real DOS reads a doubled separator as the start of a network name.  A
 * pair at the very front is left alone, because there it IS one. */
static void TidyPath( char *dst, int size, const char *src )
{
    int used = 0;

    while ( *src == ' ' ) ++src;
    for ( ; *src && used < size - 1; ++src )
    {
        if ( IsSep( *src ) && used > 1 && IsSep( dst[used-1] ) ) continue;
        dst[used++] = *src;
    }
    while ( used > 0 && dst[used-1] == ' ' ) --used;
    dst[used] = '\0';
}

/* A folder and something in it, with one separator between them whether or
 * not the folder already ends in one - the root of a drive does. */
static void JoinPath( char *dst, int size, const char *dir, const char *leaf )
{
    int used = 0;

    if ( size <= 0 ) return;
    while ( dir[used] && used < size - 2 ) { dst[used] = dir[used]; ++used; }
    if ( used > 0 && !IsSep( dst[used-1] ) ) dst[used++] = '\\';
    while ( *leaf && used < size - 1 ) dst[used++] = *leaf++;
    dst[used] = '\0';
}

void FileBox::Init( const char *promptText )
{
    border_style       = BORDER_DOUBLE_LINE;
    drop_shadow        = TRUE;
    text_justification = CENTER;

    prompt[0] = startPath[0] = name[0] = treePath[0] = result[0] = '\0';
    treeBox.row = treeBox.column = treeBox.end_row = treeBox.end_column = 0;
    fileBox = treeBox;

    mode        = FB_OPEN;
    wantFiles   = -1;
    boxW        = FB_W_BOTH;
    boxH        = FB_H;
    done        = 0;
    pendingNode = -1;
    pendingFile = -1;
    focus       = FB_AT_TREE;
    lastList    = FB_AT_TREE;
    fileSel     = -1;
    driveOpen   = 1;
    asked       = 0;
    iconDrive   = DF_ICON_NONE;
    iconFolder  = DF_ICON_NONE;
    iconFile    = DF_ICON_NONE;

    if ( promptText )
    {
        strncpy( prompt, promptText, sizeof( prompt ) - 1 );
        prompt[sizeof( prompt ) - 1] = '\0';
    }

    files.SetFilesOnly( 1 );
    field.SetMaxLength( FB_PATHLEN - 1 );
}

FileBox::FileBox( const foreground_color_t foreground,
                  const background_color_t background, const char *titleText )
    : Popup( foreground, background, titleText ),
      treeList( FG_BLACK, BG_WHITE ),
      fileList( FG_BLACK, BG_WHITE ),
      field( FG_BRIGHT_WHITE, BG_BLUE )
{
    Init( 0 );
}

FileBox::FileBox( const foreground_color_t foreground,
                  const background_color_t background,
                  const char *titleText, const char *promptText )
    : Popup( foreground, background, titleText ),
      treeList( FG_BLACK, BG_WHITE ),
      fileList( FG_BLACK, BG_WHITE ),
      field( FG_BRIGHT_WHITE, BG_BLUE )
{
    Init( promptText );
}

void FileBox::SetMode( int m )            { mode = m; }
void FileBox::ShowFiles( boolean on )     { wantFiles = on ? 1 : 0; }
void FileBox::SetFilter( const char *patterns ) { files.SetFilter( patterns ); }

void FileBox::SetStartDir( const char *dir )
{
    startPath[0] = '\0';
    if ( !dir ) return;
    strncpy( startPath, dir, sizeof( startPath ) - 1 );
    startPath[sizeof( startPath ) - 1] = '\0';
}

void FileBox::SetName( const char *initial )
{
    name[0] = '\0';
    if ( !initial ) return;
    strncpy( name, initial, sizeof( name ) - 1 );
    name[sizeof( name ) - 1] = '\0';
}

void FileBox::SetIcons( int driveIcon, int folderIcon, int fileIcon )
{
    iconDrive  = driveIcon;
    iconFolder = folderIcon;
    iconFile   = fileIcon;
}

boolean FileBox::HasFiles( void ) const
{
    if ( wantFiles >= 0 ) return (boolean)( wantFiles ? TRUE : FALSE );
    return (boolean)( ( mode == FB_DIR ) ? FALSE : TRUE );
}

const char *FileBox::FieldLabel( void ) const
{
    return ( mode == FB_DIR ) ? "Folder:" : "File:";
}

/*---- Layout --------------------------------------------------------------- *
 * Rows inside the box, 0-based from its top border:
 *   +0            border, with the title on it
 *   +1            the prompt, when there is one
 *   +1/2 .. +h-5  the tree, and the file list beside it (bordered: a list
 *                 draws its own frame)
 *   +h-4          the field
 *   +h-3          blank
 *   +h-2          the buttons
 *   +h-1          border
 *---------------------------------------------------------------------------*/
void FileBox::Place( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short wide, high, left, top;
    int   want;

    /* Wide enough for both panes or for the one, then for the prompt, and
     * never past the screen. */
    want = HasFiles() ? FB_W_BOTH : FB_W_TREE;
    if ( (int)strlen( prompt ) + 6 > want ) want = (int)strlen( prompt ) + 6;
    if ( want > cols - 2 ) want = cols - 2;
    if ( want < 30 )       want = 30;
    wide = (short)want;

    /* Grow with the screen rather than staying the size a 25-row screen
     * needed: on the graphics renderer's thirty rows a twenty-row dialog is
     * a postage stamp, and the lists are what want the space. */
    high = (short)( rows - 4 );
    if ( high > FB_H + 6 ) high = FB_H + 6;
    if ( high < 12 ) high = 12;

    boxW = wide;
    boxH = high;
    left = (short)( ( cols - wide ) / 2 );
    top  = (short)( ( rows - high ) / 2 );
    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;

    SetCoordinates( top, left, (short)( top + high - 1 ), (short)( left + wide - 1 ) );
}

void FileBox::LayOut( void )
{
    short row0   = coordinates.row;
    short col0   = coordinates.column;
    short btnRow = (short)( coordinates.end_row - 1 );
    short fldRow = (short)( coordinates.end_row - 3 );
    short fldCol = (short)( col0 + 4 + (short)strlen( FieldLabel() ) );
    short okW  = 6;                 // "[ OK ]"
    short canW = 10;                // "[ Cancel ]"
    short gap  = 4;
    short start = (short)( col0 + ( boxW - ( okW + gap + canW ) ) / 2 );

    if ( start < col0 + 1 ) start = (short)( col0 + 1 );

    treeBox.row        = (short)( row0 + ( prompt[0] ? 2 : 1 ) );
    treeBox.column     = (short)( col0 + 2 );
    treeBox.end_row    = (short)( coordinates.end_row - 4 );
    treeBox.end_column = (short)( coordinates.end_column - 2 );

    fileBox = treeBox;
    if ( HasFiles() )
    {
        treeBox.end_column = (short)( treeBox.column + FB_TREE_COLS - 1 );
        fileBox.column     = (short)( treeBox.end_column + 1 );
    }

    treeList.SetMouse( mouse );
    treeList.SetRegion( treeBox.row, treeBox.column,
                        treeBox.end_row, treeBox.end_column );
    fileList.SetMouse( mouse );
    fileList.SetRegion( fileBox.row, fileBox.column,
                        fileBox.end_row, fileBox.end_column );

    field.SetMouse( mouse );
    field.SetPosition( fldRow, fldCol,
                       (short)( coordinates.end_column - 2 - fldCol ) );

    btnOk     = Button( FG_WHITE, BG_CYAN );
    btnCancel = Button( FG_WHITE, BG_CYAN );
    btnOk.SetMouse( mouse );
    btnCancel.SetMouse( mouse );
    strcpy( btnOk.label,     "OK" );
    strcpy( btnCancel.label, "Cancel" );
    btnOk.SetSingleRow( (unsigned short)btnRow, (unsigned short)start );
    btnCancel.SetSingleRow( (unsigned short)btnRow,
                            (unsigned short)( start + okW + gap ) );
}

/*---- The models feed the lists -------------------------------------------- */

int FileBox::TreeDepthOf( void *user, int row )
{
    FileBox *box = (FileBox *)user;
    return box ? box->tree.Depth( row ) : 0;
}

int FileBox::TreeHasKids( void *user, int row )
{
    FileBox *box = (FileBox *)user;
    return box ? box->tree.HasKids( row ) : 0;
}

int FileBox::TreeIsOpen( void *user, int row )
{
    FileBox *box = (FileBox *)user;
    return box ? box->tree.IsOpen( row ) : 0;
}

int FileBox::TreeToggled( void *user, int row )
{
    FileBox *box = (FileBox *)user;

    if ( !box ) return 0;
    if ( box->tree.IsOpen( row ) ) return box->tree.Close( row );
    return box->OpenNode( row );
}

void FileBox::TreeName( void *user, int row, char *buf, int buflen )
{
    FileBox *box = (FileBox *)user;

    if ( buflen < 1 ) return;
    buf[0] = '\0';
    if ( !box ) return;
    strncpy( buf, box->tree.Name( row ), (size_t)buflen - 1 );
    buf[buflen - 1] = '\0';
}

/* The columns to the right of the name.  A folder tree has none. */
void FileBox::TreeTail( void *user, int row, char *buf, int buflen )
{
    (void)user; (void)row;
    if ( buflen > 0 ) buf[0] = '\0';
}

int FileBox::TreeIconOf( void *user, int row )
{
    FileBox *box = (FileBox *)user;

    if ( !box ) return DF_ICON_NONE;
    return box->tree.IsDrive( row ) ? box->iconDrive : box->iconFolder;
}

void FileBox::TreeTwice( void *user, int row )
{
    FileBox *box = (FileBox *)user;
    if ( box ) box->pendingNode = row;  // acted on after HandleEvent returns
}

/* Name, size and the day it was written.  Column 0 is left blank for the
 * list's own mark glyph or icon - that is the widget's rule, not ours. */
void FileBox::FileText( void *user, int row, char *buf, int buflen )
{
    FileBox *box = (FileBox *)user;
    char     size[16];
    char     date[20];

    if ( buflen < 1 ) return;
    buf[0] = '\0';
    if ( !box || buflen < 40 ) return;

    sprintf( size, "%lu", box->files.Size( row ) );
    box->files.Date( row, date, sizeof( date ) );
    sprintf( buf, " %-12.12s %10.10s  %-10.10s",
             box->files.Name( row ), size, date );
}

int FileBox::FileIconOf( void *user, int row )
{
    FileBox *box = (FileBox *)user;

    (void)row;
    return box ? box->iconFile : DF_ICON_NONE;
}

void FileBox::FileTwice( void *user, int row )
{
    FileBox *box = (FileBox *)user;
    if ( box ) box->pendingFile = row;
}

/* Nothing here can be marked: one thing is being chosen, and it is the one
 * under the highlight. */
int FileBox::NoMark( void *user, int row )
{
    (void)user; (void)row;
    return 0;
}

/*---- Reading the disk, and what to do when it will not be read ------------ *
 * Each of these makes the call, and if a critical error was the reason it
 * failed says so and offers to make it again.  Show() holds the guard that
 * makes sure there is a handler to report one.
 *---------------------------------------------------------------------------*/
int FileBox::RevealPath( const char *path )
{
    int row;

    for ( ;; )
    {
        DfCritClear();
        row = tree.Reveal( path );
        if ( !DfCritHit() ) break;
        asked = 1;
        if ( !DfCritAsk( mouse ) ) break;
    }
    return row;
}

int FileBox::OpenNode( int row )
{
    int rows;

    for ( ;; )
    {
        DfCritClear();
        rows = tree.Open( row );
        if ( !DfCritHit() ) break;
        asked = 1;
        if ( !DfCritAsk( mouse ) ) break;
    }
    return rows;
}

/* The files of the folder the highlight is on.  NOT of a drive that has
 * not been opened: passing the highlight over A: must not spin it up, let
 * alone put a box on the screen about it. */
void FileBox::RefillFiles( void )
{
    int row = treeList.Selected();

    fileSel = -1;
    if ( !HasFiles() ) return;

    if ( !treePath[0] || ( tree.IsDrive( row ) && !tree.IsOpen( row ) ) )
    {
        files.Empty();
    }
    else
    {
        for ( ;; )
        {
            DfCritClear();
            files.ScanPath( treePath );
            if ( !DfCritHit() ) break;
            asked = 1;
            if ( !DfCritAsk( mouse ) ) { files.Empty(); break; }
        }
    }

    fileList.SetData( files.Count(), FileText, this );
    fileList.SetMarkable( NoMark );
    fileList.SetDblClick( FileTwice );
    fileList.SetIconProvider( ( iconFile >= 0 ) ? FileIconOf : 0 );
    fileList.SelectFirst();
}

/* Does 'path' exist, and is it a folder?  The root of a drive is taken to
 * be one without looking - looking is a read, and whoever goes there next
 * will do the reading and the asking. */
int FileBox::Look( const char *path, int *isFolder )
{
    char     bare[FB_PATHLEN];
    unsigned attr = 0;
    int      len, found;

    *isFolder = 0;
    strncpy( bare, path, sizeof( bare ) - 1 );
    bare[sizeof( bare ) - 1] = '\0';
    len = (int)strlen( bare );
    while ( len > 0 && IsSep( bare[len-1] ) ) bare[--len] = '\0';

    if ( len == 0 || ( len == 2 && bare[1] == ':' ) )
    {
        *isFolder = 1;
        return 1;
    }

    for ( ;; )
    {
        DfCritClear();
        found = ( _dos_getfileattr( bare, &attr ) == 0 ) ? 1 : 0;
        if ( !DfCritHit() ) break;
        asked = 1;
        if ( !DfCritAsk( mouse ) ) { found = 0; break; }
    }

    if ( !found ) return 0;
    *isFolder = ( attr & _A_SUBDIR ) ? 1 : 0;
    return 1;
}

void FileBox::Fill( void )
{
    char full[FB_PATHLEN];
    int  row;
    int  nameWidth = treeBox.end_column - treeBox.column - 2;

    treeList.SetData( tree.Build(), TreeTail, this );
    treeList.SetTreeProvider( TreeDepthOf, TreeHasKids, TreeIsOpen, TreeToggled,
                              TreeName, nameWidth );
    treeList.SetMarkable( NoMark );
    treeList.SetDblClick( TreeTwice );
    treeList.SetIconProvider( ( iconDrive >= 0 || iconFolder >= 0 )
                              ? TreeIconOf : 0 );

    row = RevealPath( startPath[0] ? startPath : "." );
    treeList.SetCount( tree.Count() );
    if ( row < 0 ) row = 0;
    treeList.Select( row );

    tree.FullPath( row, treePath, sizeof( treePath ) );
    driveOpen = tree.IsDrive( row ) ? tree.IsOpen( row ) : 1;
    RefillFiles();

    if ( mode == FB_DIR )
    {
        /* The field says what the caller asked for, in full.  Where the
         * tree got all the way there the two are the same path and the
         * tree's spelling of it is used, which is the one DOS gave us. */
        full[0] = '\0';
        if ( startPath[0] && !_fullpath( full, startPath, sizeof( full ) ) )
        {
            strcpy( full, startPath );
        }
        if ( !full[0] || stricmp( full, treePath ) == 0 ) strcpy( full, treePath );
        field.SetText( full );
    }
    else
    {
        ComposeField();
    }

    /* The keyboard starts where the choosing will be done: among the files
     * when one is being opened, in the field when one is being named, and
     * in the tree when it is a folder that is wanted. */
    focus    = FB_AT_TREE;
    lastList = FB_AT_TREE;
    if ( mode == FB_SAVE ) focus = FB_AT_FIELD;
    if ( mode == FB_OPEN && HasFiles() && fileList.Count() > 0 )
    {
        focus    = FB_AT_FILES;
        lastList = FB_AT_FILES;
        SyncFromFiles( TRUE );
    }
    field.SetFocus( (boolean)( ( focus == FB_AT_FIELD ) ? TRUE : FALSE ) );
}

/* A list has a solid bar while the keyboard is in it and coloured text
 * while it is not: the row is still marked, and it is plain which of the
 * two the arrows will move. */
void FileBox::PaintFocus( void )
{
    if ( focus == FB_AT_TREE ) treeList.SetSelHighlight( FG_BLACK, BG_CYAN );
    else                       treeList.SetSelHighlight( FG_BLUE,  BG_WHITE );

    if ( focus == FB_AT_FILES ) fileList.SetSelHighlight( FG_BLACK, BG_CYAN );
    else                        fileList.SetSelHighlight( FG_BLUE,  BG_WHITE );

    MouseHide guard( mouse );
    treeList.Draw();
    if ( HasFiles() ) fileList.Draw();
    field.Draw();
}

void FileBox::Paint( void )
{
    char shown[128];
    int  room = boxW - 6;

    MouseHide guard( mouse );

    if ( prompt[0] && room > 0 )
    {
        if ( room > (int)sizeof( shown ) - 1 ) room = (int)sizeof( shown ) - 1;
        strncpy( shown, prompt, (size_t)room );
        shown[room] = '\0';
        DfTextOut( (short)( coordinates.row + 2 ), (short)( coordinates.column + 4 ),
                   shown, fg_color, bg_color );
    }

    DfTextOut( (short)( coordinates.end_row - 2 ), (short)( coordinates.column + 4 ),
               FieldLabel(), fg_color, bg_color );
    PaintFocus();
    btnOk.Draw();
    btnCancel.Draw();
}

/*---- Who has the keyboard ------------------------------------------------- */

void FileBox::SetFocusTo( int where )
{
    if ( where == FB_AT_FILES && ( !HasFiles() || fileList.Count() < 1 ) )
    {
        where = FB_AT_TREE;
    }
    if ( where == focus ) return;

    focus = where;
    if ( where != FB_AT_FIELD ) lastList = where;
    field.SetFocus( (boolean)( ( where == FB_AT_FIELD ) ? TRUE : FALSE ) );

    /* Arriving among the files picks the one under the highlight, so that
     * what Enter would take is what the bar is on. */
    if ( where == FB_AT_FILES ) SyncFromFiles( TRUE );
    PaintFocus();
}

/* What the field says once the tree has moved: the folder, the folder ready
 * to have a name typed after it, or the folder and the name being saved. */
void FileBox::ComposeField( void )
{
    char text[FB_PATHLEN];

    if ( mode == FB_DIR )       strcpy( text, treePath );
    else if ( mode == FB_OPEN ) JoinPath( text, sizeof( text ), treePath, "" );
    else                        JoinPath( text, sizeof( text ), treePath, name );
    field.SetText( text );
}

/* FB_SAVE: a name typed into the field goes along when the folder changes.
 * Whatever follows the last separator is the name, unless the whole field
 * is just the folder we are leaving. */
void FileBox::KeepTypedName( void )
{
    const char *text = field.Text();
    const char *leaf = text;
    const char *at;

    if ( !text[0] || stricmp( text, treePath ) == 0 ) return;

    for ( at = text; *at; ++at )
    {
        if ( IsSep( *at ) || *at == ':' ) leaf = at + 1;
    }
    if ( !leaf[0] ) return;

    strncpy( name, leaf, sizeof( name ) - 1 );
    name[sizeof( name ) - 1] = '\0';
}

/* The highlight is on a different folder from the one the field was last
 * filled from: say so in the field, and list that folder's files.  Opening
 * or shutting a node moves nothing, so it leaves alone whatever has been
 * typed - except that opening a DRIVE is what lets its files be read. */
void FileBox::SyncFromTree( boolean force )
{
    char now[FB_PATHLEN];
    int  row  = treeList.Selected();
    int  open = tree.IsDrive( row ) ? tree.IsOpen( row ) : 1;
    int  moved;

    tree.FullPath( row, now, sizeof( now ) );
    if ( !now[0] ) return;

    moved = ( force || strcmp( now, treePath ) != 0 ) ? 1 : 0;
    if ( !moved && open == driveOpen ) return;

    if ( moved && mode == FB_SAVE ) KeepTypedName();

    strcpy( treePath, now );
    driveOpen = open;

    if ( HasFiles() )
    {
        RefillFiles();
        fileList.Draw();
    }
    if ( moved )
    {
        ComposeField();
        field.Draw();
    }
}

void FileBox::SyncFromFiles( boolean force )
{
    char text[FB_PATHLEN];
    int  row = fileList.Selected();

    if ( mode == FB_DIR ) return;       // there to be looked at, no more
    if ( fileList.Count() < 1 ) return;
    if ( !force && row == fileSel ) return;

    fileSel = row;
    strncpy( name, files.Name( row ), sizeof( name ) - 1 );
    name[sizeof( name ) - 1] = '\0';

    JoinPath( text, sizeof( text ), treePath, name );
    field.SetText( text );
    field.Draw();
}

/* Take the tree to a folder that was typed. */
void FileBox::GoTo( const char *path )
{
    int row = RevealPath( path );

    treeList.SetCount( tree.Count() );
    if ( row >= 0 ) treeList.Select( row );

    field.SetText( "" );        // it named the folder, not a file to keep
    SyncFromTree( TRUE );

    MouseHide guard( mouse );
    treeList.Draw();
}

void FileBox::Accept( void )
{
    char text[FB_PATHLEN];
    char full[FB_PATHLEN];
    int  isFolder = 0, there, len;

    TidyPath( text, sizeof( text ), field.Text() );
    if ( !text[0] ) return;             // an empty field is not an answer

    if ( mode == FB_DIR )
    {
        strcpy( result, text );
        done = 1;
        return;
    }

    /* A name on its own belongs to the folder on show. */
    if ( text[1] == ':' || IsSep( text[0] ) ) strcpy( full, text );
    else JoinPath( full, sizeof( full ), treePath, text );

    there = Look( full, &isFolder );
    if ( there && isFolder )
    {
        GoTo( full );
        return;
    }

    len = (int)strlen( full );
    if ( len < 1 || IsSep( full[len-1] ) ) return;      // a folder, not there
    if ( mode == FB_OPEN && !there ) return;            // nothing to open

    strcpy( result, full );
    done = 1;
}

void FileBox::RunPending( void )
{
    if ( pendingNode >= 0 )
    {
        int row = pendingNode;

        pendingNode = -1;
        treeList.ToggleNode( row );
        SyncFromTree( FALSE );
    }

    if ( pendingFile >= 0 )
    {
        pendingFile = -1;
        if ( mode != FB_DIR )
        {
            SyncFromFiles( TRUE );
            Accept();
        }
    }
}

/* One key, as an Event spells it: ASCII, or 0x100 | scan code. */
void FileBox::Key( int key )
{
    Event ev;
    int   canFiles = ( HasFiles() && fileList.Count() > 0 ) ? 1 : 0;

    ev.kind    = Event::EV_KEY;
    ev.key     = key;
    ev.x = ev.y = 0;
    ev.buttons = 0;

    switch ( key )
    {
    case KEY_ESC:   done = -1;  return;
    case KEY_ENTER: Accept();   return;

    case 9:
        if ( focus == FB_AT_TREE )       SetFocusTo( canFiles ? FB_AT_FILES : FB_AT_FIELD );
        else if ( focus == FB_AT_FILES ) SetFocusTo( FB_AT_FIELD );
        else                             SetFocusTo( FB_AT_TREE );
        return;

    case ( 0x100 | _KB_BACK_TAB ):
        if ( focus == FB_AT_TREE )       SetFocusTo( FB_AT_FIELD );
        else if ( focus == FB_AT_FILES ) SetFocusTo( FB_AT_TREE );
        else                             SetFocusTo( canFiles ? FB_AT_FILES : FB_AT_TREE );
        return;

    /* These mean nothing to a one-line field, so from there they go to the
     * list that had the keyboard last - and take it back. */
    case KEY_UP:
    case KEY_DOWN:
    case ( 0x100 | _KB_PGUP ):
    case ( 0x100 | _KB_PGDN ):
        if ( focus == FB_AT_FIELD ) SetFocusTo( lastList );
        if ( focus == FB_AT_FILES )
        {
            fileList.HandleEvent( ev );
            SyncFromFiles( FALSE );
        }
        else
        {
            treeList.HandleEvent( ev );
            SyncFromTree( FALSE );
        }
        return;

    default:
        break;
    }

    if ( focus == FB_AT_FIELD )
    {
        field.HandleEvent( ev );
        return;
    }

    if ( focus == FB_AT_TREE )
    {
        switch ( key )
        {
        case KEY_LEFT:
        case KEY_RIGHT:
        case ( 0x100 | _KB_HOME ):
        case ( 0x100 | _KB_END ):
        case '+':
        case '-':
            treeList.HandleEvent( ev );
            SyncFromTree( FALSE );
            return;

        default:
            break;
        }
    }
    else
    {
        switch ( key )
        {
        case ( 0x100 | _KB_HOME ):
        case ( 0x100 | _KB_END ):
            fileList.HandleEvent( ev );
            SyncFromFiles( FALSE );
            return;

        case KEY_LEFT:  SetFocusTo( FB_AT_TREE );  return;
        case KEY_RIGHT: return;

        default:
            break;
        }
    }

    /* Anything a field can use is typing, and typing belongs in the field:
     * walk to a folder, type "\NEW", and the keyboard has followed. */
    if ( key == 8 || key == ( 0x100 | _KB_DEL ) || ( key >= 32 && key <= 126 ) )
    {
        SetFocusTo( FB_AT_FIELD );
        field.HandleEvent( ev );
    }
}

static int Inside( const coordinates_t &box, short x, short y )
{
    return ( y >= box.row && y <= box.end_row &&
             x >= box.column && x <= box.end_column );
}

void FileBox::Click( short x, short y, int buttons )
{
    Event ev;

    if ( btnCancel.HitTest( x, y ) ) { done = -1; return; }
    if ( btnOk.HitTest( x, y ) )     { Accept();  return; }

    ev.kind    = Event::EV_MOUSE;
    ev.key     = 0;
    ev.x       = x;
    ev.y       = y;
    ev.buttons = buttons;

    if ( field.HitTest( x, y ) )
    {
        SetFocusTo( FB_AT_FIELD );
        field.HandleEvent( ev );
        return;
    }

    if ( Inside( treeBox, x, y ) )
    {
        SetFocusTo( FB_AT_TREE );
        treeList.HandleEvent( ev );
        SyncFromTree( FALSE );
        return;
    }

    if ( HasFiles() && Inside( fileBox, x, y ) )
    {
        SetFocusTo( FB_AT_FILES );
        fileList.HandleEvent( ev );
        SyncFromFiles( FALSE );
    }
}

/*---- Showing it ----------------------------------------------------------- */

void FileBox::TestOpen( void )
{
    result[0]   = '\0';
    done        = 0;
    pendingNode = -1;
    pendingFile = -1;

    Place();
    LayOut();
    Fill();

    MouseHide guard( mouse );
    Draw();
    Paint();
}

void FileBox::TestKey( int key )
{
    Key( key );
    RunPending();
}

void FileBox::TestClick( short x, short y )
{
    Click( x, y, LEFT_BUTTON );
    RunPending();
}

boolean FileBox::Show( void )
{
    int lastB;

    if ( !IsTextMode() ) return FALSE;

    /* Ours for as long as the box is up, if the program has none. */
    DfCritGuard critical;

    result[0]   = '\0';
    done        = 0;
    pendingNode = -1;
    pendingFile = -1;

    /* The disk is read BEFORE anything is painted, so the box arrives whole
     * rather than as an empty frame waiting on a drive. */
    Place();
    LayOut();
    Fill();

    {
        MouseHide guard( mouse );
        Draw();
        Paint();
    }

    /* Headless harness: it has nobody to type, so it looks and leaves. */
    if ( DfModalPeek() )
    {
        DfModalPeek()();
        RestoreWindow();
        DfModalFinished();
        return FALSE;
    }

    /* Seed the edge detector from the LIVE button state - the click that
     * opened this is usually still held. */
    lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;
    asked = 0;

    while ( !done )
    {
        if ( kbhit() )
        {
            int key = getch();

            if ( key == 0 || key == 0xE0 )
            {
                /* The second half of an extended key, asked for only if it
                 * is there: getch() blocks, and a lone first half - a TSR
                 * can leave one - would stop the box dead and then eat the
                 * next key pressed.  See App::PollEvent. */
                if ( !kbhit() ) continue;
                key = 0x100 | ( getch() & 0xFF );
            }
            Key( key );
            RunPending();
        }
        else if ( mouse && mouse->IsInited() )
        {
            int now = mouse->Status();

            if ( ( now & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
            {
                Click( DfMouseCol( mouse->X() ), DfMouseRow( mouse->Y() ), now );
                RunPending();
            }
            lastB = now;
        }

        /* A box of our own - "No disk in drive" - ran its own mouse loop on
         * top of this one, and the button that answered it may still be
         * down.  Start again from how things are. */
        if ( asked )
        {
            asked = 0;
            lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;
        }
    }

    RestoreWindow();
    DfModalFinished();          /* see DOSFORMS.HPP */
    return (boolean)( ( done == 1 && result[0] ) ? TRUE : FALSE );
}
