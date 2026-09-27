/*===========================================================================
 * TEXTVIEW.HPP - Modal read-only text viewer.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * Shows a text file - or a string already in memory - with a title row, a
 * line counter, and a [ Close ] button.  FULL SCREEN by default and with no
 * border, because a readme is written for eighty columns and a bordered box
 * inside an eighty-column screen can show seventy-four of them.  A caller
 * that wants it in a box says so with SetRegion.
 *
 *   - Up/Down/PgUp/PgDn/Home/End scroll
 *   - Left/Right scroll sideways, for lines wider than the view
 *   - F     find, F3 find again (case-insensitive)
 *   - Esc / Enter / Close / the title bar's close box leave
 *
 * Show() saves the screen, runs its own event loop, and restores it.
 *
 * READ-ONLY AND BOUNDED ON PURPOSE.  A viewer that could edit would need a
 * write path; one that loaded without a cap would be the one thing in a
 * program that can be handed a 600 MB file by accident.  The load stops at
 * TV_MAX_BYTES and says so on the counter row rather than failing.
 *
 * LINKS AGAINST INPUTBOX.  Find has to ask for a string, and asking for a
 * string is what InputBox is: a build that includes TEXTVIEW.CPP needs
 * INPUTBOX.CPP too.
 *===========================================================================*/
#ifndef TEXTVIEW_HPP
#define TEXTVIEW_HPP

#include "popup.hpp"
#include "button.hpp"
#include "structs.hpp"

/* How much of a file we will hold, and how many lines we will index.
 *
 * The 32-bit numbers are what a DOS extender can allocate without thinking
 * about it.  The 16-bit ones are not a matter of taste: in the large model a
 * single allocation has to fit in a segment, so 48K of text and 2048 far
 * pointers (8K) are what there is room for.  Both report the cut. */
#ifdef __386__
#define TV_MAX_BYTES  (192L * 1024L)
#define TV_MAX_LINES  8192
#else
#define TV_MAX_BYTES  (48L * 1024L)
#define TV_MAX_LINES  2048
#endif

#define TV_FIND_MAX   64               /* longest string Find will look for */

class TextView : public Popup
{
    char  *text;                   // the whole file, NUL-terminated
    long   textLen;
    char **line;                   // line[i] points into text
    int    lines;
    int    truncated;              // the file was longer than we read

    int    top;                    // first visible line
    int    left;                   // horizontal scroll, in columns

    int    regionSet;              // SetRegion was called
    short  regTop, regLeft, regBottom, regRight;

    char   find[TV_FIND_MAX];      // last string searched for
    int    hit;                    // line a find landed on, or -1
    int    notFound;               // the last find came back empty

    Button btnClose;

    int  VisRows( void ) const;
    int  VisCols( void ) const;
    void Place( void );            // set coordinates from the region or the screen
    void ClampView( void );
    int  Index( const char *raw, long got, int cut );  // shared by both loads
    void DrawTitleRow( void );     // row 0: Popup's border no longer holds it
    void DrawBody( void );
    void DrawFoot( void );         // "line 40-63 of 512" + the truncation note
    void LayOutButtons( void );
    void Render( void );
    int  FindFrom( int startLine ); // 1 = found, and 'hit'/'top' have moved
    void AskFind( void );

    TextView( const TextView & );
    TextView &operator=( const TextView & );

public:
    TextView( const foreground_color_t foreground,
              const background_color_t background, const char *titleText );
    ~TextView();

    /* Show the viewer in a box instead of over the whole screen.  0-based,
     * border inclusive - the same convention as every other widget here. */
    void SetRegion( short row, short col, short end_row, short end_col );

    /* Read 'path' and index its lines.  Returns the number of lines, or -1 if
     * the file could not be opened.  A file with no line breaks at all is one
     * line, which is correct and looks it. */
    int Load( const char *path );

    /* The same viewer over a string already in memory: an archive's comment
     * lives in its header and was never a file.  Same return, same bound. */
    int LoadText( const char *s );

    int Lines( void ) const     { return lines; }
    int WasTruncated( void ) const { return truncated; }

    void Show( void );

    /* Self-test only: draw what Show() draws, minus the modal loop, so a
     * headless harness can snapshot it (then call RestoreWindow). */
    void TestOpen( void );

    /* Self-test only: with a peek installed, Show() renders as usual, calls
     * the peek, and returns as if Esc had been pressed - so the harness can
     * drive the REAL path (unpack the entry to a temp file, load it, show it,
     * delete it) instead of only the widget.  DfSetModalPeek does the same
     * thing for every other modal in the toolkit; this one is kept because
     * callers already use it by name. */
    typedef void (*TvPeek)( void );
    static void SetPeek( TvPeek fn );
};

#endif
