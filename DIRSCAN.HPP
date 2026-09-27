/*===========================================================================
 * DIRSCAN.HPP - A directory as a list of rows.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * Drives, the way back up, subdirectories and files, in one sorted list with
 * the columns a list widget wants: name, size, date, attributes.  It is the
 * model a file browser sits on - FileBox uses one, and so does any program
 * that wants its own window to BE a browser rather than open one.
 *
 * Navigation changes the process's current directory and drive, which is
 * what makes "where am I?" a question with one answer: getcwd.  A caller
 * that must go back where it started (a modal dialog, say) saves the path
 * before it starts and chdir's back afterwards; FileBox does exactly that.
 *
 * TWO KINDS OF LISTING.  A directory shows the way up, its subdirectories
 * and its files.  ABOVE the root of a drive is one more level - the DRIVE
 * LIST - and that is the only place drives appear.  Repeating them in every
 * listing put A: and B: in front of someone forty levels deep who was
 * looking for the way back, and made the row count of a directory depend on
 * how many drives the machine had.  Going up from a root reaches the drive
 * list; going up from the drive list does nothing, because it is the top.
 *
 * HIDDEN FILES ARE NOT LISTED, and there is no option to change it: the
 * things wearing that bit on a DOS machine are IO.SYS, MSDOS.SYS, a swap
 * file and a volume tracker.
 *===========================================================================*/
#ifndef DIRSCAN_HPP
#define DIRSCAN_HPP

#define DS_MAX_ENTRIES 1024      /* entries one directory may show           */
#define DS_PATHLEN      260
#define DS_FILTERLEN     64      /* "*.ZIP;*.7Z;*.RAR" and the like          */

/* Kinds, in the order they sort: the way out first, then drives, then
 * directories, then files. */
#define DS_PARENT 0
#define DS_DRIVE  1
#define DS_DIR    2
#define DS_FILE   3

struct DsEntry
{
    char          disp[16];      /* "[..]", "[-C-]", "[SUB]", "FOO.ZIP"      */
    char          name[16];      /* "..", "C:", "SUB", "FOO.ZIP"             */
    int           kind;
    unsigned long size;
    unsigned      date;          /* DOS packed date/time, as findfirst gives */
    unsigned      time;
    unsigned      attrib;
};

class DirScan
{
    /* On the heap, not a member array: forty-odd kilobytes of DsEntry is
     * more than a 16-bit program's whole stack, and this object is a local
     * in somebody's frame. */
    DsEntry *ents;
    int      count;
    char     path[DS_PATHLEN];
    int      drives;             /* 1 = showing the drive list, not a dir    */
    char     filter[DS_FILTERLEN];
    int      dirsOnly;

    int  Accept( const char *name ) const;   /* does the filter want it?     */

    DirScan( const DirScan & );
    DirScan &operator=( const DirScan & );

public:
    DirScan();
    ~DirScan();

    /* Show only the files matching a ';'-separated pattern list, e.g.
     * "*.ZIP;*.7Z".  '*' and '?' both work.  An empty or null filter shows
     * every file, which is the default: a browser that hides most of a
     * directory is a browser you cannot trust to tell you what is there, so
     * hiding anything has to be asked for. */
    void SetFilter( const char *patterns );

    /* List directories only - for choosing a folder rather than a file. */
    void SetDirsOnly( int on );

    /* List the current working directory, or the drives when the view is the
     * drive list.  Returns the number of rows, which is never zero: a
     * directory always has at least the way out of it, and the drive list
     * always has the drive we are running from. */
    int Rescan( void );

    int Count( void ) const            { return count; }

    /* Rows that are THINGS IN THIS DIRECTORY, which is not the same as the
     * row count: ".." is a way out of the directory, not an item in it, and
     * a status line that counts it is wrong. */
    int ItemCount( void ) const;

    /* The directory being shown, or "" in the drive list - there is no path
     * above a root, and inventing one would be a lie the caller could not
     * tell from a real directory.  Ask IsDriveList first. */
    const char *Path( void ) const     { return path; }
    int IsDriveList( void ) const      { return drives; }

    int Kind( int i ) const;
    const char *Disp( int i ) const;
    const char *Name( int i ) const;
    unsigned long Size( int i ) const;
    void Date( int i, char *buf, int buflen ) const;
    void Attr( int i, char *buf, int buflen ) const;

    /* Directory-like: the parent row, a drive, or a subdirectory.  These are
     * the rows Enter navigates rather than opens. */
    int IsDir( int i ) const;
    int IsUp( int i ) const            { return Kind( i ) == DS_PARENT; }

    /* Full path of row i, for opening or viewing it. */
    void FullName( int i, char *dst, int size ) const;

    /* Navigate into row i (parent, drive or subdirectory) and rescan.
     * Returns 1 if the view changed, 0 if the row was not a directory or the
     * change failed - a drive with no disk in it being the usual reason,
     * which is why the old directory is rescanned rather than assumed. */
    int Enter( int i );

    /* Up one level: a directory to its parent, a root to the DRIVE LIST, and
     * the drive list nowhere (returns 0 - it is the top). */
    int Up( void );
};

#endif
