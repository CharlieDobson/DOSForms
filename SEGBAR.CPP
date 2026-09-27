/*===========================================================================
 * SEGBAR.CPP - Segmented proportional bar with a legend.  See SEGBAR.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "segbar.hpp"
#include "moushide.hpp"
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>
#include <stdio.h>

#define SB_LINEBUF 168
#define SB_BLOCK   ((char)219)          /* CP437 full block  */
#define SB_EMPTY   ((char)176)          /* CP437 light shade */

SegmentBar::SegmentBar( const foreground_color_t foreground,
                        const background_color_t background )
    : DOSForms( foreground, background )
{
    region.row = region.column = region.end_row = region.end_column = 0;
    count    = 0;
    bar_rows = 1;
    legend   = 1;
    title[0] = '\0';
    border_style = BORDER_SINGLE_LINE;
}

void SegmentBar::SetRegion( short row, short col, short end_row, short end_col )
{
    region.row = row; region.column = col;
    region.end_row = end_row; region.end_column = end_col;
}

void SegmentBar::SetTitle( const char *text )
{
    title[0] = '\0';
    if ( !text ) return;
    strncpy( title, text, sizeof( title ) - 1 );
    title[sizeof( title ) - 1] = '\0';
}

void SegmentBar::SetBarRows( int rows )
{
    if ( rows < 1 ) rows = 1;
    if ( rows > 4 ) rows = 4;
    bar_rows = rows;
}

void SegmentBar::SetLegend( int on ) { legend = on ? 1 : 0; }
void SegmentBar::Clear( void )       { count = 0; }

boolean SegmentBar::Add( const char *label, unsigned long value,
                         const char *valueText, foreground_color_t color )
{
    SbSegment *s;

    if ( count >= SB_MAX_SEGMENTS ) return FALSE;
    s = &seg[count++];

    s->label[0] = '\0';
    if ( label ) { strncpy( s->label, label, sizeof( s->label ) - 1 );
                   s->label[sizeof( s->label ) - 1] = '\0'; }
    s->text[0] = '\0';
    if ( valueText ) { strncpy( s->text, valueText, sizeof( s->text ) - 1 );
                       s->text[sizeof( s->text ) - 1] = '\0'; }
    s->value = value;
    s->color = color;
    return TRUE;
}

unsigned long SegmentBar::Total( void ) const
{
    unsigned long t = 0;
    int i;

    for ( i = 0; i < count; ++i ) t += seg[i].value;
    return t;
}

void SegmentBar::Draw( void )
{
    short top1, left1, bot1, right1;
    int   inner, i, r;
    int   cells[SB_MAX_SEGMENTS];
    unsigned long total;
    int   used, widest;
    short barTop;
    char  line[SB_LINEBUF];

    if ( !IsTextMode() ) return;
    if ( region.end_row <= region.row || region.end_column <= region.column ) return;

    MouseHide guard( mouse );

    top1   = (short)( region.row + 1 );          /* the renderer is 1-based */
    left1  = (short)( region.column + 1 );
    bot1   = (short)( region.end_row + 1 );
    right1 = (short)( region.end_column + 1 );

    if ( border_style != BORDER_NONE )
    {
        DrawBox( top1, left1, bot1, right1, border_style );
        FillBox( (short)( top1 + 1 ), (short)( left1 + 1 ),
                 (short)( bot1 - 1 ), (short)( right1 - 1 ), bg_color );

        if ( title[0] )
        {
            char  t[40];
            int   tn;
            short tcol;

            sprintf( t, " %.34s ", title );
            tn   = (int)strlen( t );
            tcol = (short)( left1 + ( ( right1 - left1 + 1 ) - tn ) / 2 );
            if ( tcol < left1 + 1 ) tcol = (short)( left1 + 1 );
            DfTextOut( top1, tcol, t, fg_color, bg_color );
        }
        ++top1; ++left1; --right1;
    }
    else
        FillBox( top1, left1, bot1, right1, bg_color );

    inner = right1 - left1 + 1;
    if ( inner < 1 ) return;
    if ( inner > SB_LINEBUF - 1 ) inner = SB_LINEBUF - 1;

    /* A blank row above the bar, so it is not sitting on the frame. */
    barTop = (short)( top1 + 1 );

    /*---- how wide is each segment ---------------------------------------- *
     * Proportional, in whole cells, with the remainder given to the LAST
     * segment - somebody has to have it, and the last one is the one whose
     * right-hand end is the frame rather than another colour, so a cell
     * either way shows least there.
     *----------------------------------------------------------------------*/
    total = Total();
    used  = 0;
    for ( i = 0; i < count; ++i )
    {
        if ( total == 0 ) cells[i] = 0;
        else
        {
            cells[i] = (int)( ( (unsigned long)inner * seg[i].value ) / total );
            /* Not nothing: a segment with a value gets a cell to show it. */
            if ( cells[i] < 1 && seg[i].value > 0 ) cells[i] = 1;
        }
        used += cells[i];
    }
    /* The rounding above can overshoot as easily as undershoot when several
     * small segments were each given their one cell.  Take it back from the
     * widest, which is the only one that will not notice. */
    while ( used > inner )
    {
        widest = 0;
        for ( i = 1; i < count; ++i ) if ( cells[i] > cells[widest] ) widest = i;
        if ( cells[widest] <= 1 ) break;
        --cells[widest]; --used;
    }
    if ( count > 0 && used < inner ) cells[count - 1] += inner - used;

    /*---- the bar ---------------------------------------------------------- */
    for ( r = 0; r < bar_rows; ++r )
    {
        short sr = (short)( barTop + r );
        int   col = 0;

        if ( sr > bot1 - 1 ) break;

        if ( total == 0 )
        {
            /* Nothing to show: an empty trough rather than a blank gap, so
             * the pane looks like a bar with no data and not like a bar
             * that failed to draw. */
            for ( col = 0; col < inner; ++col ) line[col] = SB_EMPTY;
            line[inner] = '\0';
            DfTextOut( sr, left1, line, fg_color, bg_color );
            continue;
        }

        for ( i = 0; i < count; ++i )
        {
            int n;

            if ( cells[i] < 1 ) continue;
            for ( n = 0; n < cells[i] && n < SB_LINEBUF - 1; ++n ) line[n] = SB_BLOCK;
            line[n] = '\0';
            DfTextOut( sr, (short)( left1 + col ), line, seg[i].color, bg_color );
            col += cells[i];
        }
    }

    /*---- the legend ------------------------------------------------------- */
    if ( !legend ) return;

    {
        short lr = (short)( barTop + bar_rows + 1 );

        for ( i = 0; i < count && lr <= bot1 - 1; ++i, ++lr )
        {
            char sw[2];

            /* The swatch is the bar's own colour, so the eye can get from a
             * legend line to its piece of the bar without reading anything. */
            sw[0] = SB_BLOCK;
            sw[1] = '\0';
            DfTextOut( lr, (short)( left1 + 1 ), sw, seg[i].color, bg_color );

            sprintf( line, " %-12.12s %s", seg[i].label, seg[i].text );
            line[inner > 2 ? inner - 2 : 1] = '\0';
            DfTextOut( lr, (short)( left1 + 2 ), line, fg_color, bg_color );
        }
    }
}
