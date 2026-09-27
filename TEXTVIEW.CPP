/*===========================================================================
 * TEXTVIEW.CPP - Modal read-only text viewer implementation.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * The whole file is read into one malloc'd block and the lines are indexed by
 * pointing into it, with the line breaks overwritten by NULs.  That costs one
 * pointer per line on top of the text and makes scrolling a subscript, which
 * matters on the machines this runs on: an 800-line readme redrawn on every
 * arrow key is 800 pointer loads, not 800 searches through the buffer.
 *
 * Tabs are expanded on the way in rather than at paint time, so a line's
 * length in the buffer IS its width on screen and the horizontal scroll can
 * be a simple offset.
 *===========================================================================*/
#include "textview.hpp"
#include "inputbox.hpp"         // Find asks for its string with one
#include "msmouse.hpp"          // LEFT_BUTTON + MSMouse status for clicks
#include "moushide.hpp"         // hide the cursor while painting
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define TV_TAB   8
#define TV_LINEBUF 168          /* widest row this will paint in one call   */

// Self-test hook - see SetPeek in the header.  NULL in every real run.
static TextView::TvPeek s_peek = 0;

void TextView::SetPeek( TvPeek fn ) { s_peek = fn; }

TextView::TextView( const foreground_color_t foreground,
                    const background_color_t background, const char *titleText )
    : Popup( foreground, background, titleText )
{
    border_style       = BORDER_NONE;
    drop_shadow        = FALSE;      /* nowhere to cast one at full screen */
    text_justification = CENTER;
    text      = 0;
    textLen   = 0;
    line      = 0;
    lines     = 0;
    truncated = 0;
    top       = 0;
    left      = 0;
    regionSet = 0;
    regTop = regLeft = regBottom = regRight = 0;
    find[0]   = '\0';
    hit       = -1;
}

TextView::~TextView()
{
    if ( text ) free( text );
    if ( line ) free( line );
}

void TextView::SetRegion( short row, short col, short end_row, short end_col )
{
    regTop = row; regLeft = col; regBottom = end_row; regRight = end_col;
    regionSet = 1;
}

/* Where the viewer sits.  With no region set it is the WHOLE screen, which
 * is the point of it: a readme is written for eighty columns, and a border
 * plus its padding is four of the six columns that go missing. */
void TextView::Place( void )
{
    short rows = DfRows() ? DfRows() : 25;
    short cols = DfCols() ? DfCols() : 80;

    if ( regionSet )
    {
        SetCoordinates( regTop, regLeft, regBottom, regRight );
        return;
    }
    SetCoordinates( 0, 0, (short)( rows - 1 ), (short)( cols - 1 ) );
}

/* Row 0 title, last row the button, the one above it the line counter: three
 * rows of furniture and no border, so the text gets everything else. */
int TextView::VisRows( void ) const
{
    int h = coordinates.end_row - coordinates.row + 1 - 3;
    return ( h < 1 ) ? 1 : h;
}

int TextView::VisCols( void ) const
{
    int w = coordinates.end_column - coordinates.column + 1;
    if ( w < 1 ) w = 1;
    if ( w > TV_LINEBUF - 2 ) w = TV_LINEBUF - 2;
    return w;
}

/* Read the file, expand tabs, split into lines.
 *
 * Control characters other than tab and the line breaks are shown as '.': a
 * readme.txt is the intended subject, but nothing stops a user opening this on
 * a .COM file, and a stray 0x07 in that case would beep the machine once per
 * redraw. */
int TextView::Load( const char *path )
{
    FILE *fp;
    char *raw;
    long  got;
    int   rc;

    fp = fopen( path, "rb" );
    if ( !fp ) return -1;

    raw = (char *)malloc( (size_t)TV_MAX_BYTES + 1 );
    if ( !raw ) { fclose( fp ); return -1; }
    got = (long)fread( raw, 1, (size_t)TV_MAX_BYTES, fp );
    if ( got < 0 ) got = 0;
    raw[got] = '\0';
    /* Anything still there means we stopped short of the end.  Read before the
     * close and passed in, because Index cannot know it. */
    rc = Index( raw, got, ( fgetc( fp ) != EOF ) );
    fclose( fp );
    free( raw );
    return rc;
}

/* The same viewer over text already in memory - an archive's comment, which
 * lives in the header and was never a file.  Same bound: a comment longer than
 * TV_MAX_BYTES is shown as far as it goes and says so. */
int TextView::LoadText( const char *s )
{
    long n;

    if ( !s ) return -1;
    n = (long)strlen( s );
    return Index( s, ( n > TV_MAX_BYTES ) ? TV_MAX_BYTES : n,
                  ( n > TV_MAX_BYTES ) );
}

/* Expand tabs, drop CRs, replace control bytes, and index the lines.  Shared
 * by both loaders so a comment and a file are shown by exactly the same code. */
int TextView::Index( const char *raw, long got, int cut )
{
    long i;
    long outLen = 0;
    int  col    = 0;

    if ( text ) { free( text ); text = 0; }
    if ( line ) { free( line ); line = 0; }
    lines = 0; truncated = cut; top = 0; left = 0; hit = -1; notFound = 0;

    /* Worst case every byte is a tab, so the expansion can be TV_TAB times as
     * long.  Rather than allocate eight times the file, count first. */
    for ( i = 0; i < got; i++ )
    {
        if ( raw[i] == '\t' ) { outLen += TV_TAB - ( col % TV_TAB );
                                col     += TV_TAB - ( col % TV_TAB ); }
        else if ( raw[i] == '\n' ) { outLen++; col = 0; }
        else if ( raw[i] == '\r' ) { /* dropped */ }
        else { outLen++; col++; }
    }

    text = (char *)malloc( (size_t)outLen + 2 );
    if ( !text ) return -1;

    col = 0; textLen = 0;
    for ( i = 0; i < got; i++ )
    {
        unsigned char c = (unsigned char)raw[i];
        if ( c == '\t' )
        {
            int pad = TV_TAB - ( col % TV_TAB );
            while ( pad-- ) { text[textLen++] = ' '; col++; }
        }
        else if ( c == '\n' ) { text[textLen++] = '\n'; col = 0; }
        else if ( c == '\r' ) { /* dropped: CRLF and LF look the same here */ }
        else if ( c < 32 || c == 127 ) { text[textLen++] = '.'; col++; }
        else { text[textLen++] = (char)c; col++; }
    }
    text[textLen] = '\0';

    /* Index the lines, turning each break into the NUL that ends the one
     * before it.  An empty file is one empty line, so the box is never blank
     * with no explanation. */
    line = (char **)malloc( (size_t)TV_MAX_LINES * sizeof( char * ) );
    if ( !line ) { free( text ); text = 0; return -1; }

    line[lines++] = text;
    for ( i = 0; i < textLen && lines < TV_MAX_LINES; i++ )
    {
        if ( text[i] == '\n' )
        {
            text[i] = '\0';
            if ( i + 1 <= textLen ) line[lines++] = text + i + 1;
        }
    }
    /* Ran out of index before running out of text. */
    if ( lines >= TV_MAX_LINES && i < textLen ) truncated = 1;

    /* A file ending in a newline gives a final empty line that is real but
     * looks like a bug at the bottom of the box.  Drop just that one. */
    if ( lines > 1 && line[lines - 1][0] == '\0' ) lines--;

    return lines;
}

void TextView::ClampView( void )
{
    int vis = VisRows();
    int maxTop = ( lines > vis ) ? lines - vis : 0;
    if ( top > maxTop ) top = maxTop;
    if ( top < 0 ) top = 0;
    if ( left < 0 ) left = 0;
    if ( left > 512 ) left = 512;
}

void TextView::DrawBody( void )
{
    int  vis = VisRows();
    int  iw  = VisCols();
    int  i;
    char out[TV_LINEBUF];

    for ( i = 0; i < vis; ++i )
    {
        int         idx = top + i;
        const char *s   = "";
        int         j   = 0;
        foreground_color_t fg = fg_color;
        background_color_t bg = bg_color;

        if ( idx < lines )
        {
            int len = (int)strlen( line[idx] );
            s = ( left < len ) ? ( line[idx] + left ) : "";
        }
        while ( s[j] && j < iw ) { out[j] = s[j]; ++j; }
        while ( j < iw ) out[j++] = ' ';
        out[iw] = '\0';

        /* The line a Find landed on, marked so the answer to "where?" is on
         * the screen rather than in the reader's counting. */
        if ( idx == hit && idx < lines ) { fg = FG_BLACK; bg = BG_CYAN; }

        /* Row 0 is the title, so the text starts on row 1, at the left edge:
         * no border and no padding is the whole point. */
        DfTextOut( (short)( coordinates.row + 2 + i ), (short)( coordinates.column + 1 ),
                   out, fg, bg );
    }
}

/* The line counter, the find hint, and the warning if there is more file than
 * we read. */
void TextView::DrawFoot( void )
{
    char foot[TV_LINEBUF];
    int  vis  = VisRows();
    int  last = top + vis;
    int  n;
    int  iw   = VisCols();

    if ( last > lines ) last = lines;

    sprintf( foot, " Lines %d-%d of %d   F=Find  F3=Again%s%s",
             ( lines ? top + 1 : 0 ), last, lines,
             truncated ? "   (TRUNCATED - too big to show it all)" : "",
             notFound ? "   NOT FOUND" : ( left ? "   >>" : "" ) );
    for ( n = (int)strlen( foot ); n < iw; n++ ) foot[n] = ' ';
    foot[iw] = '\0';

    DfTextOut( (short)( coordinates.row + 2 + vis ), (short)( coordinates.column + 1 ),
               foot, FG_YELLOW, bg_color );
}

void TextView::LayOutButtons( void )
{
    int row = coordinates.end_row;        /* the last row, not one in from it */
    int oc  = coordinates.column +
              ( ( coordinates.end_column - coordinates.column + 1 ) - 9 ) / 2;

    if ( oc < coordinates.column ) oc = coordinates.column;

    btnClose = Button( FG_WHITE, BG_CYAN );
    btnClose.SetMouse( mouse );
    strcpy( btnClose.label, "Close" );      /* -> "[ Close ]", 9 cells */
    btnClose.SetSingleRow( (unsigned short)row, (unsigned short)oc );
}

void TextView::Render( void )
{
    MouseHide guard( mouse );
    /* Popup's own fill insets by one on every side whatever the border style,
     * so with BORDER_NONE the outer ring keeps whatever was on the screen
     * underneath.  Paint the whole rectangle before anything goes on it: at
     * full screen this window IS the screen, and a one-cell frame of the old
     * list showing through it looks like a drawing bug. */
    FillBox( (short)( coordinates.row + 1 ), (short)( coordinates.column + 1 ),
             (short)( coordinates.end_row + 1 ),
             (short)( coordinates.end_column + 1 ), bg_color );
    DrawTitleRow();
    DrawBody();
    DrawFoot();
    btnClose.Draw();
}

/* The file's name, centred on row 0.  Popup draws its title into the border it
 * no longer has, so this draws it instead. */
void TextView::DrawTitleRow( void )
{
    char row0[TV_LINEBUF];
    int  w   = VisCols();
    int  n   = (int)strlen( title );
    int  pad = ( w - n ) / 2;
    int  i;

    if ( pad < 0 ) pad = 0;
    for ( i = 0; i < pad && i < w; i++ ) row0[i] = ' ';
    for ( n = 0; title[n] && i < w; n++ ) row0[i++] = title[n];
    while ( i < w ) row0[i++] = ' ';
    row0[i] = '\0';

    DfTextOut( (short)( coordinates.row + 1 ), (short)( coordinates.column + 1 ),
               row0, FG_BLACK, BG_CYAN );
}

/*---- Find ----------------------------------------------------------------- *
 * Case-insensitive, whole-line-at-a-time, starting below the line the last
 * one landed on so F3 walks forward through the file.  A viewer without a
 * find is a viewer you page through looking for a word you already know.
 *---------------------------------------------------------------------------*/
static const char *StrFindI( const char *hay, const char *needle )
{
    int nlen = (int)strlen( needle );
    int i, j;

    if ( nlen == 0 ) return hay;
    for ( i = 0; hay[i]; ++i )
    {
        for ( j = 0; j < nlen; ++j )
        {
            if ( !hay[i+j] ) return 0;
            if ( toupper( (unsigned char)hay[i+j] ) !=
                 toupper( (unsigned char)needle[j] ) ) break;
        }
        if ( j == nlen ) return hay + i;
    }
    return 0;
}

int TextView::FindFrom( int startLine )
{
    int i;

    if ( !find[0] || lines < 1 ) return 0;
    if ( startLine < 0 ) startLine = 0;

    for ( i = startLine; i < lines; ++i )
    {
        if ( StrFindI( line[i], find ) )
        {
            hit = i;
            /* Put the hit a third of the way down rather than at the very
             * top: what is around a match is usually why it was wanted. */
            top = i - VisRows() / 3;
            ClampView();
            return 1;
        }
    }
    return 0;
}

void TextView::AskFind( void )
{
    InputBox box( FG_BLUE, BG_WHITE, "Find", "Text to look for:" );

    box.SetMouse( mouse );
    box.SetMaxLength( TV_FIND_MAX - 1 );
    box.SetText( find );
    if ( !box.Show() ) { Render(); return; }

    strncpy( find, box.Text(), sizeof( find ) - 1 );
    find[sizeof( find ) - 1] = '\0';

    hit = -1;
    notFound = FindFrom( 0 ) ? 0 : 1;
    Render();
}

void TextView::TestOpen( void )
{
    Place();
    Draw();
    LayOutButtons();
    ClampView();
    Render();
}

void TextView::Show( void )
{
    int lastB;
    int vis;

    Place();
    Draw();
    LayOutButtons();
    ClampView();
    Render();
    vis = VisRows();

    /* Headless self-test: rendered is as far as a snapshot can follow.  Look,
     * and leave as though Esc had been pressed.  Either hook works - the
     * toolkit-wide one and this class's own. */
    if ( s_peek )        { s_peek();        RestoreWindow(); DfModalFinished(); return; }
    if ( DfModalPeek() ) { DfModalPeek()(); RestoreWindow(); DfModalFinished(); return; }

    /* Seed the edge detector from the LIVE button state - the same reason as
     * every other modal here: the click that opened this is usually still
     * held, and starting at 0 reads its release as a fresh press. */
    lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;

    for ( ;; )
    {
        if ( kbhit() )
        {
            int k = getch();
            if ( k == 27 ) break;                       /* Esc */
            if ( k == 13 ) break;                       /* Enter closes too */
            if ( k == 'f' || k == 'F' )
            {
                AskFind();
                lastB = ( mouse && mouse->IsInited() ) ? mouse->Status() : 0;
                continue;
            }
            if ( k == 0 || k == 0xE0 )
            {
                int ext = getch();
                int before = top, beforeL = left;
                switch ( ext )
                {
                case _KB_UP:    top--;              break;
                case _KB_DOWN:  top++;              break;
                case _KB_PGUP:  top -= vis;         break;
                case _KB_PGDN:  top += vis;         break;
                case _KB_HOME:  top = 0; left = 0;  break;
                case _KB_END:   top = lines;        break;
                case _KB_LEFT:  left -= 8;          break;
                case _KB_RIGHT: left += 8;          break;
                case _KB_F3:
                    /* Again, from the line after the last hit. */
                    if ( find[0] )
                        notFound = ( FindFrom( hit + 1 ) || FindFrom( 0 ) ) ? 0 : 1;
                    Render();
                    continue;
                default: continue;
                }
                ClampView();
                if ( top != before || left != beforeL ) Render();
            }
            continue;
        }
        if ( mouse && mouse->IsInited() )
        {
            int b = mouse->Status();
            if ( ( b & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
            {
                short mx = DfMouseCol( mouse->X() ), my = DfMouseRow( mouse->Y() );
                if ( btnClose.HitTest( mx, my ) ) break;
            }
            lastB = b;
        }
    }

    RestoreWindow();
    DfModalFinished();          /* see DOSFORMS.HPP */
}
