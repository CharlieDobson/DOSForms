/*===========================================================================
 * DIRTREE.CPP - The drives and folders as a tree of rows.  See DIRTREE.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "dirtree.hpp"
#include "dirscan.hpp"          // DsDriveExists
#include "criterr.hpp"          // DfFindFirst - a search a disk error cannot lie to
#include <string.h>
#include <stdlib.h>             // malloc, qsort, _fullpath
#include <ctype.h>
#include <dos.h>                // find_t

/* What DirScan asks for, and for its reasons: no _A_HIDDEN, so hidden
 * folders are left out. */
#define DT_FIND_ATTR ( _A_SUBDIR | _A_SYSTEM | _A_RDONLY )

static int IsSep( char c )
{
    return ( c == '\\' || c == '/' );
}

static int NodeCmp( const void *a, const void *b )
{
    return stricmp( ( (const DtNode *)a )->name, ( (const DtNode *)b )->name );
}

/* 'dir' with "\*.*" on the end, minding a 'dir' that already ends in a
 * separator - the root of a drive does.  Returns 0 if it does not fit. */
static int MakePattern( char *dst, int size, const char *dir )
{
    int len = (int)strlen( dir );

    if ( len + 5 > size ) return 0;
    strcpy( dst, dir );
    if ( len > 0 && !IsSep( dst[len-1] ) ) dst[len++] = '\\';
    strcpy( dst + len, "*.*" );
    return 1;
}

/* Is there a folder inside 'dir'?  Stops at the first one, so the cost is
 * the files that happen to come before it. */
static int HasFolders( const char *dir )
{
    char          pattern[DT_PATHLEN];
    struct find_t found;

    if ( !MakePattern( pattern, sizeof( pattern ), dir ) ) return 0;
    if ( DfFindFirst( pattern, DT_FIND_ATTR, &found ) != 0 ) return 0;

    do
    {
        if ( ( found.attrib & _A_SUBDIR ) && found.name[0] != '.' ) return 1;
    } while ( DfFindNext( &found ) == 0 );

    return 0;
}

DirTree::DirTree()
{
    count = 0;
    nodes = (DtNode *)malloc( (size_t)DT_MAX_NODES * sizeof( DtNode ) );
}

DirTree::~DirTree()
{
    if ( nodes ) free( nodes );
}

int DirTree::Build( void )
{
    int drive;

    count = 0;
    if ( !nodes ) return 0;

    for ( drive = 1; drive <= 26 && count < DT_MAX_NODES; ++drive )
    {
        if ( !DsDriveExists( drive ) ) continue;

        nodes[count].name[0] = (char)( 'A' + drive - 1 );
        nodes[count].name[1] = ':';
        nodes[count].name[2] = '\0';
        nodes[count].depth   = 0;
        /* A guess, and it has to be: see the note in the header. */
        nodes[count].flags   = (unsigned char)( DT_DRIVE | DT_KIDS );
        ++count;
    }
    return count;
}

int DirTree::Depth( int row ) const
{
    return ( row >= 0 && row < count ) ? nodes[row].depth : 0;
}

int DirTree::HasKids( int row ) const
{
    return ( row >= 0 && row < count && ( nodes[row].flags & DT_KIDS ) ) ? 1 : 0;
}

int DirTree::IsOpen( int row ) const
{
    return ( row >= 0 && row < count && ( nodes[row].flags & DT_OPEN ) ) ? 1 : 0;
}

int DirTree::IsDrive( int row ) const
{
    return ( row >= 0 && row < count && ( nodes[row].flags & DT_DRIVE ) ) ? 1 : 0;
}

const char *DirTree::Name( int row ) const
{
    return ( row >= 0 && row < count ) ? nodes[row].name : "";
}

/* A row's ancestors are the nearest rows above it at each shallower depth,
 * which is all a flat list with depths needs to know to find them. */
void DirTree::FullPath( int row, char *dst, int size ) const
{
    int chain[DT_MAX_DEPTH];
    int want, i, level, used = 0;

    if ( size <= 0 ) return;
    dst[0] = '\0';
    if ( row < 0 || row >= count ) return;

    want = nodes[row].depth;
    if ( want >= DT_MAX_DEPTH ) return;

    chain[want] = row;
    for ( i = row - 1; i >= 0 && want > 0; --i )
    {
        if ( nodes[i].depth == want - 1 ) chain[--want] = i;
    }
    if ( want != 0 ) return;            /* no drive above it: not ours */

    for ( level = 0; level <= nodes[row].depth; ++level )
    {
        const char *part = nodes[chain[level]].name;

        /* One separator after the drive and one between folders - never
         * two together, which real DOS reads as the start of a UNC name. */
        if ( level > 1 && used < size - 1 ) dst[used++] = '\\';
        for ( i = 0; part[i] && used < size - 1; ++i ) dst[used++] = part[i];
        if ( level == 0 && used < size - 1 ) dst[used++] = '\\';
    }
    dst[used] = '\0';
}

/* Read the folders inside 'row' and put them in the list straight after it.
 * Returns how many there were.
 *
 * The rows below are moved to the top of the array first, which opens a gap
 * the children can be written into and sorted where they lie; then the rows
 * below are brought back down to meet them.  No second buffer. */
int DirTree::Scan( int row )
{
    char          path[DT_PATHLEN];
    char          pattern[DT_PATHLEN];
    struct find_t found;
    DtNode       *kid;
    int           below, room, base, kids = 0, i;

    if ( !nodes || row < 0 || row >= count ) return 0;
    if ( nodes[row].depth + 1 >= DT_MAX_DEPTH ) return 0;

    FullPath( row, path, sizeof( path ) - DT_NAMELEN - 1 );
    if ( !path[0] ) return 0;
    if ( !MakePattern( pattern, sizeof( pattern ), path ) ) return 0;

    below = count - ( row + 1 );
    room  = DT_MAX_NODES - count;
    if ( room < 1 ) return 0;

    if ( below > 0 )
    {
        memmove( nodes + DT_MAX_NODES - below, nodes + row + 1,
                 (size_t)below * sizeof( DtNode ) );
    }
    kid = nodes + row + 1;

    if ( DfFindFirst( pattern, DT_FIND_ATTR, &found ) == 0 )
    {
        do
        {
            if ( !( found.attrib & _A_SUBDIR ) ) continue;
            if ( found.name[0] == '.' ) continue;       /* . and ..          */
            if ( kids >= room ) break;

            strncpy( kid[kids].name, found.name, DT_NAMELEN - 1 );
            kid[kids].name[DT_NAMELEN - 1] = '\0';
            kid[kids].depth = (unsigned char)( nodes[row].depth + 1 );
            kid[kids].flags = 0;
            ++kids;
        } while ( DfFindNext( &found ) == 0 );
    }

    if ( kids > 1 ) qsort( kid, (size_t)kids, sizeof( DtNode ), NodeCmp );

    /* Look into each one - AFTER the search above has finished, so that two
     * searches are never in progress at once. */
    base = (int)strlen( path );
    if ( base > 0 && !IsSep( path[base-1] ) ) path[base++] = '\\';
    for ( i = 0; i < kids; ++i )
    {
        strcpy( path + base, kid[i].name );
        if ( HasFolders( path ) ) kid[i].flags |= DT_KIDS;
    }

    if ( below > 0 )
    {
        memmove( kid + kids, nodes + DT_MAX_NODES - below,
                 (size_t)below * sizeof( DtNode ) );
    }
    count += kids;
    return kids;
}

int DirTree::Open( int row )
{
    if ( row < 0 || row >= count ) return count;
    if ( nodes[row].flags & DT_OPEN ) return count;
    if ( !( nodes[row].flags & DT_KIDS ) ) return count;

    if ( Scan( row ) > 0 )
    {
        nodes[row].flags |= DT_OPEN;
    }
    else if ( !( nodes[row].flags & DT_DRIVE ) )
    {
        /* Nothing in it after all - somebody removed them since the [+] was
         * worked out.  A DRIVE keeps its marker whatever was found: the
         * usual reason for finding nothing is that there is no disk in it,
         * and the user has to be able to try again once there is. */
        nodes[row].flags &= (unsigned char)~DT_KIDS;
    }
    return count;
}

int DirTree::Close( int row )
{
    int end, gone;

    if ( row < 0 || row >= count ) return count;
    if ( !( nodes[row].flags & DT_OPEN ) ) return count;

    end = row + 1;
    while ( end < count && nodes[end].depth > nodes[row].depth ) ++end;

    gone = end - ( row + 1 );
    if ( gone > 0 && end < count )
    {
        memmove( nodes + row + 1, nodes + end,
                 (size_t)( count - end ) * sizeof( DtNode ) );
    }
    count -= gone;
    nodes[row].flags &= (unsigned char)~DT_OPEN;
    return count;
}

int DirTree::Toggle( int row )
{
    return IsOpen( row ) ? Close( row ) : Open( row );
}

int DirTree::FindChild( int row, const char *name ) const
{
    int i;

    if ( row < 0 || row >= count ) return -1;

    for ( i = row + 1; i < count && nodes[i].depth > nodes[row].depth; ++i )
    {
        if ( nodes[i].depth == nodes[row].depth + 1 &&
             stricmp( nodes[i].name, name ) == 0 ) return i;
    }
    return -1;
}

int DirTree::Reveal( const char *path )
{
    char        full[DT_PATHLEN];
    char        part[DT_NAMELEN];
    const char *at;
    int         row = -1, i, len;

    if ( !path || !path[0] ) return -1;

    if ( !_fullpath( full, path, sizeof( full ) ) )
    {
        strncpy( full, path, sizeof( full ) - 1 );
        full[sizeof( full ) - 1] = '\0';
    }
    if ( !full[0] || full[1] != ':' ) return -1;

    for ( i = 0; i < count; ++i )
    {
        if ( ( nodes[i].flags & DT_DRIVE ) &&
             toupper( (unsigned char)nodes[i].name[0] ) ==
             toupper( (unsigned char)full[0] ) ) { row = i; break; }
    }
    if ( row < 0 ) return -1;

    at = full + 2;
    for ( ;; )
    {
        int child;

        while ( IsSep( *at ) ) ++at;
        if ( !*at ) break;

        len = 0;
        while ( at[len] && !IsSep( at[len] ) ) ++len;
        if ( len >= DT_NAMELEN ) break;         /* not a name DOS gave us    */
        memcpy( part, at, (size_t)len );
        part[len] = '\0';
        at += len;

        Open( row );
        child = FindChild( row, part );
        if ( child < 0 ) break;                 /* from here down is new     */
        row = child;
    }
    return row;
}
