/*===========================================================================
 * FIELDPAN.HPP - A panel of label / value rows.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * The shape of every "here is what this machine has" page ever written: a
 * column of labels, a column of values lined up beside them, headings and
 * blank rows in between.  It exists because that page was being drawn by
 * hand everywhere - a printf with tab stops, absolute row and column
 * numbers, and a value column kept in line by counting characters.
 *
 *     FieldPane pane( FG_BLACK, BG_WHITE );
 *     pane.SetRegion( 3, 2, 20, 60 );
 *     pane.SetTitle( "Processor" );
 *     pane.AddHeading( "Identification" );
 *     pane.Add( "Vendor", cpu.vendor );
 *     pane.AddF( "Speed", "%d MHz", cpu.mhz );
 *     pane.AddBlank();
 *     pane.Draw();
 *
 * THE VALUE COLUMN LINES ITSELF UP.  By default it sits two columns past
 * the longest label in the pane, so adding a longer label moves the whole
 * column rather than pushing one value out of line - which is what happens
 * when each row is positioned by hand.  SetValueColumn overrides it for a
 * caller that wants two panes to agree with each other.
 *
 * IT IS A PASSIVE VIEW.  No input, no scrolling: rows past the bottom of
 * the region are not drawn, and Count() against VisibleRows() says whether
 * that happened.  A page with more than a screenful of rows wants a
 * ListBox, which scrolls, rather than this.
 *===========================================================================*/
#ifndef FIELDPAN_HPP
#define FIELDPAN_HPP

#include "dosforms.hpp"
#include "structs.hpp"
#include <stdarg.h>

#define FP_MAX_ROWS    48
#define FP_LABEL_MAX   32
#define FP_VALUE_MAX   64

/* What a row is.  A heading has no value and is drawn in its own colour; a
 * blank is a spacer; a rule is a line of '-' across the pane. */
#define FP_FIELD    0
#define FP_HEADING  1
#define FP_BLANK    2
#define FP_RULE     3

struct FpRow
{
    char label[FP_LABEL_MAX];
    char value[FP_VALUE_MAX];
    int  kind;
    foreground_color_t color;      // per-row override, or FP_NO_COLOR
};

/* Not a colour: "use the pane's".  FG_BLACK is 0 and is a real choice, so
 * the sentinel has to be something no colour is. */
#define FP_NO_COLOR ( (foreground_color_t)0x7F )

class FieldPane : public DOSForms
{
    coordinates_t region;          // 0-based, border inclusive
    FpRow *rows;                   // on the heap: 48 x ~100 bytes
    int    count;
    int    value_col;              // 0 = work it out from the labels
    char   title[36];
    foreground_color_t label_fg;
    foreground_color_t value_fg;
    foreground_color_t heading_fg;

    FieldPane( const FieldPane & );
    FieldPane &operator=( const FieldPane & );

    int  ValueColumn( void ) const;     // resolved, in columns from the left
    FpRow *NewRow( int kind );          // 0 when the pane is full

public:
    FieldPane( const foreground_color_t foreground, const background_color_t background );
    virtual ~FieldPane( void );

    void SetRegion( short row, short col, short end_row, short end_col );
    void SetTitle( const char *text );          // on the top border; 0 = none
    void SetValueColumn( int col );             // 0 = automatic
    void SetColors( foreground_color_t label, foreground_color_t value,
                    foreground_color_t heading );

    /* Rows.  Each returns FALSE only when the pane is full, so a caller may
     * ignore the answer and the page simply stops where it ran out. */
    void    Clear( void );
    boolean Add( const char *label, const char *value );
    boolean AddF( const char *label, const char *fmt, ... );
    boolean AddHeading( const char *text );
    boolean AddBlank( void );
    boolean AddRule( void );

    /* The colour of the row just added - for the one figure on a page that
     * has to be red.  Ignored by blanks and rules. */
    void SetLastColor( foreground_color_t fg );

    int Count( void ) const;
    int VisibleRows( void ) const;      // how many the region has room for

    border_style_t border_style;        // BORDER_SINGLE_LINE; NONE = no frame

    virtual void Draw( void );
};

#endif
