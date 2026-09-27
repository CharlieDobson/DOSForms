/*===========================================================================
 * LISTBOX.CPP - Scrolling list widget implementation.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "listbox.hpp"
#include "moushide.hpp"
#include "msmouse.hpp"           // LEFT_BUTTON / RIGHT_BUTTON
#include "screen.hpp"            // DfTextOut / DfIconOut - text or graphics
#include <string.h>

#define MARK_GLYPH '*'           /* plain ASCII: survives the test snapshot */

static void FormatPad( char *dst, const char *src, int width )
{
    int i = 0;
    if ( width < 0 )   width = 0;
    if ( width > 126 ) width = 126;
    while ( src[i] && i < width ) { dst[i] = src[i]; ++i; }
    while ( i < width ) dst[i++] = ' ';
    dst[i] = '\0';
}

ListBox::ListBox( const foreground_color_t foreground, const background_color_t background )
    : DOSForms( foreground, background )
{
    region.row = region.column = region.end_row = region.end_column = 0;
    count = 0; sel = 0; top = 0;
    filler = 0; on_activate = 0; on_dblclick = 0; on_marks = 0;
    can_mark = 0; user = 0;
    row_icon = 0; mark_icon = DF_ICON_NONE;
    row_depth = 0; row_haskids = 0; row_expanded = 0; row_toggle = 0;
    row_label = 0; name_width = 24;
    header[0] = '\0';
    marks = 0; mark_count = 0;
    last_click_row = -1; last_click_at = 0;
    sel_fg = FG_BLACK;
    sel_bg = BG_CYAN;
    mark_fg = FG_YELLOW;
}

ListBox::~ListBox( void )
{
    FreeMarks();
}

void ListBox::FreeMarks( void )
{
    delete [] marks;
    marks = 0;
    mark_count = 0;
}

void ListBox::SetRegion( short row, short col, short end_row, short end_col )
{
    region.row = row; region.column = col;
    region.end_row = end_row; region.end_column = end_col;
}

void ListBox::SetHeader( const char *text )
{
    if ( !text ) { header[0] = '\0'; return; }
    strncpy( header, text, sizeof( header ) - 1 );
    header[sizeof( header ) - 1] = '\0';
}

void ListBox::SetData( int rowCount, RowFiller f, void *userData )
{
    count = rowCount; filler = f; user = userData;

    /* New data means new rows: the old marks refer to entries that are gone,
     * and so does the pending double-click - row 3 of the folder we just left
     * is not row 3 of this one. */
    FreeMarks();
    last_click_row = -1;
    if ( count > 0 )
    {
        marks = new unsigned char[count];
        if ( marks ) memset( marks, 0, (size_t)count );
    }

    if ( sel >= count ) sel = count - 1;
    if ( sel < 0 ) sel = 0;
    top = 0;
    ClampView();
}

void ListBox::SetActivate( RowActivate a )               { on_activate = a; }
void ListBox::SetDblClick( RowActivate d )               { on_dblclick = d; }
void ListBox::SetMarksChanged( MarksChanged m )          { on_marks = m; }
void ListBox::SetMarkable( RowMarkable m )               { can_mark = m; }
void ListBox::SetIconProvider( RowIcon fn )              { row_icon = fn; }
void ListBox::SetMarkIcon( int icon )                    { mark_icon = icon; }
void ListBox::SetSelHighlight( foreground_color_t fg, background_color_t bg )
{ sel_fg = fg; sel_bg = bg; }

/*---- The name field ------------------------------------------------------- *
 * Exactly 'width' columns, ending with a NUL:
 *
 *     column 0        left blank for the mark glyph or the icon
 *     then            two spaces per level of depth
 *     then            "[+] " or "[-] " on a row with children, or four
 *                     spaces on one without, so names line up either way
 *     then            the label, cut where the field ends
 *
 * A deep enough row runs out of field, and the indent is clamped rather
 * than allowed to push the marker out of sight: past that depth the tree
 * stops LOOKING deeper, which is better than a row whose name has gone.
 *---------------------------------------------------------------------------*/
void ListBox::BuildTreeName( int index, char *dst, int width )
{
    char label[80];
    int  depth = row_depth ? row_depth( user, index ) : 0;
    int  kids  = row_haskids ? row_haskids( user, index ) : 0;
    int  open  = row_expanded ? row_expanded( user, index ) : 0;
    int  n = 0, i, indent;

    if ( width < 1 ) { dst[0] = '\0'; return; }

    if ( depth < 0 ) depth = 0;
    indent = depth * 2;
    if ( indent > width - 6 ) indent = ( width > 6 ) ? width - 6 : 0;

    dst[n++] = ' ';                                  /* the mark column */
    for ( i = 0; i < indent && n < width; ++i ) dst[n++] = ' ';

    if ( kids )
    {
        if ( n < width ) dst[n++] = '[';
        if ( n < width ) dst[n++] = (char)( open ? '-' : '+' );
        if ( n < width ) dst[n++] = ']';
        if ( n < width ) dst[n++] = ' ';
    }
    else
        for ( i = 0; i < 4 && n < width; ++i ) dst[n++] = ' ';

    label[0] = '\0';
    if ( row_label ) row_label( user, index, label, sizeof( label ) );
    for ( i = 0; label[i] && n < width; ++i ) dst[n++] = label[i];

    while ( n < width ) dst[n++] = ' ';
    dst[n] = '\0';
}

void ListBox::SetTreeProvider( RowDepth depth, RowHasKids hasKids,
                               RowExpanded expanded, RowToggle toggle,
                               RowLabel label, int nameWidth )
{
    row_depth    = depth;
    row_haskids  = hasKids;
    row_expanded = expanded;
    row_toggle   = toggle;
    row_label    = label;
    name_width   = ( nameWidth > 6 ) ? nameWidth : 6;
}

/* The row count changed but the list did not become something else: keep the
 * highlight and as much of the marked set as still exists.  SetData is the
 * other case - new data, and everything about the old data forgotten. */
void ListBox::SetCount( int rowCount )
{
    unsigned char *old = marks;
    int            oldCount = count;
    int            i;

    if ( rowCount < 0 ) rowCount = 0;
    count = rowCount;

    marks = 0;
    mark_count = 0;
    if ( count > 0 )
    {
        marks = new unsigned char[count];
        if ( marks )
        {
            memset( marks, 0, (size_t)count );
            if ( old )
            {
                int n = ( oldCount < count ) ? oldCount : count;
                for ( i = 0; i < n; ++i )
                    if ( old[i] ) { marks[i] = 1; ++mark_count; }
            }
        }
    }
    delete [] old;

    /* A double-click half-finished against the old shape is not a
     * double-click against the new one. */
    last_click_row = -1;
    ClampView();
}

void ListBox::ToggleNode( int index )
{
    int newCount;

    if ( !row_toggle || index < 0 || index >= count ) return;
    if ( row_haskids && !row_haskids( user, index ) ) return;

    newCount = row_toggle( user, index );
    SetCount( newCount );
    sel = index;                /* stay on the node that was opened or shut */
    ClampView();
    Draw();
}
void ListBox::SetMarkColor( foreground_color_t fg )      { mark_fg = fg; }
void ListBox::SelectFirst( void ) { sel = 0; top = 0; ClampView(); }
void ListBox::Select( int index ) { sel = index; ClampView(); }

/*---- marking -------------------------------------------------------------- *
 * mark_count is maintained incrementally rather than counted on demand: the
 * status line asks for it on every keystroke, and a 20,000-entry archive would
 * be rescanned each time.                                                     */
boolean ListBox::IsMarked( int index ) const
{
    if ( !marks || index < 0 || index >= count ) return FALSE;
    return (boolean)( marks[index] != 0 );
}

boolean ListBox::CanMark( int index ) const
{
    if ( !marks || index < 0 || index >= count ) return FALSE;
    if ( can_mark && !can_mark( user, index ) ) return FALSE;
    return TRUE;
}

void ListBox::SetMark( int index, boolean on )
{
    if ( !marks || index < 0 || index >= count ) return;
    /* The veto stops a mark going ON.  Taking one OFF is always allowed, so a
     * rule that changes while rows are marked can never strand one. */
    if ( on && !CanMark( index ) ) return;
    if ( on && !marks[index] )      { marks[index] = 1; ++mark_count; }
    else if ( !on && marks[index] ) { marks[index] = 0; --mark_count; }
}

void ListBox::ToggleMark( int index )
{
    SetMark( index, (boolean)( !IsMarked( index ) ) );
}

void ListBox::MarkAll( void )
{
    int i;
    if ( !marks ) return;
    mark_count = 0;
    for ( i = 0; i < count; ++i )
    {
        marks[i] = (unsigned char)( CanMark( i ) ? 1 : 0 );
        mark_count += marks[i];
    }
}

void ListBox::UnmarkAll( void )
{
    if ( !marks ) return;
    memset( marks, 0, (size_t)count );
    mark_count = 0;
}

void ListBox::InvertMarks( void )
{
    int i;
    if ( !marks ) return;
    mark_count = 0;
    for ( i = 0; i < count; ++i )
    {
        marks[i] = (unsigned char)( ( !marks[i] && CanMark( i ) ) ? 1 : 0 );
        mark_count += marks[i];
    }
}

int ListBox::GetMarked( int *out, int max ) const
{
    int i, n = 0;
    if ( !out || !marks ) return 0;
    for ( i = 0; i < count && n < max; ++i )
        if ( marks[i] ) out[n++] = i;
    return n;
}

void ListBox::NotifyMarks( void )
{
    Draw();
    if ( on_marks ) on_marks( user );
}

int ListBox::InteriorCols( void ) const
{
    int w = region.end_column - region.column - 1;
    return ( w < 0 ) ? 0 : w;
}

int ListBox::InteriorRows( void ) const
{
    int h = region.end_row - region.row - 1;      // rows inside the border
    if ( header[0] ) --h;                         // header eats one
    return ( h < 0 ) ? 0 : h;
}

void ListBox::ClampView( void )
{
    int vis = InteriorRows();
    if ( count <= 0 ) { sel = 0; top = 0; return; }
    if ( sel < 0 ) sel = 0;
    if ( sel > count - 1 ) sel = count - 1;
    if ( vis <= 0 ) { top = sel; return; }
    if ( sel < top ) top = sel;
    if ( sel >= top + vis ) top = sel - vis + 1;
    if ( top > count - vis ) top = count - vis;
    if ( top < 0 ) top = 0;
}

void ListBox::Draw( void )
{
    if ( !IsTextMode() ) return;
    if ( region.end_row <= region.row || region.end_column <= region.column ) return;

    MouseHide guard( mouse );
    ClampView();

    short top1   = region.row + 1;              // graph.h is 1-based
    short left1  = region.column + 1;
    short bot1   = region.end_row + 1;
    short right1 = region.end_column + 1;

    DrawBox( top1, left1, bot1, right1, BORDER_SINGLE_LINE );
    FillBox( top1 + 1, left1 + 1, bot1 - 1, right1 - 1, bg_color );

    int  icols   = InteriorCols();
    int  hdrRows = header[0] ? 1 : 0;
    char line[130];

    /* Icons take over the mark column, but only on a screen that can draw
     * them: in text mode nothing below changes and the '*' glyph stays. */
    boolean useIcons = (boolean)( DfHasIcons() && ( row_icon || mark_icon >= 0 ) );

    if ( hdrRows )
    {
        FormatPad( line, header, icols );
        DfTextOut( (short)( top1 + 1 ), (short)( left1 + 1 ), line,
                   FG_BRIGHT_WHITE, bg_color );
    }

    int vis = InteriorRows();
    for ( int i = 0; i < vis; ++i )
    {
        int idx = top + i;
        short screenRow = (short)( top1 + 1 + hdrRows + i );
        boolean hi = (boolean)( idx == sel && count > 0 );
        boolean mk = IsMarked( idx );
        foreground_color_t rowFg = hi ? sel_fg : ( mk ? mark_fg : fg_color );
        background_color_t rowBg = hi ? sel_bg : bg_color;

        if ( idx < count && row_depth )
        {
            /* Tree row: the widget owns the name field at the left - mark
             * column, indent, marker, label - and the filler supplies the
             * columns to the right of it, which is what keeps those columns
             * in the same place whatever depth the row is at. */
            char raw[130];
            char tail[130];
            int  nw = name_width;

            if ( nw > icols ) nw = icols;
            BuildTreeName( idx, raw, nw );

            tail[0] = '\0';
            if ( filler ) filler( user, idx, tail, sizeof( tail ) );
            strncat( raw, tail, sizeof( raw ) - 1 - strlen( raw ) );
            FormatPad( line, raw, icols );
        }
        else if ( idx < count && filler )
        {
            char raw[130];
            filler( user, idx, raw, sizeof( raw ) );
            FormatPad( line, raw, icols );
        }
        else
            FormatPad( line, "", icols );

        /* The mark glyph goes in the row's first cell - the mark column - which
         * the filler is required to leave as padding.  It matters even when the
         * row is highlighted (the highlight colour hides mark_fg). */
        if ( mk && icols > 0 && !useIcons ) line[0] = MARK_GLYPH;

        DfTextOut( screenRow, (short)( left1 + 1 ), line, rowFg, rowBg );

        /* The icon goes on AFTER the row text, over the cell the text left
         * blank for it, and keeps the row's own background so the highlight
         * bar runs behind it unbroken. */
        if ( useIcons && icols > 0 && idx < count )
        {
            int ic = mk ? mark_icon
                        : ( row_icon ? row_icon( user, idx ) : DF_ICON_NONE );
            if ( ic >= 0 ) DfIconOut( screenRow, (short)( left1 + 1 ), ic, rowBg );
        }
    }

    /* vertical scrollbar on the right border when there are hidden rows */
    if ( count > vis && vis > 0 )
    {
        int  trackH = ( vis >= 3 ) ? vis - 2 : vis;   /* cells for the thumb  */
        int  maxTop = count - vis;
        int  thumb  = ( trackH * vis ) / count;
        int  thumbPos, r;
        char cell[2];

        if ( thumb < 1 ) thumb = 1;
        if ( thumb > trackH ) thumb = trackH;
        thumbPos = ( maxTop > 0 )
                   ? (int)( ( (long)top * ( trackH - thumb ) ) / maxTop ) : 0;

        cell[1] = '\0';
        for ( r = 0; r < vis; ++r )
        {
            if ( vis >= 3 && r == 0 )            cell[0] = (char)0x18;  /* up   */
            else if ( vis >= 3 && r == vis - 1 ) cell[0] = (char)0x19;  /* down */
            else
            {
                int local = ( vis >= 3 ) ? r - 1 : r;
                cell[0] = ( local >= thumbPos && local < thumbPos + thumb )
                          ? (char)0xDB : (char)0xB1;
            }
            /* one column in from the right border so it isn't clipped by
             * screen overscan at column 80 */
            DfTextOut( (short)( top1 + 1 + hdrRows + r ), (short)( right1 - 1 ),
                       cell, FG_BRIGHT_WHITE, bg_color );
        }
    }
}

boolean ListBox::HandleEvent( const Event &e )
{
    if ( e.kind == Event::EV_MOUSE )
    {
        int vis = InteriorRows();
        int firstRow = region.row + 1 + ( header[0] ? 1 : 0 );

        // Click on the right-border scrollbar: arrows step, track jumps.  (The
        // app loop delivers press events only, so this is click-to-position, no
        // drag.)  Moving 'sel' scrolls the view since ClampView follows it.
        if ( ( e.buttons & LEFT_BUTTON ) && count > vis && vis > 0 &&
             e.x == region.end_column - 1 )    /* scrollbar sits 1 col in */
        {
            int r = e.y - firstRow;
            if ( r >= 0 && r < vis )
            {
                if ( vis >= 3 && r == 0 )            { if ( sel > 0 ) --sel; }
                else if ( vis >= 3 && r == vis - 1 ) { if ( sel < count - 1 ) ++sel; }
                else
                {
                    int inTop  = ( vis >= 3 ) ? 1 : 0;
                    int trackH = ( vis >= 3 ) ? vis - 2 : vis;
                    int local  = r - inTop;
                    if ( local < 0 ) local = 0;
                    if ( local > trackH - 1 ) local = trackH - 1;
                    sel = ( trackH > 1 )
                          ? ( local * ( count - 1 ) ) / ( trackH - 1 ) : 0;
                }
                ClampView();
                Draw();
                return TRUE;
            }
        }

        // Marking with the mouse.  Two routes to the same toggle:
        //   - the RIGHT button anywhere on the row (the Norton Commander idiom),
        //   - the LEFT button in the mark column, the first interior column,
        //     where the '*' is drawn - a visible target for a left-only mouse.
        // Checked before the plain left-click select, and right first so a
        // chorded press (both buttons down) marks rather than just selects.
        if ( ( e.buttons & RIGHT_BUTTON ) ||
             ( ( e.buttons & LEFT_BUTTON ) && e.x == region.column + 1 ) )
        {
            if ( e.x > region.column && e.x < region.end_column )
            {
                int i = e.y - firstRow;
                if ( i >= 0 && i < vis )
                {
                    int idx = top + i;
                    if ( idx >= 0 && idx < count )
                    {
                        sel = idx;
                        ToggleMark( idx );
                        ClampView();
                        NotifyMarks();
                        return TRUE;
                    }
                }
            }
            if ( e.buttons & RIGHT_BUTTON ) return FALSE;   /* not our row */
        }

        // Left button pressed inside the list body selects that row, and a
        // SECOND press on the same row within half a second activates it -
        // the same gesture and timing FileBox uses for the disk browser, so
        // opening a folder feels identical in both.  The app loop delivers
        // presses only, so a press-pair is all there is to go on; that is
        // enough, since the release between them is not something a widget
        // can see anyway.
        if ( ( e.buttons & LEFT_BUTTON ) &&
             e.x > region.column && e.x < region.end_column )
        {
            int i = e.y - firstRow;
            if ( i >= 0 && i < vis )
            {
                int idx = top + i;

                /* A click ON the [+] / [-] marker opens or shuts that node
                 * and does nothing else - not a select, and certainly not
                 * half of a double-click.  It is a button, drawn where a
                 * button would be, and people aim at it. */
                if ( row_depth && idx >= 0 && idx < count &&
                     row_haskids && row_haskids( user, idx ) )
                {
                    int d  = row_depth( user, idx );
                    int nw = name_width;
                    int indent, m0;

                    if ( nw > InteriorCols() ) nw = InteriorCols();
                    indent = ( d > 0 ) ? d * 2 : 0;
                    /* the same clamp BuildTreeName applies, so the cells
                     * this accepts are the cells that were drawn */
                    if ( indent > nw - 6 ) indent = ( nw > 6 ) ? nw - 6 : 0;
                    m0 = region.column + 2 + indent;

                    if ( e.x >= m0 && e.x <= m0 + 2 )
                    {
                        sel = idx;
                        ToggleNode( idx );
                        return TRUE;
                    }
                }

                if ( idx >= 0 && idx < count )
                {
                    clock_t now = clock();
                    int     dbl = ( idx == sel && idx == last_click_row &&
                                    ( now - last_click_at ) <
                                        (clock_t)( CLOCKS_PER_SEC / 2 ) );
                    sel = idx;
                    ClampView();
                    Draw();

                    last_click_row = idx;
                    last_click_at  = now;

                    /* Fired AFTER the redraw: the action may replace the whole
                     * list, and it must not find a half-painted one
                     * underneath.  The timer is reset first so the action's
                     * own click cannot start another double-click.
                     *
                     * Deliberately its own callback rather than on_activate:
                     * Enter on a plain file extracts it, and an accidental
                     * double-click should not put up an extract dialog.  The
                     * caller wires this to navigation only. */
                    if ( dbl && on_dblclick )
                    {
                        last_click_row = -1;
                        on_dblclick( user, sel );
                    }
                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    if ( e.kind != Event::EV_KEY ) return FALSE;

    int vis = InteriorRows();
    if ( vis < 1 ) vis = 1;

    switch ( e.key )
    {
    case KEY_UP:               if ( sel > 0 ) --sel;            break;
    case KEY_DOWN:             if ( sel < count - 1 ) ++sel;    break;
    case ( 0x100 | _KB_PGUP ): sel -= vis; if ( sel < 0 ) sel = 0;                 break;
    case ( 0x100 | _KB_PGDN ): sel += vis; if ( sel > count - 1 ) sel = count - 1; break;
    case ( 0x100 | _KB_HOME ): sel = 0;                                            break;
    case ( 0x100 | _KB_END ):  sel = count - 1;                                    break;

    /* Space / Ins toggle the current row then step down, so a run of rows can
     * be marked by holding one key - the DOS file-manager idiom. */
    case ' ':
    case ( 0x100 | _KB_INS ):
        if ( count > 0 )
        {
            ToggleMark( sel );
            if ( sel < count - 1 ) ++sel;
            ClampView();
            NotifyMarks();
        }
        return TRUE;
    /* In a TREE these two open and shut the node under the highlight, which
     * is what they do in every DOS tree there has ever been.  In a flat list
     * they are Mark All and Unmark All, as before.  See LISTBOX.HPP. */
    case '+':
    case ( 0x100 | _KB_RIGHT ):
        if ( row_depth )
        {
            if ( count > 0 && row_expanded && !row_expanded( user, sel ) )
                ToggleNode( sel );
            return TRUE;
        }
        if ( e.key == '+' && count > 0 ) { MarkAll(); NotifyMarks(); }
        return ( e.key == '+' ) ? TRUE : FALSE;

    case '-':
    case ( 0x100 | _KB_LEFT ):
        if ( row_depth )
        {
            if ( count > 0 && row_expanded && row_expanded( user, sel ) )
                ToggleNode( sel );
            return TRUE;
        }
        if ( e.key == '-' && count > 0 ) { UnmarkAll(); NotifyMarks(); }
        return ( e.key == '-' ) ? TRUE : FALSE;
    case '*': if ( count > 0 ) { InvertMarks(); NotifyMarks(); } return TRUE;

    case KEY_ENTER:
        /* In a tree, Enter on a node that HAS children opens or shuts it -
         * the node is the thing you are pointing at, and opening it is what
         * pointing at it means.  On a leaf it activates, as everywhere. */
        if ( row_depth && count > 0 && row_haskids && row_haskids( user, sel ) )
        {
            ToggleNode( sel );
            return TRUE;
        }
        if ( on_activate && count > 0 ) on_activate( user, sel );
        return TRUE;
    default:
        return FALSE;
    }
    ClampView();
    Draw();
    return TRUE;
}
