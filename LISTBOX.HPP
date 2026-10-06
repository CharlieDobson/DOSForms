/*===========================================================================
 * LISTBOX.HPP - Scrolling list widget.
 *
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * A DOSForms child that shows a vertical, scrollable list of rows inside a
 * bordered box with an optional column header.  Rows come from a caller
 * callback (RowFiller), so the widget knows nothing about where they come
 * from - an archive, a directory, a list of drives.
 * Keyboard: Up/Down, PgUp/PgDn, Home/End move the highlight (and scroll);
 * Enter fires an optional activate callback.
 *
 * MARKING (multi-select).  Any number of rows can be marked independently of
 * the highlight, so a caller can act on a set of rows rather than just one:
 *   Keyboard  Space / Ins toggle the highlighted row and step down,
 *             '+' marks all, '-' unmarks all, '*' inverts.
 *   Mouse     right-click a row toggles it; so does a left click in the
 *             leftmost interior column (the mark column).
 * A double left-click on a row fires the SetDblClick callback (see there).
 * A marked row is drawn in mark_fg with a '*' in its first character cell, so
 * a RowFiller must leave column 0 of its text as padding (the widget writes
 * over it).  MarksChanged fires whenever the set changes, for a status line.
 *===========================================================================*/
#ifndef LISTBOX_HPP
#define LISTBOX_HPP

#include "dosforms.hpp"
#include "structs.hpp"
#include <time.h>              // clock() / CLOCKS_PER_SEC for double-click

// Fill 'buf' (buflen incl. NUL) with the display text for row 'index'.
typedef void (*RowFiller)( void *user, int index, char *buf, int buflen );
// Fired when Enter is pressed on row 'index'.
typedef void (*RowActivate)( void *user, int index );
// Fired after the marked set changes (by any of the routes above).
typedef void (*MarksChanged)( void *user );
// Asked whether row 'index' may be marked at all.  Optional: with no callback
// set, every row can be.  It exists because some rows stand for navigation
// rather than data - the [..] row above all - and a mark on one of those is a
// promise the caller cannot keep: the status line counts it, the user believes
// something is selected, and the extract does nothing with it.
typedef int (*RowMarkable)( void *user, int index );
/* The icon to put in row 'index's mark column, as a DfIconRegister id, or
 * DF_ICON_NONE for none.  Only ever asked when the renderer can draw icons
 * (DfHasIcons), which today means the graphics screen; in text mode the
 * column keeps the '*' mark glyph it has always had and this is not called
 * at all.  See the note on SetMarkIcon about what happens to a marked row. */
typedef int (*RowIcon)( void *user, int index );

/*---- Tree rows ------------------------------------------------------------ *
 * A list becomes a TREE by answering four questions about each row - how
 * deep it is, whether it has children, whether those children are showing,
 * and how to open or close it - plus one that supplies the row's NAME
 * separately from the rest of its columns.
 *
 * WHY THE NAME IS SEPARATE.  Indentation moves the name to the right, and
 * if it moved the whole row with it then a tree's Size and Date columns
 * would sit in a different place on every line, which is unreadable.  So
 * the widget owns a NAME FIELD at the left - mark column, indent, [+]/[-]
 * marker and label, all clipped to the field's width - and the RowFiller
 * supplies only what comes AFTER it.  A tree with no other columns has a
 * filler that writes nothing.
 *
 * THE MODEL IS STILL THE CALLER'S.  The widget never holds a tree; it asks
 * about the row in front of it.  A caller keeps whatever it already has -
 * a flat array of nodes with a depth each is the usual shape - and the
 * rows the list shows are the ones currently visible in it.  Toggle is
 * therefore expected to CHANGE THE ROW COUNT, and says so by RETURNING the
 * new one: the widget cannot work that out and must not guess at it.
 *
 * KEYS CHANGE IN TREE MODE, and this is the one surprise worth stating
 * out loud: '+' and '-' expand and collapse rather than marking all and
 * unmarking all, because that is what those keys do in every DOS tree
 * there has ever been.  Space, Ins and '*' are untouched, and MarkAll()
 * and UnmarkAll() can still be called from a menu.
 *---------------------------------------------------------------------------*/
typedef int  (*RowDepth)( void *user, int index );      /* 0 = top level     */
typedef int  (*RowHasKids)( void *user, int index );
typedef int  (*RowExpanded)( void *user, int index );
typedef int  (*RowToggle)( void *user, int index );      /* returns the NEW
                                                         * row count */
typedef void (*RowLabel)( void *user, int index, char *buf, int buflen );

class ListBox : public DOSForms
{
    coordinates_t region;      // 0-based box incl. border
    int   count;               // number of rows
    int   sel;                 // highlighted row
    int   top;                 // first visible row
    RowFiller    filler;
    RowActivate  on_activate;
    RowActivate  on_dblclick;
    MarksChanged on_marks;
    RowMarkable  can_mark;
    RowIcon      row_icon;
    int          mark_icon;
    RowDepth     row_depth;         // tree mode: 0 when there is none
    RowHasKids   row_haskids;
    RowExpanded  row_expanded;
    RowToggle    row_toggle;
    RowLabel     row_label;
    int          name_width;        // columns the widget owns at the left
    void *user;
    char  header[128];

    unsigned char *marks;      // one byte per row (0/1), or 0 when count == 0
    int   mark_count;          // number of set bytes in 'marks'

    // Double-click detection: the row of the last left press and when it
    // happened.  A second press on the same row inside half a second fires
    // on_activate, exactly as Enter does.
    int     last_click_row;
    clock_t last_click_at;

    foreground_color_t sel_fg;
    background_color_t sel_bg;
    foreground_color_t mark_fg;

    int  InteriorRows( void ) const;   // usable text rows (minus border + header)
    int  InteriorCols( void ) const;
    void ClampView( void );
    void FreeMarks( void );
    // tree mode only; 'iconAt' receives the cell kept for the row's icon,
    // counted from the mark column, or -1 when there is none
    void BuildTreeName( int index, char *dst, int width, int *iconAt );
    int  TreeIndent( int index, int width ) const;
    boolean TreeIcons( void ) const;   // icons beside the name, not in col 0
    void NotifyMarks( void );          // redraw + fire on_marks

    ListBox( const ListBox & );
    ListBox &operator=( const ListBox & );

public:
    ListBox( const foreground_color_t foreground, const background_color_t background );
    virtual ~ListBox( void );

    void SetRegion( short row, short col, short end_row, short end_col );
    void SetHeader( const char *text );
    void SetData( int rowCount, RowFiller f, void *userData );
    void SetActivate( RowActivate a );
    /* Fired by a double left-click on a row.  Separate from SetActivate on
     * purpose: Enter's action and a double-click's action are not always the
     * same thing, and an accidental double-click should not do something
     * destructive or modal. */
    void SetDblClick( RowActivate d );
    void SetMarksChanged( MarksChanged m );
    /* Veto for rows that must never carry a mark.  Applies to every route -
     * Space, the right button, the mark column, Mark All and Invert - so there
     * is one place to state the rule rather than five. */
    void SetMarkable( RowMarkable m );
    /* Per-row icons for the mark column, on a screen that can draw them.
     * Costs no columns: the icon goes in the one cell the row text already
     * has to leave blank for the mark glyph.
     *
     * IN A TREE THE ICON GOES WITH THE NAME instead - after the [+] marker,
     * indented as far as the row is - because an icon left behind in the
     * margin says nothing about which level it belongs to.  That does cost
     * two columns of the name field, and only on a screen that draws icons;
     * the mark column is left to the mark icon alone. */
    void SetIconProvider( RowIcon fn );
    /* The icon a MARKED row shows in place of its type icon - a tick, by
     * convention.  Replacing the icon rather than sitting next to it is what
     * keeps the column one cell wide; the row's mark colour is still there
     * as well, so a marked row says so twice. */
    void SetMarkIcon( int icon );
    void SetSelHighlight( foreground_color_t fg, background_color_t bg );
    void SetMarkColor( foreground_color_t fg );

    /*---- tree mode -------------------------------------------------------- *
     * Turn it on by giving the five callbacks and the width of the name
     * field; pass a null 'depth' to turn it off again.  'nameWidth' counts
     * from the mark column, so it has to be wide enough for the deepest
     * indent plus "[-] " plus something of the name - about 24 is right for
     * a folder tree.  See the note above the typedefs.
     *----------------------------------------------------------------------*/
    void SetTreeProvider( RowDepth depth, RowHasKids hasKids,
                          RowExpanded expanded, RowToggle toggle,
                          RowLabel label, int nameWidth );
    boolean IsTree( void ) const { return (boolean)( row_depth ? TRUE : FALSE ); }

    /* Open or shut row 'index' - the same thing '+' and '-' do - taking the
     * new row count from what the toggle callback returns, then repaint.  A
     * caller whose model changed for some other reason (a rescan, say) calls
     * SetCount instead. */
    void ToggleNode( int index );

    /* The row count on its own, for a model that changed shape but not
     * identity.  Marks and the highlight are kept where they can be. */
    void SetCount( int rowCount );

    int  Count( void ) const    { return count; }
    int  Selected( void ) const { return sel; }
    void SelectFirst( void );
    /* Put the highlight on a given row, scrolling it into view.  Out-of-range
     * indices are clamped, so a caller restoring a remembered position after
     * the data changed underneath it need not check first. */
    void Select( int index );

    /*---- marking --------------------------------------------------------- */
    int  MarkCount( void ) const { return mark_count; }
    boolean IsMarked( int index ) const;
    boolean CanMark( int index ) const;
    void SetMark( int index, boolean on );
    void ToggleMark( int index );
    void MarkAll( void );
    void UnmarkAll( void );
    void InvertMarks( void );
    /* Copy the marked row indices, ascending, into 'out' (at most 'max' of
     * them).  Returns how many were written. */
    int  GetMarked( int *out, int max ) const;

    virtual void Draw( void );
    virtual boolean HandleEvent( const Event &e );
};

#endif
