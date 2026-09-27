/*===========================================================================
 * FIELDPAN.CPP - A panel of label / value rows.  See FIELDPAN.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "fieldpan.hpp"
#include "moushide.hpp"
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FP_LINEBUF 168

FieldPane::FieldPane( const foreground_color_t foreground,
                      const background_color_t background )
    : DOSForms( foreground, background )
{
    region.row = region.column = region.end_row = region.end_column = 0;
    rows = (FpRow *)malloc( (size_t)FP_MAX_ROWS * sizeof( FpRow ) );
    count = 0;
    value_col = 0;
    title[0] = '\0';
    label_fg   = foreground;
    value_fg   = FG_BRIGHT_WHITE;
    heading_fg = FG_YELLOW;
    border_style = BORDER_SINGLE_LINE;
}

FieldPane::~FieldPane( void )
{
    if ( rows ) free( rows );
}

void FieldPane::SetRegion( short row, short col, short end_row, short end_col )
{
    region.row = row; region.column = col;
    region.end_row = end_row; region.end_column = end_col;
}

void FieldPane::SetTitle( const char *text )
{
    title[0] = '\0';
    if ( !text ) return;
    strncpy( title, text, sizeof( title ) - 1 );
    title[sizeof( title ) - 1] = '\0';
}

void FieldPane::SetValueColumn( int col ) { value_col = ( col > 0 ) ? col : 0; }

void FieldPane::SetColors( foreground_color_t label, foreground_color_t value,
                           foreground_color_t heading )
{
    label_fg = label; value_fg = value; heading_fg = heading;
}

void FieldPane::Clear( void ) { count = 0; }

int FieldPane::Count( void ) const { return count; }

int FieldPane::VisibleRows( void ) const
{
    int h = region.end_row - region.row + 1;
    if ( border_style != BORDER_NONE ) h -= 2;
    return ( h < 0 ) ? 0 : h;
}

FpRow *FieldPane::NewRow( int kind )
{
    FpRow *r;

    if ( !rows || count >= FP_MAX_ROWS ) return 0;
    r = &rows[count++];
    r->label[0] = '\0';
    r->value[0] = '\0';
    r->kind  = kind;
    r->color = FP_NO_COLOR;
    return r;
}

boolean FieldPane::Add( const char *label, const char *value )
{
    FpRow *r = NewRow( FP_FIELD );

    if ( !r ) return FALSE;
    if ( label ) { strncpy( r->label, label, sizeof( r->label ) - 1 );
                   r->label[sizeof( r->label ) - 1] = '\0'; }
    if ( value ) { strncpy( r->value, value, sizeof( r->value ) - 1 );
                   r->value[sizeof( r->value ) - 1] = '\0'; }
    return TRUE;
}

boolean FieldPane::AddF( const char *label, const char *fmt, ... )
{
    FpRow  *r = NewRow( FP_FIELD );
    va_list ap;
    char    buf[FP_VALUE_MAX * 2];

    if ( !r ) return FALSE;
    if ( label ) { strncpy( r->label, label, sizeof( r->label ) - 1 );
                   r->label[sizeof( r->label ) - 1] = '\0'; }

    buf[0] = '\0';
    if ( fmt ) {
        va_start( ap, fmt );
        vsprintf( buf, fmt, ap );
        va_end( ap );
    }
    strncpy( r->value, buf, sizeof( r->value ) - 1 );
    r->value[sizeof( r->value ) - 1] = '\0';
    return TRUE;
}

boolean FieldPane::AddHeading( const char *text )
{
    FpRow *r = NewRow( FP_HEADING );

    if ( !r ) return FALSE;
    if ( text ) { strncpy( r->label, text, sizeof( r->label ) - 1 );
                  r->label[sizeof( r->label ) - 1] = '\0'; }
    return TRUE;
}

boolean FieldPane::AddBlank( void ) { return (boolean)( NewRow( FP_BLANK ) ? TRUE : FALSE ); }
boolean FieldPane::AddRule( void )  { return (boolean)( NewRow( FP_RULE  ) ? TRUE : FALSE ); }

void FieldPane::SetLastColor( foreground_color_t fg )
{
    if ( rows && count > 0 ) rows[count - 1].color = fg;
}

/* Two columns past the longest LABEL - not past the longest anything, and
 * not a number typed in by the caller.  Headings are left out of the
 * measurement on purpose: a heading spans the pane, so letting one push the
 * value column right would indent every value on the page to make room for
 * a word that is not in that column at all. */
int FieldPane::ValueColumn( void ) const
{
    int widest = 0, i, n;

    if ( value_col > 0 ) return value_col;
    if ( !rows ) return 2;

    for ( i = 0; i < count; ++i )
    {
        if ( rows[i].kind != FP_FIELD ) continue;
        n = (int)strlen( rows[i].label );
        if ( n > widest ) widest = n;
    }
    return widest + 2;
}

void FieldPane::Draw( void )
{
    short top1, left1, bot1, right1;
    int   inner, vcol, vis, i, drawn;
    char  line[FP_LINEBUF];

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
            /* On the top border, centred, with a space each side so the
             * line-drawing characters do not touch the letters. */
            char  t[40];
            int   tn;
            short tcol;

            sprintf( t, " %.34s ", title );
            tn   = (int)strlen( t );
            tcol = (short)( left1 + ( ( right1 - left1 + 1 ) - tn ) / 2 );
            if ( tcol < left1 + 1 ) tcol = (short)( left1 + 1 );
            DfTextOut( top1, tcol, t, fg_color, bg_color );
        }
        ++top1; ++left1; --right1;               /* inside the frame now */
    }
    else
        FillBox( top1, left1, bot1, right1, bg_color );

    inner = right1 - left1 + 1;
    if ( inner < 1 ) return;
    if ( inner > FP_LINEBUF - 1 ) inner = FP_LINEBUF - 1;

    vcol = ValueColumn();
    if ( vcol > inner - 1 ) vcol = ( inner > 2 ) ? inner - 2 : 0;

    vis = VisibleRows();
    drawn = 0;

    for ( i = 0; i < count && drawn < vis; ++i, ++drawn )
    {
        FpRow             *r  = &rows[i];
        short              sr = (short)( top1 + drawn );
        foreground_color_t fg;
        int                n, j;

        switch ( r->kind )
        {
        case FP_BLANK:
            continue;                    /* the fill already left it blank */

        case FP_RULE:
            for ( n = 0; n < inner; ++n ) line[n] = (char)196;   /* CP437 - */
            line[inner] = '\0';
            DfTextOut( sr, left1, line, fg_color, bg_color );
            continue;

        case FP_HEADING:
            fg = ( r->color != FP_NO_COLOR ) ? r->color : heading_fg;
            for ( n = 0; r->label[n] && n < inner; ++n ) line[n] = r->label[n];
            while ( n < inner ) line[n++] = ' ';
            line[inner] = '\0';
            DfTextOut( sr, left1, line, fg, bg_color );
            continue;

        default:
            break;
        }

        /* The label, then spaces out to the value column, then the value -
         * built as ONE string and written in ONE call, so a row cannot be
         * left half-painted and the old row's tail cannot show through. */
        for ( n = 0; r->label[n] && n < vcol - 1 && n < inner; ++n )
            line[n] = r->label[n];
        while ( n < vcol && n < inner ) line[n++] = ' ';
        for ( j = 0; r->value[j] && n < inner; ++j ) line[n++] = r->value[j];
        while ( n < inner ) line[n++] = ' ';
        line[inner] = '\0';

        /* Label and value want different colours, which is two writes of the
         * same buffer: the whole row in the label's colour, then the value
         * part over the top of it in the value's. */
        fg = ( r->color != FP_NO_COLOR ) ? r->color : value_fg;
        DfTextOut( sr, left1, line, label_fg, bg_color );
        if ( r->value[0] && vcol < inner )
            DfTextOut( sr, (short)( left1 + vcol ), line + vcol, fg, bg_color );
    }
}
