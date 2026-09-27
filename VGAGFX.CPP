/*===========================================================================
 * VGAGFX.CPP  -  VGA 640x480x16 renderer for DOSForms.  See VGAGFX.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS - 16-bit real mode (large
 *         model) and 32-bit protected mode (DOS/4GW, DOS/32A)
 *===========================================================================*/

#include <stdlib.h>
#include <string.h>
#include <conio.h>              /* inp / outp                                */
#include <dos.h>                /* _disable / _enable (Watcom: via i86.h)    */
#include <graph.h>              /* _setvideomode - the mode switch only      */
#include "screen.hpp"
#include "vgagfx.hpp"
#include "dosforms.hpp"

/*---- The screen ----------------------------------------------------------- */
#define G_COLS        80
#define G_ROWS        30
#define CELL_W         8
#define CELL_H        16
#define LINE_BYTES    80        /* 640 pixels / 8 per byte, per plane         */
#define CELL_ROW_BYTES ( CELL_H * LINE_BYTES )  /* one row of cells, per plane */

/*---- VGA registers -------------------------------------------------------- */
#define SEQ_INDEX   0x3C4
#define SEQ_DATA    0x3C5
#define GC_INDEX    0x3CE
#define GC_DATA     0x3CF

/* A pointer into the frame buffer.  Under a 32-bit extender the low megabyte
 * is mapped one-to-one, the same assumption the text renderer makes about
 * 0xB8000; in real mode it is segment A000h.  As in DOSFORMS.CPP the
 * difference has to be in the TYPE, or a small-data model would silently
 * truncate it to the data segment.
 *
 * Every offset into it is computed UNSIGNED.  Thirty rows of cells is 38,400
 * bytes a plane, which fits a real-mode segment but not a signed 16-bit int:
 * row 26 onwards would go negative, and the bottom four rows of the screen
 * would be painted over the top of the one before the frame buffer. */
#ifdef __386__
typedef unsigned char *df_vgaptr_t;
static df_vgaptr_t const VGA = (df_vgaptr_t)0xA0000;
#else
typedef unsigned char far *df_vgaptr_t;
static df_vgaptr_t const VGA = (df_vgaptr_t)0xA0000000UL;
#endif

/* The first byte of cell (row, col) in whichever plane the map mask has on,
 * both 0-based. */
static df_vgaptr_t CellByte( int row, int col )
{
    return VGA + ( (unsigned)row * CELL_ROW_BYTES + (unsigned)col );
}

static void SeqWrite( int index, int value )
{
    outp( SEQ_INDEX, index );
    outp( SEQ_DATA, value );
}

static void GcWrite( int index, int value )
{
    outp( GC_INDEX, index );
    outp( GC_DATA, value );
}

/*===========================================================================*
 * The font
 *
 * Taken from the card rather than built in, so the glyphs are the ones the
 * machine's own text mode uses - box drawing, the shade blocks, whatever
 * codepage has been loaded over it.  The character generator lives in bit
 * plane 2 while the card is in a TEXT mode, 32 bytes to a character of which
 * an 8x16 font uses the first 16, so it has to be lifted before the mode
 * switch, not after: in mode 12h plane 2 is pixels.
 *===========================================================================*/

static unsigned char s_font[256 * CELL_H];

static boolean GrabRomFont( void )
{
    unsigned char save_seq2, save_seq4, save_gc4, save_gc5, save_gc6;
    int  text_gc6 = ( DOSForms::video.mode == _TEXTMONO ) ? 0x0A : 0x0E;
    int  g, y;
    boolean ok;

    outp( SEQ_INDEX, 2 ); save_seq2 = (unsigned char)inp( SEQ_DATA );
    outp( SEQ_INDEX, 4 ); save_seq4 = (unsigned char)inp( SEQ_DATA );
    outp( GC_INDEX,  4 ); save_gc4  = (unsigned char)inp( GC_DATA );
    outp( GC_INDEX,  5 ); save_gc5  = (unsigned char)inp( GC_DATA );
    outp( GC_INDEX,  6 ); save_gc6  = (unsigned char)inp( GC_DATA );

    _disable();
    SeqWrite( 0, 0x01 );        /* synchronous reset                          */
    SeqWrite( 2, 0x04 );        /* write plane 2                              */
    SeqWrite( 4, 0x06 );        /* sequential access, extended memory         */
    SeqWrite( 0, 0x03 );        /* end reset                                  */
    GcWrite( 4, 0x02 );         /* read plane 2                               */
    GcWrite( 5, 0x00 );         /* read mode 0, write mode 0                  */
    GcWrite( 6, 0x00 );         /* map A0000-BFFFF, alphanumeric              */

    for ( g = 0; g < 256; ++g )
        for ( y = 0; y < CELL_H; ++y )
            s_font[g * CELL_H + y] = VGA[(unsigned)g * 32 + y];

    SeqWrite( 0, 0x01 );
    SeqWrite( 2, save_seq2 );
    SeqWrite( 4, save_seq4 );
    SeqWrite( 0, 0x03 );
    GcWrite( 4, save_gc4 );
    GcWrite( 5, save_gc5 );
    GcWrite( 6, (unsigned char)( save_gc6 ? save_gc6 : text_gc6 ) );
    _enable();

    /* Did we actually get a font?  A machine where the plane-2 trick does
     * not work (an emulator that does not model the character generator, a
     * card behind a driver that has taken the registers over) hands back a
     * block of zeros or a block of 0xFF, and either would paint a screen of
     * solid nothing.  Capital A is the test: it has set pixels and clear
     * ones, so it fails both ways of being wrong. */
    ok = FALSE;
    for ( y = 0; y < CELL_H; ++y )
    {
        unsigned char b = s_font['A' * CELL_H + y];
        if ( b != 0x00 && b != 0xFF ) { ok = TRUE; break; }
    }
    return ok;
}

/*===========================================================================*
 * Icons
 *
 * Compiled from the art in SCREEN.HPP's format to four planes plus an
 * opacity mask, the first time one is asked for.  With the mask in hand a
 * blit needs no read-modify-write: the transparent pixels take the cell's
 * background colour, which we know, so every byte can be computed and
 * written outright.
 *===========================================================================*/

struct IconBits
{
    unsigned char plane[4][CELL_H];   /* colour bits, already masked          */
    unsigned char mask[CELL_H];       /* 1 = opaque                           */
    boolean       compiled;
};

static IconBits s_icons[DF_ICON_MAX];

static int ArtColor( char c )
{
    if ( c >= '0' && c <= '9' ) return c - '0';
    if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    return -1;                        /* '.' and anything else: transparent   */
}

static const IconBits *IconFor( int id )
{
    const char * const *art;
    int rows = 0, y, x, p;

    if ( id < 0 || id >= DF_ICON_MAX ) return 0;
    if ( s_icons[id].compiled ) return &s_icons[id];

    art = DfIconArt( id, &rows );
    if ( !art ) return 0;

    memset( &s_icons[id], 0, sizeof( s_icons[id] ) );
    for ( y = 0; y < CELL_H && y < rows; ++y )
    {
        const char *row = art[y];
        if ( !row ) continue;
        for ( x = 0; x < CELL_W && row[x]; ++x )
        {
            int color = ArtColor( row[x] );
            unsigned char bit = (unsigned char)( 0x80 >> x );
            if ( color < 0 ) continue;
            s_icons[id].mask[y] = (unsigned char)( s_icons[id].mask[y] | bit );
            for ( p = 0; p < 4; ++p )
                if ( color & ( 1 << p ) )
                    s_icons[id].plane[p][y] = (unsigned char)( s_icons[id].plane[p][y] | bit );
        }
    }
    s_icons[id].compiled = TRUE;
    return &s_icons[id];
}

/*===========================================================================*
 * Pictures
 *
 * The pixels behind a rectangle of cells, kept in the planar form the
 * blitter writes - four planes, one bit a pixel, a byte for every eight
 * pixels of a row - so painting one of its cells is sixteen byte copies a
 * plane and no conversion at all.  Each plane is its own allocation: a
 * whole-screen picture would be 38,400 bytes a plane, which a 16-bit program
 * can allocate four times over but not as one block.
 *===========================================================================*/

struct PictureBits
{
    unsigned char *plane[4];
    short   top, left;          /* 1-based cell of the top-left corner         */
    short   rows, cols;         /* size in cells; a row is 'cols' bytes a plane */
    int     bg;
    boolean dirty;              /* pixels changed since the last flush         */
};

static PictureBits s_pics[DF_PICTURE_MAX];

/*===========================================================================*
 * The renderer
 *===========================================================================*/

class DfVgaRenderer : public DfRenderer
{
public:
    DfVgaRenderer( void );

    boolean Open( void );           /* allocate the grids                     */
    void    Close( void );

    virtual short Rows( void ) const { return G_ROWS; }
    virtual short Cols( void ) const { return G_COLS; }
    virtual short CellW( void ) const { return CELL_W; }
    virtual short CellH( void ) const { return CELL_H; }
    virtual boolean IsGraphics( void ) const { return TRUE; }
    virtual boolean CanDraw( void ) const { return (boolean)( cells != 0 ); }

    virtual void TextOut( short row, short col, const char *s,
                          foreground_color_t fg, background_color_t bg );
    virtual void FillRect( short top, short left, short bottom, short right,
                           background_color_t bg );
    virtual void Clear( foreground_color_t fg, background_color_t bg );
    virtual void ClearRow( short row, foreground_color_t fg, background_color_t bg );
    virtual void Shade( short top, short left, short bottom, short right );
    virtual void ProgressBar( short row, short col, short cells_wide,
                              unsigned long done, unsigned long total,
                              foreground_color_t fg, background_color_t bg );
    virtual void CellsRead( short top, short left, short bottom, short right,
                            df_cell_t *buf );
    virtual void CellsWrite( short top, short left, short bottom, short right,
                             const df_cell_t *buf );
    virtual df_cell_t CellAt( short row, short col ) const;
    virtual boolean HasIcons( void ) const { return TRUE; }
    virtual void IconOut( short row, short col, int icon, background_color_t bg );
    virtual boolean HasPictures( void ) const { return TRUE; }
    virtual boolean PictureOpen( int id, short top, short left,
                                 short bottom, short right, int bg );
    virtual void PictureSpan( int id, int y, int x0, int x1, int color );
    virtual void PictureClose( int id );
    virtual void Flush( void );

private:
    df_cell_t *cells;       /* what the widgets have asked for                */
    df_cell_t *shown;       /* what is actually on the glass                  */

    void Put( short row, short col, df_cell_t c );
    void BlitRun( int row, int col, int count, unsigned char attr,
                  const df_cell_t *src );
    void BlitIcon( int row, int col, int id, unsigned char attr );
    void BlitBar( int row, int col, unsigned char attr, unsigned char extra );
    void BlitPicture( int row, int col, df_cell_t c );
};

static DfVgaRenderer s_vga;

DfVgaRenderer::DfVgaRenderer( void )
{
    cells = 0;
    shown = 0;
}

boolean DfVgaRenderer::Open( void )
{
    int n = G_ROWS * G_COLS;
    if ( cells ) return TRUE;
    cells = (df_cell_t *)calloc( (size_t)n, sizeof( df_cell_t ) );
    shown = (df_cell_t *)calloc( (size_t)n, sizeof( df_cell_t ) );
    if ( !cells || !shown ) { Close(); return FALSE; }
    return TRUE;
}

void DfVgaRenderer::Close( void )
{
    int id;
    for ( id = 0; id < DF_PICTURE_MAX; ++id ) PictureClose( id );
    if ( cells ) { free( cells ); cells = 0; }
    if ( shown ) { free( shown ); shown = 0; }
}

/*---- Writing to the grid -------------------------------------------------- */

void DfVgaRenderer::Put( short row, short col, df_cell_t c )
{
    if ( row < 1 || row > G_ROWS || col < 1 || col > G_COLS ) return;
    cells[ ( row - 1 ) * G_COLS + ( col - 1 ) ] = c;
}

void DfVgaRenderer::TextOut( short row, short col, const char *s,
                             foreground_color_t fg, background_color_t bg )
{
    unsigned char attr = (unsigned char)( ( (int)fg & 0x0F ) | ( ( (int)bg & 0x0F ) << 4 ) );
    if ( !s || !cells ) return;
    for ( ; *s && col <= G_COLS; ++s, ++col )
        Put( row, col, DF_CELL_MAKE( *s, attr ) );
}

void DfVgaRenderer::FillRect( short top, short left, short bottom, short right,
                              background_color_t bg )
{
    short r, c;
    if ( !cells || bottom < top || right < left ) return;
    if ( top < 1 ) top = 1;
    if ( left < 1 ) left = 1;
    if ( bottom > G_ROWS ) bottom = G_ROWS;
    if ( right > G_COLS ) right = G_COLS;

    for ( r = top; r <= bottom; ++r )
        for ( c = left; c <= right; ++c )
        {
            /* Background only, like the text renderer: the foreground of a
             * blanked cell is whatever it was, which nothing can see. */
            df_cell_t old = cells[ ( r - 1 ) * G_COLS + ( c - 1 ) ];
            unsigned char attr = (unsigned char)( ( DF_CELL_ATTR( old ) & 0x0F ) |
                                                  ( ( (int)bg & 0x0F ) << 4 ) );
            cells[ ( r - 1 ) * G_COLS + ( c - 1 ) ] = DF_CELL_MAKE( ' ', attr );
        }
}

void DfVgaRenderer::Clear( foreground_color_t fg, background_color_t bg )
{
    unsigned char attr = (unsigned char)( ( (int)fg & 0x0F ) | ( ( (int)bg & 0x0F ) << 4 ) );
    df_cell_t blank = DF_CELL_MAKE( ' ', attr );
    int i, n = G_ROWS * G_COLS;
    if ( !cells ) return;
    for ( i = 0; i < n; ++i ) cells[i] = blank;
}

void DfVgaRenderer::ClearRow( short row, foreground_color_t fg, background_color_t bg )
{
    unsigned char attr = (unsigned char)( ( (int)fg & 0x0F ) | ( ( (int)bg & 0x0F ) << 4 ) );
    df_cell_t blank = DF_CELL_MAKE( ' ', attr );
    short c;
    if ( !cells || row < 1 || row > G_ROWS ) return;
    for ( c = 1; c <= G_COLS; ++c ) cells[ ( row - 1 ) * G_COLS + ( c - 1 ) ] = blank;
}

void DfVgaRenderer::Shade( short top, short left, short bottom, short right )
{
    short r, c;
    if ( !cells || bottom < top || right < left ) return;
    if ( top < 1 ) top = 1;
    if ( left < 1 ) left = 1;
    if ( bottom > G_ROWS ) bottom = G_ROWS;
    if ( right > G_COLS ) right = G_COLS;

    /* A real drop shadow, which is the one place this renderer is allowed to
     * look different from the text one: instead of erasing the cells to
     * black, keep what is under them and recolour it dark grey on black, so
     * the window casts a shadow over the desktop rather than a hole in it. */
    for ( r = top; r <= bottom; ++r )
        for ( c = left; c <= right; ++c )
        {
            df_cell_t *p = &cells[ ( r - 1 ) * G_COLS + ( c - 1 ) ];
            unsigned char ex = DF_CELL_EXTRA( *p );
            /* A picture dims its own pixels when it is painted - keep the
             * cell, flag it.  An icon or a bar slice has no character to
             * dim; blank it. */
            if ( DF_EXTRA_IS_PICTURE( ex ) ) *p = *p | DF_CELL_SHADED;
            else if ( ex != DF_EXTRA_NONE ) *p = DF_CELL_MAKE( ' ', 0x08 );
            else *p = DF_CELL_MAKE( DF_CELL_CHAR( *p ), 0x08 );
        }
}

void DfVgaRenderer::ProgressBar( short row, short col, short cells_wide,
                                 unsigned long done, unsigned long total,
                                 foreground_color_t fg, background_color_t bg )
{
    unsigned char attr = (unsigned char)( ( (int)fg & 0x0F ) | ( ( (int)bg & 0x0F ) << 4 ) );
    unsigned long pixels, filled;
    short i;

    if ( !cells || cells_wide < 1 ) return;
    if ( col + cells_wide - 1 > G_COLS ) cells_wide = (short)( G_COLS - col + 1 );
    if ( cells_wide < 1 ) return;

    pixels = (unsigned long)cells_wide * CELL_W;
    if ( total == 0 )         filled = 0;
    else if ( done >= total ) filled = pixels;
    else filled = ( pixels * done ) / total;

    for ( i = 0; i < cells_wide; ++i )
    {
        unsigned long start = (unsigned long)i * CELL_W;
        int here;
        unsigned char extra;

        if ( filled <= start )                  here = 0;
        else if ( filled >= start + CELL_W )    here = CELL_W;
        else                                    here = (int)( filled - start );

        extra = (unsigned char)( DF_EXTRA_BAR | here );
        if ( i == 0 )               extra = (unsigned char)( extra | DF_EXTRA_BAR_LEFT );
        if ( i == cells_wide - 1 )  extra = (unsigned char)( extra | DF_EXTRA_BAR_RIGHT );

        Put( row, (short)( col + i ), DF_CELL_WITH( DF_CELL_MAKE( 0, attr ), extra ) );
    }
}

void DfVgaRenderer::IconOut( short row, short col, int icon, background_color_t bg )
{
    unsigned char attr;
    df_cell_t old;

    if ( !cells || row < 1 || row > G_ROWS || col < 1 || col > G_COLS ) return;
    if ( icon < 0 || icon >= DF_ICON_MAX ) return;

    old  = cells[ ( row - 1 ) * G_COLS + ( col - 1 ) ];
    attr = (unsigned char)( ( DF_CELL_ATTR( old ) & 0x0F ) | ( ( (int)bg & 0x0F ) << 4 ) );
    Put( row, col, DF_CELL_WITH( DF_CELL_MAKE( 0, attr ), DF_EXTRA_ICON( icon ) ) );
}

/*---- Pictures ------------------------------------------------------------- */

boolean DfVgaRenderer::PictureOpen( int id, short top, short left,
                                    short bottom, short right, int bg )
{
    PictureBits *pic;
    short    rows, cols, r, c;
    unsigned bytes;
    int      p;
    df_cell_t cell;

    if ( !cells || id < 0 || id >= DF_PICTURE_MAX ) return FALSE;
    if ( top < 1 ) top = 1;
    if ( left < 1 ) left = 1;
    if ( bottom > G_ROWS ) bottom = G_ROWS;
    if ( right > G_COLS ) right = G_COLS;
    if ( bottom < top || right < left ) return FALSE;

    pic   = &s_pics[id];
    rows  = (short)( bottom - top + 1 );
    cols  = (short)( right - left + 1 );
    bytes = (unsigned)rows * CELL_H * (unsigned)cols;

    /* The same size as last time keeps its planes - which is every redraw of
     * a chart whose data changed and whose place on the screen did not. */
    if ( pic->plane[0] && ( pic->rows != rows || pic->cols != cols ) )
        PictureClose( id );
    if ( !pic->plane[0] )
    {
        for ( p = 0; p < 4; ++p )
        {
            pic->plane[p] = (unsigned char *)malloc( bytes );
            if ( !pic->plane[p] ) { PictureClose( id ); return FALSE; }
        }
    }

    pic->top  = top;
    pic->left = left;
    pic->rows = rows;
    pic->cols = cols;
    pic->bg   = bg & 0x0F;
    for ( p = 0; p < 4; ++p )
        memset( pic->plane[p], ( ( pic->bg >> p ) & 1 ) ? 0xFF : 0x00, bytes );

    /* The background goes in the attribute too, so a cell of the picture
     * that ends up somewhere the picture no longer covers paints as plain
     * background rather than as nothing in particular. */
    cell = DF_CELL_WITH( DF_CELL_MAKE( ' ', (unsigned char)( pic->bg << 4 ) ),
                         DF_EXTRA_PICTURE( id ) );
    for ( r = top; r <= bottom; ++r )
        for ( c = left; c <= right; ++c )
            cells[ ( r - 1 ) * G_COLS + ( c - 1 ) ] = cell;

    pic->dirty = TRUE;
    return TRUE;
}

void DfVgaRenderer::PictureSpan( int id, int y, int x0, int x1, int color )
{
    PictureBits  *pic;
    int           width, bx0, bx1, p, b;
    unsigned      row;
    unsigned char m0, m1;

    if ( id < 0 || id >= DF_PICTURE_MAX ) return;
    pic = &s_pics[id];
    if ( !pic->plane[0] ) return;

    if ( x0 > x1 ) { int t = x0; x0 = x1; x1 = t; }
    width = pic->cols * CELL_W;
    if ( y < 0 || y >= pic->rows * CELL_H ) return;
    if ( x0 < 0 ) x0 = 0;
    if ( x1 >= width ) x1 = width - 1;
    if ( x0 > x1 ) return;

    row = (unsigned)y * (unsigned)pic->cols;
    bx0 = x0 >> 3;
    bx1 = x1 >> 3;
    m0  = (unsigned char)( 0xFF >> ( x0 & 7 ) );              /* x0 to its byte's end   */
    m1  = (unsigned char)( 0xFF << ( 7 - ( x1 & 7 ) ) );      /* its byte's start to x1 */
    if ( bx0 == bx1 ) m0 = m1 = (unsigned char)( m0 & m1 );

    for ( p = 0; p < 4; ++p )
    {
        unsigned char *line = pic->plane[p] + row;
        if ( ( color >> p ) & 1 )
        {
            line[bx0] = (unsigned char)( line[bx0] | m0 );
            for ( b = bx0 + 1; b < bx1; ++b ) line[b] = 0xFF;
            if ( bx1 != bx0 ) line[bx1] = (unsigned char)( line[bx1] | m1 );
        }
        else
        {
            line[bx0] = (unsigned char)( line[bx0] & (unsigned char)~m0 );
            for ( b = bx0 + 1; b < bx1; ++b ) line[b] = 0x00;
            if ( bx1 != bx0 ) line[bx1] = (unsigned char)( line[bx1] & (unsigned char)~m1 );
        }
    }
    pic->dirty = TRUE;
}

void DfVgaRenderer::PictureClose( int id )
{
    int p;
    if ( id < 0 || id >= DF_PICTURE_MAX ) return;
    for ( p = 0; p < 4; ++p )
    {
        if ( s_pics[id].plane[p] ) free( s_pics[id].plane[p] );
        s_pics[id].plane[p] = 0;
    }
    s_pics[id].dirty = FALSE;
}

void DfVgaRenderer::CellsRead( short top, short left, short bottom, short right,
                               df_cell_t *buf )
{
    short r, c;
    if ( !cells || !buf || bottom < top || right < left ) return;
    for ( r = top; r <= bottom; ++r )
        for ( c = left; c <= right; ++c )
            *buf++ = ( r >= 1 && r <= G_ROWS && c >= 1 && c <= G_COLS )
                     ? cells[ ( r - 1 ) * G_COLS + ( c - 1 ) ]
                     : DF_CELL_MAKE( ' ', 0x07 );
}

void DfVgaRenderer::CellsWrite( short top, short left, short bottom, short right,
                                const df_cell_t *buf )
{
    short r, c;
    if ( !cells || !buf || bottom < top || right < left ) return;
    for ( r = top; r <= bottom; ++r )
        for ( c = left; c <= right; ++c )
            Put( r, c, *buf++ );
}

df_cell_t DfVgaRenderer::CellAt( short row, short col ) const
{
    if ( !cells || row < 1 || row > G_ROWS || col < 1 || col > G_COLS )
        return DF_CELL_MAKE( ' ', 0x07 );
    return cells[ ( row - 1 ) * G_COLS + ( col - 1 ) ];
}

/*===========================================================================*
 * Blitting
 *
 * Write mode 0 with set/reset DISABLED, which puts the CPU byte straight
 * into whichever planes the map mask has enabled.  A run of cells in one
 * colour pair costs four port writes - one map mask per plane - and then
 * nothing but memory.
 *
 * THE CARD IS NOT OURS ALONE, so those registers are set at the start of
 * every flush and the map mask is put back to all-planes at the end of one,
 * rather than being set once when the mode is entered.  The mouse driver
 * draws its own cursor, from an interrupt, through these same registers, and
 * it is entitled to assume it finds them as the BIOS left them.  Leaving the
 * map mask at the single plane the last blit used is what makes a real
 * driver's pointer come out in one colour bit of four - a pointer you can
 * see the screen through.  DOSBox never shows it: its INT 33h cursor is
 * painted into the emulated frame buffer directly and never reads a VGA
 * register at all, so this is a fault only real hardware can report.
 *
 * A plane's byte for one scanline of one cell is one of four things, and
 * which one depends only on the colour pair: where the foreground has this
 * plane's bit set and the background does not, it is the glyph; the other
 * way round, the glyph inverted; both, solid; neither, empty.  So a whole
 * run in one colour is four passes of straight byte stores.
 *===========================================================================*/

/* Put the graphics controller into the state the blitter below assumes.  All
 * five values are what the BIOS leaves behind after a mode set, so this is a
 * no-op on a card nothing else has touched - it is here for the card that
 * something else HAS touched, which on a machine with a mouse is every card. */
static void BlitStateEnter( void )
{
    GcWrite( 0, 0x00 );         /* set/reset value (unused with 1 = 0)        */
    GcWrite( 1, 0x00 );         /* enable set/reset: none - CPU data passes   */
    GcWrite( 3, 0x00 );         /* function select: replace, rotate 0         */
    GcWrite( 5, 0x00 );         /* write mode 0, read mode 0                  */
    GcWrite( 8, 0xFF );         /* bit mask: every bit                        */
}

/* Hand the card back.  The map mask is the ONE register the blitter leaves
 * somewhere the BIOS would not, so putting it back to all four planes is the
 * whole of the restore - and the reason the mouse pointer is solid. */
static void BlitStateLeave( void )
{
    SeqWrite( 2, 0x0F );
}

void DfVgaRenderer::BlitRun( int row, int col, int count, unsigned char attr,
                             const df_cell_t *src )
{
    int fg = attr & 0x0F, bg = ( attr >> 4 ) & 0x0F;
    df_vgaptr_t base = CellByte( row, col );
    int p, y, i;

    for ( p = 0; p < 4; ++p )
    {
        int fgbit = ( fg >> p ) & 1;
        int bgbit = ( bg >> p ) & 1;
        df_vgaptr_t line = base;

        SeqWrite( 2, 1 << p );

        if ( fgbit == bgbit )
        {
            unsigned char solid = (unsigned char)( fgbit ? 0xFF : 0x00 );
            for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
                for ( i = 0; i < count; ++i ) line[i] = solid;
        }
        else if ( fgbit )
        {
            for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
                for ( i = 0; i < count; ++i )
                    line[i] = s_font[ (unsigned)DF_CELL_CHAR( src[i] ) * CELL_H + y ];
        }
        else
        {
            for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
                for ( i = 0; i < count; ++i )
                    line[i] = (unsigned char)
                        ~s_font[ (unsigned)DF_CELL_CHAR( src[i] ) * CELL_H + y ];
        }
    }
}

void DfVgaRenderer::BlitIcon( int row, int col, int id, unsigned char attr )
{
    const IconBits *ic = IconFor( id );
    df_vgaptr_t base = CellByte( row, col );
    int bg = ( attr >> 4 ) & 0x0F;
    int p, y;

    if ( !ic )
    {
        /* Never registered, or the table was full: leave the cell to the
         * background rather than painting whatever happens to be in the
         * compile cache. */
        df_cell_t blank = DF_CELL_MAKE( ' ', attr );
        BlitRun( row, col, 1, attr, &blank );
        return;
    }

    for ( p = 0; p < 4; ++p )
    {
        int bgbit = ( bg >> p ) & 1;
        df_vgaptr_t line = base;
        SeqWrite( 2, 1 << p );
        for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
        {
            unsigned char v = ic->plane[p][y];
            if ( bgbit ) v = (unsigned char)( v | (unsigned char)~ic->mask[y] );
            line[0] = v;
        }
    }
}

/* One cell of a progress bar.  The frame is a two-pixel inset box in the
 * foreground colour with the fill inside it, which at eight pixels to a cell
 * gives a bar that moves eight times for every step the text renderer's
 * block characters manage. */
void DfVgaRenderer::BlitBar( int row, int col, unsigned char attr, unsigned char extra )
{
    df_vgaptr_t base = CellByte( row, col );
    int fg = attr & 0x0F, bg = ( attr >> 4 ) & 0x0F;
    int fill = DF_EXTRA_BAR_FILL( extra );
    int top = 3, bot = CELL_H - 4;          /* the bar's own top and bottom   */
    unsigned char rowmask[CELL_H];          /* 1 = this pixel is foreground   */
    int p, y, x;

    if ( fill > CELL_W ) fill = CELL_W;

    for ( y = 0; y < CELL_H; ++y )
    {
        unsigned char m = 0;
        if ( y == top || y == bot )
            m = 0xFF;                       /* the frame's top and bottom     */
        else if ( y > top && y < bot )
        {
            for ( x = 0; x < fill; ++x ) m = (unsigned char)( m | ( 0x80 >> x ) );
            if ( extra & DF_EXTRA_BAR_LEFT )  m = (unsigned char)( m | 0x80 );
            if ( extra & DF_EXTRA_BAR_RIGHT ) m = (unsigned char)( m | 0x01 );
        }
        rowmask[y] = m;
    }

    for ( p = 0; p < 4; ++p )
    {
        int fgbit = ( fg >> p ) & 1;
        int bgbit = ( bg >> p ) & 1;
        df_vgaptr_t line = base;
        SeqWrite( 2, 1 << p );
        for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
        {
            unsigned char v = 0;
            if ( fgbit ) v = (unsigned char)( v | rowmask[y] );
            if ( bgbit ) v = (unsigned char)( v | (unsigned char)~rowmask[y] );
            line[0] = v;
        }
    }
}

/* One cell of a picture, copied out of its planes.  Under a drop shadow the
 * same dimming a character cell gets: the picture's background goes black
 * and everything drawn on it goes dark grey, so a menu casts a shadow over a
 * chart rather than a hole in it. */
void DfVgaRenderer::BlitPicture( int row, int col, df_cell_t c )
{
    int                id  = DF_EXTRA_PICTURE_ID( DF_CELL_EXTRA( c ) );
    const PictureBits *pic = ( id < DF_PICTURE_MAX ) ? &s_pics[id] : 0;
    df_vgaptr_t        base = CellByte( row, col );
    int                pr, pc, p, y;
    unsigned           src;

    pr = pic ? row - ( pic->top - 1 ) : -1;
    pc = pic ? col - ( pic->left - 1 ) : -1;
    if ( !pic || !pic->plane[0] || pr < 0 || pr >= pic->rows ||
         pc < 0 || pc >= pic->cols )
    {
        /* Closed, or a cell the picture no longer covers: background. */
        df_cell_t blank = DF_CELL_MAKE( ' ', DF_CELL_ATTR( c ) );
        BlitRun( row, col, 1, DF_CELL_ATTR( c ), &blank );
        return;
    }

    src = (unsigned)pr * CELL_H * (unsigned)pic->cols + (unsigned)pc;

    if ( c & DF_CELL_SHADED )
    {
        unsigned char same[CELL_H];     /* 1 = the picture's background here */
        for ( y = 0; y < CELL_H; ++y )
        {
            unsigned char m = 0xFF;
            unsigned      at = src + (unsigned)y * (unsigned)pic->cols;
            for ( p = 0; p < 4; ++p )
            {
                unsigned char v = pic->plane[p][at];
                m = (unsigned char)( m & ( ( ( pic->bg >> p ) & 1 ) ? v : (unsigned char)~v ) );
            }
            same[y] = m;
        }
        for ( p = 0; p < 4; ++p )
        {
            df_vgaptr_t line = base;
            SeqWrite( 2, 1 << p );
            for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES )
                line[0] = (unsigned char)( ( p == 3 ) ? (unsigned char)~same[y] : 0 );
        }
        return;
    }

    for ( p = 0; p < 4; ++p )
    {
        const unsigned char *s = pic->plane[p] + src;
        df_vgaptr_t line = base;
        SeqWrite( 2, 1 << p );
        for ( y = 0; y < CELL_H; ++y, line += LINE_BYTES, s += pic->cols )
            line[0] = *s;
    }
}

/* Has picture 'extra' been drawn on since the last flush?  Its cells then
 * need painting even where the cell itself is unchanged - the cell says
 * WHICH picture, and it is the pixels behind it that moved. */
static boolean PictureDirty( unsigned char extra )
{
    int id = DF_EXTRA_PICTURE_ID( extra );
    return ( id < DF_PICTURE_MAX && s_pics[id].dirty ) ? TRUE : FALSE;
}

void DfVgaRenderer::Flush( void )
{
    int     r, c;
    boolean touched = FALSE;    /* have we taken the registers over yet?     */

    if ( !cells || !shown ) return;

    for ( r = 0; r < G_ROWS; ++r )
    {
        df_cell_t *want = cells + r * G_COLS;
        df_cell_t *have = shown + r * G_COLS;

        c = 0;
        while ( c < G_COLS )
        {
            int start, count;
            unsigned char attr, extra;

            if ( want[c] == have[c] )
            {
                unsigned char same = DF_CELL_EXTRA( want[c] );
                if ( !DF_EXTRA_IS_PICTURE( same ) || !PictureDirty( same ) )
                {
                    ++c;
                    continue;
                }
            }

            /* Taken over at the first cell that actually needs painting, so
             * a flush with nothing to do writes no ports at all - which
             * matters, because a flush follows every unbracketed draw. */
            if ( !touched ) { BlitStateEnter(); touched = TRUE; }

            extra = DF_CELL_EXTRA( want[c] );
            attr  = DF_CELL_ATTR( want[c] );

            if ( extra != DF_EXTRA_NONE )
            {
                /* Icons, bar slices and pictures are one cell at a time:
                 * each carries its own pixels, so there is no run to gather. */
                if ( DF_EXTRA_IS_PICTURE( extra ) )
                    BlitPicture( r, c, want[c] );
                else if ( DF_EXTRA_IS_BAR( extra ) )
                    BlitBar( r, c, attr, extra );
                else if ( DF_EXTRA_IS_ICON( extra ) )
                    BlitIcon( r, c, (int)extra - 1, attr );
                else
                {
                    /* An extra byte that is neither - nothing writes one
                     * today, and a cell whose meaning we do not know is
                     * painted as background rather than as whatever the
                     * icon cache happens to hold at that index. */
                    df_cell_t blank = DF_CELL_MAKE( ' ', attr );
                    BlitRun( r, c, 1, attr, &blank );
                }
                have[c] = want[c];
                ++c;
                continue;
            }

            /* A run of changed cells sharing one colour pair, which is what
             * a redrawn line of text is.  Gathering them is the difference
             * between four port writes for the line and four per character. */
            start = c;
            while ( c < G_COLS && want[c] != have[c] &&
                    DF_CELL_EXTRA( want[c] ) == DF_EXTRA_NONE &&
                    DF_CELL_ATTR( want[c] ) == attr )
                ++c;
            count = c - start;
            BlitRun( r, start, count, attr, want + start );
            memcpy( have + start, want + start, (size_t)count * sizeof( df_cell_t ) );
        }
    }

    if ( touched ) BlitStateLeave();

    /* Every VISIBLE cell of a changed picture has just been painted.  A cell
     * a dialog is covering was not, and needs no flag to be: when the dialog
     * goes, putting the cell back is itself the change that repaints it. */
    for ( r = 0; r < DF_PICTURE_MAX; ++r ) s_pics[r].dirty = FALSE;
}

/*===========================================================================*
 * Entry points
 *===========================================================================*/

static boolean     s_active = FALSE;
static short       s_saved_mode = 0;
static const char *s_error = "";

boolean DfGraphicsActive( void ) { return s_active; }
const char *DfGraphicsError( void ) { return s_error; }

boolean DfGraphicsStart( void )
{
    videoconfig_t cfg;

    if ( s_active ) return TRUE;
    s_error = "";

    _getvideoconfig( &cfg );
    s_saved_mode = (short)cfg.mode;

    /* EGA cannot do 640x480 and CGA cannot do sixteen colours at 640 wide;
     * asking _setvideomode would fail anyway, but failing here says why.
     * _SVGA is tested for rather than assumed: Watcom's graph.h has it and
     * not every Microsoft one does. */
    if ( cfg.adapter != _VGA && cfg.adapter != _MCGA
#ifdef _SVGA
         && cfg.adapter != _SVGA
#endif
       )
    {
        s_error = "Graphics mode needs a VGA adapter.";
        return FALSE;
    }

    if ( !GrabRomFont() )
    {
        s_error = "Could not read the adapter's character font.";
        return FALSE;
    }

    if ( !s_vga.Open() )
    {
        s_error = "Not enough memory for the graphics screen.";
        return FALSE;
    }

    if ( _setvideomode( _VRES16COLOR ) == 0 )
    {
        s_vga.Close();
        s_error = "The adapter refused 640x480 in 16 colours.";
        return FALSE;
    }

    /* No register setup here on purpose - BlitStateEnter does it at the top
     * of every flush.  See the blitter's header: the mouse driver paints
     * through these same registers from an interrupt, so a state we set once
     * and trust is a state that is wrong the first time the pointer moves. */

    s_active = TRUE;
    DfSetScreen( &s_vga );

    /* The mode switch cleared the card but not the shadow, which still
     * describes the text screen we came from.  Blank both so the first real
     * paint is the only thing anybody sees. */
    s_vga.Clear( FG_BLACK, BG_BLACK );
    s_vga.Flush();
    return TRUE;
}

void DfGraphicsStop( void )
{
    if ( !s_active ) return;
    s_active = FALSE;
    DfSetScreen( 0 );
    s_vga.Close();
    _setvideomode( s_saved_mode ? s_saved_mode : _TEXTC80 );
    /* The text renderer reads the grid out of DOSForms::video, and the mode
     * switch has just changed what that is. */
    {
        videoconfig_t cfg;
        _getvideoconfig( &cfg );
        DOSForms::video = cfg;
        DOSForms::SyncGrid( (short)cfg.numtextrows, (short)cfg.numtextcols );
    }
}
