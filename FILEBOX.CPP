/*===========================================================================
 * FILEBOX.CPP - Modal file / folder chooser implementation.  See FILEBOX.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "filebox.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON + MSMouse status for clicks
#include "moushide.hpp"         // hide the cursor while painting
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <direct.h>             // getcwd, chdir
#include <dos.h>                // _dos_getdrive, _dos_setdrive

#define FB_W        60
#define FB_H        20

FileBox::FileBox( const foreground_color_t foreground,
                  const background_color_t background, const char *titleText )
    : Popup( foreground, background, titleText ),
      list( FG_BLACK, BG_WHITE )
{
    border_style       = BORDER_DOUBLE_LINE;
    drop_shadow        = TRUE;
    text_justification = CENTER;

    curPath[0] = result[0] = startDir[0] = '\0';
    name[0]  = '\0';
    nameLen  = 0;
    nameCur  = 0;
    mode     = FB_OPEN;
    boxW     = FB_W;
    boxH     = FB_H;
    done     = 0;
    pending  = -1;

    list.SetSelHighlight( FG_BLACK, BG_CYAN );
}

void FileBox::SetMode( int m )
{
    mode = m;
    scan.SetDirsOnly( ( m == FB_DIR ) ? 1 : 0 );
}

void FileBox::SetFilter( const char *patterns )
{
    scan.SetFilter( patterns );
}

void FileBox::SetStartDir( const char *dir )
{
    startDir[0] = '\0';
    if ( !dir ) return;
    strncpy( startDir, dir, sizeof( startDir ) - 1 );
    startDir[sizeof( startDir ) - 1] = '\0';
}

void FileBox::SetName( const char *initial )
{
    name[0] = '\0';
    if ( initial )
    {
        strncpy( name, initial, sizeof( name ) - 1 );
        name[sizeof( name ) - 1] = '\0';
    }
    nameLen = (int)strlen( name );
    nameCur = nameLen;
}

/*---- Layout --------------------------------------------------------------- *
 * Rows inside the box, 0-based from its top border:
 *   +0            border, with the title on it
 *   +1            the path being browsed
 *   +2 .. +h-4    the list (bordered: it draws its own frame)
 *   +h-3          the name field, in FB_SAVE - blank otherwise
 *   +h-2          the buttons
 *   +h-1          border
 *---------------------------------------------------------------------------*/
void FileBox::Place( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;
    short w = (short)boxW, h;
    short left, top;

    if ( w > cols - 2 ) w = (short)( cols - 2 );
    if ( w < 30 ) w = 30;

    /* Grow with the screen rather than staying the size a 25-row screen
     * needed: on the graphics renderer's thirty rows a twenty-row dialog is
     * a postage stamp, and the list is the thing that wants the space. */
    h = (short)( rows - 4 );
    if ( h > FB_H + 6 ) h = FB_H + 6;
    if ( h < 12 ) h = 12;

    boxW = w;
    boxH = h;
    left = (short)( ( cols - w ) / 2 );
    top  = (short)( ( rows - h ) / 2 );
    if ( left < 0 ) left = 0;
    if ( top  < 0 ) top  = 0;

    SetCoordinates( top, left, (short)( top + h - 1 ), (short)( left + w - 1 ) );
}

void FileBox::LayOut( void )
{
    short row0 = coordinates.row;
    short col0 = coordinates.column;
    short btnRow = (short)( coordinates.end_row - 1 );
    short okW  = 6;                 // "[ OK ]"
    short canW = 10;                // "[ Cancel ]"
    short gap  = 2;
    short start = (short)( col0 + ( boxW - ( okW + gap + canW ) ) / 2 );

    if ( start < col0 + 1 ) start = (short)( col0 + 1 );

    list.SetMouse( mouse );
    list.SetRegion( (short)( row0 + 2 ), (short)( col0 + 1 ),
                    (short)( coordinates.end_row - 3 ),
                    (short)( coordinates.end_column - 1 ) );

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

/* The path being browsed, cut from the LEFT when it is too long: the end of
 * a path is the part that says where you are. */
void FileBox::DrawPathRow( void )
{
    char line[FB_PATHLEN + 4];
    int  room = boxW - 4;
    int  n;
    const char *p = scan.IsDriveList() ? "Drives" : scan.Path();

    if ( room < 4 ) room = 4;
    if ( room > (int)sizeof( line ) - 1 ) room = (int)sizeof( line ) - 1;

    n = (int)strlen( p );
    if ( n <= room ) strcpy( line, p );
    else
    {
        strcpy( line, "..." );
        strcpy( line + 3, p + n - ( room - 3 ) );
    }
    for ( n = (int)strlen( line ); n < room; ++n ) line[n] = ' ';
    line[room] = '\0';

    DfTextOut( (short)( coordinates.row + 2 ), (short)( coordinates.column + 3 ),
               line, FG_BRIGHT_WHITE, bg_color );
}

/* FB_SAVE only: the editable file name, with the cursor cell inverted - the
 * same look as InputBox's field, because it is the same thing. */
void FileBox::DrawNameRow( void )
{
    char line[FB_NAMELEN + 4];
    char cell[2];
    int  room = boxW - 10;
    int  i;

    if ( mode != FB_SAVE ) return;
    if ( room < 8 ) room = 8;
    if ( room > (int)sizeof( line ) - 1 ) room = (int)sizeof( line ) - 1;

    DfTextOut( (short)( coordinates.end_row - 2 ),
               (short)( coordinates.column + 3 ), "Name:", fg_color, bg_color );

    for ( i = 0; i < room; ++i ) line[i] = ( i < nameLen ) ? name[i] : ' ';
    line[room] = '\0';
    DfTextOut( (short)( coordinates.end_row - 2 ),
               (short)( coordinates.column + 9 ), line, FG_BLACK, BG_WHITE );

    if ( nameCur >= 0 && nameCur < room )
    {
        cell[0] = ( nameCur < nameLen ) ? name[nameCur] : ' ';
        cell[1] = '\0';
        DfTextOut( (short)( coordinates.end_row - 2 ),
                   (short)( coordinates.column + 9 + nameCur ),
                   cell, FG_WHITE, BG_BLACK );
    }
}

/*---- The model feeds the list --------------------------------------------- */

void FileBox::RowText( void *user, int index, char *buf, int buflen )
{
    FileBox *fb = (FileBox *)user;
    char     size[16];
    char     date[20];

    buf[0] = '\0';
    if ( !fb || buflen < 8 ) return;

    /* Column 0 is left blank for the list's own mark glyph - that is the
     * widget's rule, not ours. */
    if ( fb->scan.Kind( index ) == DS_FILE )
    {
        sprintf( size, "%lu", fb->scan.Size( index ) );
        fb->scan.Date( index, date, sizeof( date ) );
        sprintf( buf, " %-14.14s %10.10s  %-16.16s",
                 fb->scan.Disp( index ), size, date );
    }
    else
        sprintf( buf, " %-14.14s", fb->scan.Disp( index ) );

    buf[buflen - 1] = '\0';
}

void FileBox::RowChosen( void *user, int index )
{
    FileBox *fb = (FileBox *)user;
    if ( fb ) fb->pending = index;      // acted on after HandleEvent returns
}

void FileBox::Refill( void )
{
    scan.Rescan();
    strncpy( curPath, scan.Path(), sizeof( curPath ) - 1 );
    curPath[sizeof( curPath ) - 1] = '\0';

    list.SetData( scan.Count(), RowText, this );
    list.SetActivate( RowChosen );
    list.SetDblClick( RowChosen );
    list.SelectFirst();
}

/*---- Accepting ------------------------------------------------------------ */

void FileBox::Accept( int row )
{
    if ( row < 0 || row >= scan.Count() ) return;

    /* A directory-like row is a MOVE, not a choice - in every mode.  Even a
     * folder picker has to be able to walk into a folder before choosing it,
     * which is why OK on a folder opens it and OK on nothing chooses where
     * you are. */
    if ( scan.IsDir( row ) )
    {
        scan.Enter( row );
        Refill();
        {
            MouseHide guard( mouse );
            DrawPathRow();
            list.Draw();
        }
        return;
    }

    if ( mode == FB_DIR ) return;       // files are not choosable here

    scan.FullName( row, result, sizeof( result ) );
    if ( mode == FB_SAVE )
    {
        strncpy( name, scan.Name( row ), sizeof( name ) - 1 );
        name[sizeof( name ) - 1] = '\0';
        nameLen = (int)strlen( name );
        nameCur = nameLen;
        {
            MouseHide guard( mouse );
            DrawNameRow();
        }
        return;                         // picked, not yet accepted: OK does that
    }
    done = 1;
}

void FileBox::AcceptName( void )
{
    const char *dir = scan.Path();
    int         n;

    if ( nameLen < 1 ) return;
    name[nameLen] = '\0';

    /* A name with a drive or a separator in it is a path already and is
     * taken as typed; anything else is relative to the directory on screen. */
    if ( name[1] == ':' || name[0] == '\\' || name[0] == '/' )
    {
        strncpy( result, name, sizeof( result ) - 1 );
        result[sizeof( result ) - 1] = '\0';
        done = 1;
        return;
    }

    if ( scan.IsDriveList() ) return;   // no directory to put it in

    strncpy( result, dir, sizeof( result ) - 2 );
    result[sizeof( result ) - 2] = '\0';
    n = (int)strlen( result );
    if ( n > 0 && result[n-1] != '\\' && result[n-1] != '/' ) result[n++] = '\\';
    result[n] = '\0';
    strncat( result, name, sizeof( result ) - 1 - n );
    done = 1;
}

void FileBox::AcceptDir( void )
{
    if ( scan.IsDriveList() ) return;   // "Drives" is not a folder
    strncpy( result, scan.Path(), sizeof( result ) - 1 );
    result[sizeof( result ) - 1] = '\0';
    done = 1;
}

void FileBox::NameInsert( char c )
{
    int i;

    if ( nameLen >= (int)sizeof( name ) - 1 ) return;
    for ( i = nameLen; i > nameCur; --i ) name[i] = name[i-1];
    name[nameCur] = c;
    ++nameLen; ++nameCur;
    name[nameLen] = '\0';
}

void FileBox::NameDelete( int at )
{
    int i;

    if ( at < 0 || at >= nameLen ) return;
    for ( i = at; i < nameLen - 1; ++i ) name[i] = name[i+1];
    --nameLen;
    name[nameLen] = '\0';
}

/*---- Showing it ----------------------------------------------------------- */

void FileBox::TestOpen( void )
{
    Place();
    Draw();
    LayOut();
    if ( startDir[0] ) chdir( startDir );
    Refill();

    MouseHide guard( mouse );
    DrawPathRow();
    list.Draw();
    DrawNameRow();
    btnOk.Draw();
    btnCancel.Draw();
}

boolean FileBox::Show( void )
{
    char     backDir[FB_PATHLEN];
    unsigned backDrive = 0, n;
    int      lastB;

    if ( !IsTextMode() ) return FALSE;

    /* Where we were, so we can put it back.  Both halves: the drive is not
     * part of getcwd's answer on the drive we end up on. */
    _dos_getdrive( &backDrive );
    if ( !getcwd( backDir, sizeof( backDir ) ) ) backDir[0] = '\0';

    result[0] = '\0';
    done      = 0;
    pending   = -1;

    Place();
    Draw();
    LayOut();

    if ( startDir[0] )
    {
        if ( startDir[1] == ':' )
            _dos_setdrive( (unsigned)( toupper( (unsigned char)startDir[0] ) - 'A' + 1 ), &n );
        chdir( startDir );
    }
    Refill();

    {
        MouseHide guard( mouse );
        DrawPathRow();
        list.Draw();
        DrawNameRow();
        btnOk.Draw();
        btnCancel.Draw();
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

    while ( !done )
    {
        Event ev;

        if ( kbhit() )
        {
            int k = getch();

            if ( k == 27 ) { done = -1; break; }
            if ( k == 0 || k == 0xE0 )
            {
                int ext = getch();

                /* The name field takes the editing keys in FB_SAVE; the list
                 * takes the movement keys in every mode. */
                if ( mode == FB_SAVE &&
                     ( ext == _KB_LEFT || ext == _KB_RIGHT || ext == _KB_DEL ) )
                {
                    if ( ext == _KB_LEFT  && nameCur > 0 )       --nameCur;
                    if ( ext == _KB_RIGHT && nameCur < nameLen ) ++nameCur;
                    if ( ext == _KB_DEL )                        NameDelete( nameCur );
                    MouseHide guard( mouse );
                    DrawNameRow();
                    continue;
                }

                ev.kind = Event::EV_KEY;
                ev.key  = 0x100 | ext;
                ev.x = ev.y = 0;
                ev.buttons = 0;
                list.HandleEvent( ev );
                continue;
            }

            if ( k == 13 )
            {
                /* Enter means the list's current row - which may be a move
                 * rather than a choice - except in FB_SAVE with something
                 * typed, where it means the field. */
                if ( mode == FB_SAVE && nameLen > 0 &&
                     !scan.IsDir( list.Selected() ) )
                    AcceptName();
                else
                    Accept( list.Selected() );
                continue;
            }

            if ( mode == FB_SAVE )
            {
                if ( k == 8 )
                {
                    if ( nameCur > 0 ) { NameDelete( nameCur - 1 ); --nameCur; }
                    MouseHide guard( mouse );
                    DrawNameRow();
                    continue;
                }
                if ( k >= 32 && k < 127 )
                {
                    NameInsert( (char)k );
                    MouseHide guard( mouse );
                    DrawNameRow();
                    continue;
                }
            }
            continue;
        }

        if ( mouse && mouse->IsInited() )
        {
            int b = mouse->Status();
            if ( ( b & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
            {
                short mx = DfMouseCol( mouse->X() );
                short my = DfMouseRow( mouse->Y() );

                if ( btnCancel.HitTest( mx, my ) ) { done = -1; lastB = b; break; }
                if ( btnOk.HitTest( mx, my ) )
                {
                    if ( mode == FB_DIR )            AcceptDir();
                    else if ( mode == FB_SAVE && nameLen > 0 ) AcceptName();
                    else                             Accept( list.Selected() );
                    lastB = b;
                    continue;
                }

                ev.kind    = Event::EV_MOUSE;
                ev.key     = 0;
                ev.x       = mx;
                ev.y       = my;
                ev.buttons = b;
                list.HandleEvent( ev );
            }
            lastB = b;
        }

        /* A row the list handed back through its activate/double-click
         * callback.  Acted on HERE rather than inside the callback, because
         * acting on it rescans and redraws the very list that is mid-event. */
        if ( pending >= 0 )
        {
            int row = pending;
            pending = -1;
            Accept( row );
        }
    }

    RestoreWindow();

    /* Put the process back where it was standing. */
    if ( backDrive ) _dos_setdrive( backDrive, &n );
    if ( backDir[0] ) chdir( backDir );

    DfModalFinished();          /* see DOSFORMS.HPP */
    return (boolean)( ( done == 1 && result[0] ) ? TRUE : FALSE );
}
