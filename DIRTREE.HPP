/*===========================================================================
 * DIRTREE.HPP - The drives and folders of the machine, as a tree of rows.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * The model a folder tree sits on, in the shape ListBox's tree mode asks
 * for: every row has a depth, may have children, and is open or shut.  The
 * top level is the drives; under each are its folders, and theirs.  Files
 * are never listed.  FileBox's tree is one.
 *
 * NOTHING IS READ UNTIL IT IS OPENED.  Build() lists the drives without
 * touching a disk - the same driver question DirScan asks - and a drive or a
 * folder is scanned when the user opens it, not before.  So a machine with
 * an empty floppy drive shows A: and leaves it alone until somebody asks
 * what is in it.
 *
 * SHUTTING A NODE FORGETS WHAT WAS UNDER IT.  The rows held are exactly the
 * rows showing, so the row number the list has IS the index here and there
 * is no second table to keep in step; and opening the node again reads the
 * disk again, which is what you want after changing a floppy.
 *
 * THE [+] IS HONEST FOR FOLDERS AND A GUESS FOR DRIVES.  When a folder's
 * children are read, each child is looked into far enough to say whether it
 * has folders of its own, so a [+] on one means there is something to open.
 * A drive gets a [+] regardless, and keeps it: finding out would mean
 * reading it, and a drive that had nothing to show may only have had no
 * disk in it - the marker is how the user asks again.
 *
 * THE CURRENT DIRECTORY IS NOT MOVED.  Everything is scanned by full path,
 * so unlike DirScan there is nothing for a caller to put back afterwards.
 *
 * HIDDEN FOLDERS ARE NOT LISTED, for the reason DirScan gives.
 *
 * Needs DIRSCAN.CPP, for the drive probe.
 *===========================================================================*/
#ifndef DIRTREE_HPP
#define DIRTREE_HPP

#define DT_MAX_NODES   1024      /* rows the tree may show at once           */
#define DT_MAX_DEPTH     34      /* a drive, and 32 folders of a DOS path    */
#define DT_PATHLEN      260
#define DT_NAMELEN       13      /* 8.3 and its NUL                          */

#define DT_OPEN   0x01           /* its children are showing                 */
#define DT_KIDS   0x02           /* it has folders in it, or may have        */
#define DT_DRIVE  0x04

struct DtNode
{
    char          name[DT_NAMELEN];     /* "C:", or "GAMES"                  */
    unsigned char depth;                /* 0 = a drive                       */
    unsigned char flags;
};

class DirTree
{
    /* On the heap for the reason DirScan's entries are: fifteen kilobytes
     * is a large part of a 16-bit program's stack, and this object is a
     * member of a dialog that is a local in somebody's frame. */
    DtNode *nodes;
    int     count;

    int  Scan( int row );               /* read a node's children in        */
    int  FindChild( int row, const char *name ) const;

    DirTree( const DirTree & );
    DirTree &operator=( const DirTree & );

public:
    DirTree();
    ~DirTree();

    /* Start again from the list of drives, all shut.  Returns the number of
     * rows, which is zero only if there was no memory for any. */
    int Build( void );

    int Count( void ) const             { return count; }

    int Depth( int row ) const;
    int HasKids( int row ) const;
    int IsOpen( int row ) const;
    int IsDrive( int row ) const;
    const char *Name( int row ) const;

    /* "C:\" for a drive, "C:\GAMES\DOOM" for a folder. */
    void FullPath( int row, char *dst, int size ) const;

    /* Open, shut, or whichever it is not.  Each returns the NEW row count,
     * which is what ListBox's toggle callback has to answer with.  Opening
     * a folder that turns out to have nothing in it takes its [+] away and
     * leaves it shut. */
    int Open( int row );
    int Close( int row );
    int Toggle( int row );

    /* Open the tree down to 'path' and answer the row it is on.  A path
     * that does not exist all the way down - a folder about to be created -
     * gets as far as the part that does.  Relative paths are taken from the
     * current directory.  -1 if not even the drive is there. */
    int Reveal( const char *path );
};

#endif
