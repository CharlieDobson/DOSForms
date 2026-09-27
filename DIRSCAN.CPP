/*===========================================================================
 * DIRSCAN.CPP - A directory as a list of rows.  See DIRSCAN.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "dirscan.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <direct.h>             // getcwd, chdir
#include <dos.h>                // _dos_findfirst, _dos_setdrive, union REGS
#ifdef __386__
#include <i86.h>                // int386 (the IOCTL drive probe)
#endif

static int EntryCmp( const void *a, const void *b )
{
    const DsEntry *ea = (const DsEntry *)a;
    const DsEntry *eb = (const DsEntry *)b;
    if ( ea->kind != eb->kind ) return ea->kind - eb->kind;
    return stricmp( ea->name, eb->name );
}

/* Is a drive letter present?  INT 21h/44h/08h (IOCTL "is this block device
 * removable") asks the device DRIVER and does NOT read the media - so probing
 * A: and B: does not spin a floppy up, the way _dos_getdiskfree would.  That
 * matters because a browser rescans on every navigation, not once when a
 * dialog opens.
 *   drive1 = 1 for A:, 2 for B:, ...   Returns 1 if the drive exists.
 *
 * Two versions of the same call.  The 32-bit one goes through the extender's
 * int386; the 16-bit one is a plain int86 with the register pair named
 * instead of the extended one.  Everything else about the call is identical,
 * including what the answers mean. */
static int DriveExists( int drive1 )
{
    union REGS r;

#ifdef __386__
    r.x.eax = 0x4408;
    r.x.ebx = (unsigned)drive1;             /* BL = drive (1=A:) */
    int386( 0x21, &r, &r );
#else
    r.h.ah = 0x44;
    r.h.al = 0x08;
    r.h.bl = (unsigned char)drive1;         /* BL = drive (1=A:) */
    int86( 0x21, &r, &r );
#endif

    if ( r.x.cflag )                         /* carry: error */
        return ( r.w.ax != 0x000F );         /* 0Fh = invalid drive; anything
                                                else (01h = IOCTL unsupported)
                                                means the drive is there */
    return 1;                                /* AX = 0 removable / 1 fixed */
}

/*---- The filter ----------------------------------------------------------- *
 * One pattern at a time, '*' and '?' as DOS means them, matched against the
 * whole name (extension included) rather than against a split 8.3 pair - so
 * "*.ZIP" and "README.*" and "A?.TXT" all work and none of them needs a
 * special case.  Case-insensitive, because the file system is.
 *---------------------------------------------------------------------------*/
static int GlobMatch( const char *pat, const char *name )
{
    while ( *pat )
    {
        if ( *pat == '*' )
        {
            ++pat;
            if ( !*pat ) return 1;              /* trailing '*' eats the rest */
            while ( *name )
            {
                if ( GlobMatch( pat, name ) ) return 1;
                ++name;
            }
            return GlobMatch( pat, name );      /* and the empty tail */
        }
        if ( !*name ) return 0;
        if ( *pat != '?' &&
             toupper( (unsigned char)*pat ) != toupper( (unsigned char)*name ) )
            return 0;
        ++pat; ++name;
    }
    return ( *name == '\0' );
}

int DirScan::Accept( const char *name ) const
{
    const char *p = filter;
    char        one[DS_FILTERLEN];
    int         n;

    if ( !filter[0] ) return 1;             /* no filter: everything */

    while ( *p )
    {
        n = 0;
        while ( *p && *p != ';' && n < (int)sizeof( one ) - 1 ) one[n++] = *p++;
        one[n] = '\0';
        while ( *p == ';' ) ++p;
        if ( n > 0 && GlobMatch( one, name ) ) return 1;
    }
    return 0;
}

DirScan::DirScan()
{
    count   = 0;
    path[0] = '\0';
    drives  = 0;
    filter[0] = '\0';
    dirsOnly = 0;
    ents = (DsEntry *)malloc( (size_t)DS_MAX_ENTRIES * sizeof( DsEntry ) );
}

DirScan::~DirScan()
{
    if ( ents ) free( ents );
}

void DirScan::SetFilter( const char *patterns )
{
    filter[0] = '\0';
    if ( !patterns ) return;
    strncpy( filter, patterns, sizeof( filter ) - 1 );
    filter[sizeof( filter ) - 1] = '\0';
}

void DirScan::SetDirsOnly( int on )
{
    dirsOnly = on ? 1 : 0;
}

/* Is 'path' the root of a drive - "C:\" and nothing more? */
static int PathIsRoot( const char *p )
{
    return ( p[0] && p[1] == ':' && p[2] == '\\' && p[3] == '\0' );
}

int DirScan::Rescan( void )
{
    struct find_t f;
    unsigned      d;

    count = 0;
    if ( !ents ) return 0;          /* no room for a listing; say so as none */

    /* getcwd failing means the drive we are standing on has gone away - an
     * empty floppy is the usual way - and there is no directory to list.
     * Fall back to the drive list rather than drawing an empty window: it is
     * the one view that is always available and always somewhere to go from. */
    if ( !drives && !getcwd( path, sizeof( path ) ) ) drives = 1;

    if ( drives )
    {
        path[0] = '\0';                  /* there is no path above a root */
        for ( d = 1; d <= 26 && count < DS_MAX_ENTRIES; ++d )
        {
            if ( DriveExists( (int)d ) )
            {
                char letter = (char)( 'A' + d - 1 );
                sprintf( ents[count].disp, "[-%c-]", letter );
                sprintf( ents[count].name, "%c:", letter );
                ents[count].kind   = DS_DRIVE;
                ents[count].size   = 0;
                ents[count].date   = 0;
                ents[count].time   = 0;
                ents[count].attrib = 0;
                ++count;
            }
        }
        qsort( ents, count, sizeof( DsEntry ), EntryCmp );
        return count;
    }

    /* The way up is on every directory INCLUDING a root, where it leads to
     * the drive list rather than to another directory.  That is the whole
     * reason drives are no longer repeated in each listing: there is one
     * place to find them and one move that gets there. */
    if ( count < DS_MAX_ENTRIES )
    {
        strcpy( ents[count].disp, ".." );
        strcpy( ents[count].name, ".." );
        ents[count].kind   = DS_PARENT;
        ents[count].size   = 0;
        ents[count].date   = 0;
        ents[count].time   = 0;
        ents[count].attrib = 0;
        ++count;
    }

    /* EVERYTHING IS ASKED FOR, and the filter decides what is kept - the
     * wildcard goes to Accept rather than to DOS, because a caller's filter
     * is a list of patterns ("*.ZIP;*.7Z") and findfirst takes one.
     *
     * HIDDEN IS THE ONE THING NEVER LISTED, filter or no filter: the
     * things wearing that bit on a DOS machine are IO.SYS, MSDOS.SYS, a swap
     * file and a volume tracker, none of which this program has any business
     * with.  _A_HIDDEN is absent from the mask, which is what excludes them -
     * DOS returns a file only when every attribute bit it carries is one the
     * mask asked for, so hidden+system files go with it.  _A_SYSTEM stays, so
     * a file marked system and NOT hidden is still listed; that is a real if
     * rare thing, and hiding it was not what was asked for. */
    if ( _dos_findfirst( "*.*", _A_SUBDIR | _A_SYSTEM | _A_RDONLY, &f ) == 0 )
    {
        do
        {
            if ( count >= DS_MAX_ENTRIES ) break;
            if ( f.attrib & _A_SUBDIR )
            {
                if ( f.name[0] == '.' ) continue;      /* skip . and ..       */
                sprintf( ents[count].disp, "[%.13s]", f.name );
                ents[count].kind = DS_DIR;
            }
            else
            {
                /* The two ways a file can be left out: a folder picker wants
                 * none of them, and a filter wants only some. */
                if ( dirsOnly ) continue;
                if ( !Accept( f.name ) ) continue;
                strncpy( ents[count].disp, f.name, sizeof( ents[count].disp ) - 1 );
                ents[count].disp[sizeof( ents[count].disp ) - 1] = '\0';
                ents[count].kind = DS_FILE;
            }
            strncpy( ents[count].name, f.name, sizeof( ents[count].name ) - 1 );
            ents[count].name[sizeof( ents[count].name ) - 1] = '\0';
            ents[count].size   = (unsigned long)f.size;
            ents[count].date   = f.wr_date;
            ents[count].time   = f.wr_time;
            ents[count].attrib = f.attrib;
            ++count;
        } while ( _dos_findnext( &f ) == 0 );
    }

    qsort( ents, count, sizeof( DsEntry ), EntryCmp );
    return count;
}

int DirScan::ItemCount( void ) const
{
    int i, n = 0;
    for ( i = 0; i < count; i++ )
        if ( ents[i].kind != DS_PARENT ) n++;
    return n;
}

int DirScan::Kind( int i ) const
{
    return ( i >= 0 && i < count ) ? ents[i].kind : DS_FILE;
}

const char *DirScan::Disp( int i ) const
{
    return ( i >= 0 && i < count ) ? ents[i].disp : "";
}

const char *DirScan::Name( int i ) const
{
    return ( i >= 0 && i < count ) ? ents[i].name : "";
}

unsigned long DirScan::Size( int i ) const
{
    return ( i >= 0 && i < count ) ? ents[i].size : 0UL;
}

int DirScan::IsDir( int i ) const
{
    int k = Kind( i );
    return ( k == DS_PARENT || k == DS_DRIVE || k == DS_DIR );
}

/* DOS packed date/time -> "YYYY-MM-DD HH:MM", the same shape ArcEntryDate
 * writes, so the column reads the same in both halves of the window. */
void DirScan::Date( int i, char *buf, int buflen ) const
{
    unsigned dt, tm;

    if ( buflen <= 0 ) return;
    buf[0] = '\0';
    if ( i < 0 || i >= count || ents[i].kind != DS_FILE ) return;

    dt = ents[i].date;
    tm = ents[i].time;
    if ( !dt ) return;
    if ( buflen < 17 ) return;

    sprintf( buf, "%04d-%02d-%02d %02d:%02d",
             1980 + ( ( dt >> 9 ) & 0x7F ), ( dt >> 5 ) & 0x0F, dt & 0x1F,
             ( tm >> 11 ) & 0x1F, ( tm >> 5 ) & 0x3F );
}

void DirScan::Attr( int i, char *buf, int buflen ) const
{
    int n = 0;
    unsigned a;

    if ( buflen <= 0 ) return;
    buf[0] = '\0';
    if ( i < 0 || i >= count ) return;

    a = ents[i].attrib;
    if ( ( a & _A_SUBDIR ) && n < buflen - 1 ) buf[n++] = 'D';
    if ( ( a & _A_RDONLY ) && n < buflen - 1 ) buf[n++] = 'R';
    if ( ( a & _A_HIDDEN ) && n < buflen - 1 ) buf[n++] = 'H';
    if ( ( a & _A_SYSTEM ) && n < buflen - 1 ) buf[n++] = 'S';
    if ( ( a & _A_ARCH   ) && n < buflen - 1 ) buf[n++] = 'A';
    buf[n] = '\0';
}

void DirScan::FullName( int i, char *dst, int size ) const
{
    int n = 0;

    if ( size <= 0 ) return;
    dst[0] = '\0';
    if ( i < 0 || i >= count ) return;

    /* A drive-list row is not a file and has no directory to sit in; its own
     * name ("C:") is the whole of the answer. */
    if ( drives )
    {
        while ( ents[i].name[n] && n < size - 1 ) { dst[n] = ents[i].name[n]; n++; }
        dst[n] = '\0';
        return;
    }

    while ( path[n] && n < size - 2 ) { dst[n] = path[n]; n++; }
    /* "C:\" already ends in one; "C:\SUB" does not.  Same rule, and the same
     * reason, as every path builder in this suite: a doubled separator is
     * read as the start of a UNC name by real DOS. */
    if ( n > 0 && dst[n-1] != '\\' && dst[n-1] != '/' ) dst[n++] = '\\';
    { int j = 0; while ( ents[i].name[j] && n < size - 1 ) dst[n++] = ents[i].name[j++]; }
    dst[n] = '\0';
}

int DirScan::Enter( int i )
{
    char before[DS_PATHLEN];
    int  wasDrives = drives;

    if ( i < 0 || i >= count || !IsDir( i ) ) return 0;

    if ( !getcwd( before, sizeof( before ) ) ) before[0] = '\0';

    switch ( ents[i].kind )
    {
    case DS_PARENT:
        /* At a root the parent is the drive list, not another directory:
         * chdir("..") there succeeds and goes nowhere, which is exactly the
         * dead end this level was added to remove. */
        if ( PathIsRoot( path ) ) drives = 1;
        else                      chdir( ".." );
        break;
    case DS_DRIVE:
        {
            unsigned n;
            _dos_setdrive( (unsigned)( toupper( ents[i].name[0] ) - 'A' + 1 ), &n );
            drives = 0;          /* into that drive's current directory */
        }
        break;
    default:
        chdir( ents[i].name );
        break;
    }

    Rescan();
    /* Compare where we ARE against where we were, rather than trusting
     * chdir's return: selecting an empty floppy drive changes the drive
     * successfully and then getcwd fails, and "did anything happen?" is the
     * question the caller actually has.  Changing which of the two VIEWS is
     * showing counts as a change even when the path does not - leaving the
     * drive list for a directory whose name it never held, say. */
    return ( drives != wasDrives || strcmp( path, before ) != 0 );
}

int DirScan::Up( void )
{
    char before[DS_PATHLEN];

    if ( drives ) return 0;                  /* the drive list is the top */

    if ( !getcwd( before, sizeof( before ) ) ) before[0] = '\0';

    if ( PathIsRoot( before ) )
    {
        drives = 1;
        Rescan();
        return 1;
    }

    chdir( ".." );
    Rescan();
    return ( strcmp( path, before ) != 0 );
}
