/*===========================================================================
 * CRITERR.HPP - Critical errors (INT 24h), answered by the program.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS   (16-bit and 32-bit)
 *
 * A drive with no disk in it, a write-protected disk, a bad sector: DOS
 * reports these by calling INT 24h BEFORE the call that hit them returns,
 * and the handler a program starts with is COMMAND.COM's, which writes
 *
 *     Not ready reading drive A
 *     Abort, Retry, Fail?
 *
 * straight onto whatever screen the program has up, and waits.  On a text
 * screen that is a line of someone else's text through the middle of a
 * dialog; on the graphics screen it is worse; and Abort ends the program
 * without a word to it.
 *
 * WHAT THIS DOES INSTEAD, and it is two halves on purpose:
 *
 *   THE HANDLER only takes notes and tells DOS to FAIL the call.  It runs
 *   inside DOS, part of the way through the call that failed, and DOS
 *   cannot be entered twice: character I/O is all it will take from a
 *   handler - no memory, no files, and so no Popup, which allocates the
 *   save buffer for what it covers.  (The stack is the program's own; DOS
 *   puts it back before the call.)  It writes down what went wrong and
 *   returns.
 *
 *   THE PROGRAM asks afterwards.  The call that failed comes back with an
 *   error like any other; the code that made it looks here to see whether
 *   a critical error was the reason, puts up the box, and if the answer is
 *   Retry makes the call again:
 *
 *       for ( ;; )
 *       {
 *           DfCritClear();
 *           tree.Open( row );
 *           if ( !DfCritHit() ) break;          // it worked, or failed quietly
 *           if ( !DfCritAsk( mouse ) ) break;   // Cancel
 *       }
 *
 * Construct ONE DfCritGuard in main to install the handler; the vector it
 * found goes back when the guard does.  A program that never constructs
 * one keeps DOS's prompt for its own disk calls, DfCritHit() is always zero
 * there, and code written as above runs its loop once.
 *
 * GUARDS NEST.  Only the outermost installs and puts back, so a widget that
 * reads the disk holds one of its own while it is up: FileBox does, and
 * asks Retry / Cancel in any program, with a guard in main or without.
 *
 * DfCritAsk is a DlgBox, so a build that includes CRITERR.CPP needs DLGBOX,
 * POPUP and BUTTON.
 *===========================================================================*/
#ifndef CRITERR_HPP
#define CRITERR_HPP

#include "structs.hpp"

class MSMouse;

/* What DOS puts in the low byte of DI.  The three a user can do something
 * about have their own wording; the rest are "error reading" / "writing". */
#define DF_CRIT_WRITE_PROTECT  0x00
#define DF_CRIT_NOT_READY      0x02

class DfCritGuard
{
public:
    DfCritGuard( void );
    ~DfCritGuard( void );

private:
    DfCritGuard( const DfCritGuard & );
    DfCritGuard &operator=( const DfCritGuard & );
};

/* Forget the last error.  Call it before the operation being watched. */
void DfCritClear( void );

/* How many critical errors since the last clear.  One operation can raise
 * several - DOS retries, and a directory search is more than one read. */
int  DfCritHit( void );

int  DfCritCode( void );        /* DF_CRIT_... or another DOS code           */
int  DfCritDrive( void );       /* 0 = A:, or -1 when it was not a disk      */
int  DfCritWriting( void );     /* 1 if it was a write that failed           */

/* The error in words: "No disk in drive A:." */
void DfCritText( char *buf, int size );

/* Say what went wrong and ask: Retry, or Cancel.  TRUE means try again -
 * and DOS has then been told to forget what it has in its buffers (a disk
 * reset), because what it has for the sector it could not read is whatever
 * was in the buffer before, and it would use that rather than read again. */
boolean DfCritAsk( MSMouse *mouse );

/* A directory search in which A CALL THAT MET A CRITICAL ERROR HAS FAILED,
 * WHATEVER DOS RETURNED.  DOS told to fail a read does not always pass the
 * failure on.  Measured on MS-DOS 5, a root directory that could not be
 * read: the search came back "found" four times, with names made of what
 * was lying in the buffer.  These two are _dos_findfirst and _dos_findnext
 * with that seen to - use them wherever the names are going to be shown or
 * acted on.  Non-zero is "no more", as theirs is. */
struct find_t;
unsigned DfFindFirst( const char *pattern, unsigned attrib, struct find_t *found );
unsigned DfFindNext( struct find_t *found );

/* Answer for the dialog.  With one set, DfCritAsk calls it instead of
 * putting the box up and takes what it returns (non-zero = retry) - for a
 * test harness, which has nobody to press Retry, and for a program that
 * wants to ask in its own way. */
typedef int (*DfCritAskFn)( int code, int drive );
void DfSetCritAsk( DfCritAskFn fn );

#endif
