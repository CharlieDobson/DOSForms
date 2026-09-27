/*===========================================================================
 * SEGBAR.HPP - A proportional bar split into coloured segments, with a legend.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * What a pie chart becomes on a character grid: one horizontal bar divided
 * in proportion to the values it is given, and under it a legend - a colour
 * swatch, a name, and the figure - one line per segment.
 *
 *     SegmentBar bar( FG_BLACK, BG_WHITE );
 *     bar.SetRegion( 2, 1, 9, 38 );
 *     bar.SetTitle( "Disk Usage - Drive C:" );
 *     bar.Add( "Used", 765900000UL, "765.9 MB", FG_BLUE );
 *     bar.Add( "Free", 249900000UL, "249.9 MB", FG_GREEN );
 *     bar.Draw();
 *
 * THE WIDGET DOES NOT FORMAT THE NUMBER.  Add takes the value twice: once
 * as a number, which is only ever used to work out how wide the segment is,
 * and once as the text to print beside the swatch.  Kilobytes, megabytes,
 * percentages, files, seconds - the caller knows which it has and how the
 * rest of its program writes them, and a toolkit that guessed would be
 * wrong in half the programs that used it.
 *
 * IT IS A PASSIVE VIEW, like FieldPane: no input, and it repaints when the
 * caller says so.
 *
 * A SEGMENT THAT ROUNDS TO NOTHING STILL GETS ONE CELL, as long as its
 * value is not zero.  A tenth of a percent of a disk is not nothing, and a
 * legend entry with no colour anywhere in the bar looks like a bug.
 *===========================================================================*/
#ifndef SEGBAR_HPP
#define SEGBAR_HPP

#include "dosforms.hpp"
#include "structs.hpp"

#define SB_MAX_SEGMENTS  6
#define SB_LABEL_MAX    20
#define SB_TEXT_MAX     20

struct SbSegment
{
    char               label[SB_LABEL_MAX];
    char               text[SB_TEXT_MAX];     // the figure, as the caller writes it
    unsigned long      value;                 // proportions only
    foreground_color_t color;
};

class SegmentBar : public DOSForms
{
    coordinates_t region;                     // 0-based, border inclusive
    SbSegment     seg[SB_MAX_SEGMENTS];
    int           count;
    int           bar_rows;                   // thickness of the bar
    int           legend;                     // draw one?
    char          title[36];

    SegmentBar( const SegmentBar & );
    SegmentBar &operator=( const SegmentBar & );

    unsigned long Total( void ) const;

public:
    SegmentBar( const foreground_color_t foreground, const background_color_t background );

    void SetRegion( short row, short col, short end_row, short end_col );
    void SetTitle( const char *text );        // on the top border; 0 = none
    void SetBarRows( int rows );              // 1..4, default 1
    void SetLegend( int on );                 // default on

    void    Clear( void );
    boolean Add( const char *label, unsigned long value, const char *valueText,
                 foreground_color_t color );

    int Count( void ) const { return count; }

    border_style_t border_style;              // BORDER_SINGLE_LINE; NONE = none

    virtual void Draw( void );
};

#endif
