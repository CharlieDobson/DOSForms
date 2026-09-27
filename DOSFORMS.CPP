/*===========================================================================
 * DOSFORMS.CPP  -  DOSForms base class, and the TEXT screen renderer
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * Two things live here, and the second is why the file grew.
 *
 * DOSForms itself is the base every widget derives from: the colour pair,
 * the mouse to hide while drawing, and the three shared primitives
 * (ClearScreen, FillBox, DrawBox) that the widgets build their pictures out
 * of.  Those primitives no longer call graph.h; they call the renderer.
 *
 * The TEXT RENDERER is the graph.h implementation of that interface, and it
 * is here rather than in a file of its own for a link-order reason worth
 * stating: every program that uses any part of this toolkit already compiles
 * DOSFORMS.CPP, and none of them list their sources the same way (four hand
 * written .CMD scripts and two generated MSVC makefiles).  Putting the
 * default renderer in a NEW module would have meant editing all six to add
 * it, including two that cannot be built or tested from here.  Putting it in
 * the one module they all already have means a program gets the renderer for
 * free and DISKSTAT, REVEAL and CDIR carry on producing exactly the screens
 * they produced before, byte for byte.  A graphics back end is then purely
 * additive - a second file that only the program wanting it links (see
 * VGAGFX.CPP) - and nothing else moves.
 *===========================================================================*/
// implementation for DOSForms base class

#include <stdlib.h>
#include <string.h>
#include <graph.h>
#include "dosforms.hpp"
#include "screen.hpp"
#include "moushide.hpp"

// shared video mode: one screen, one mode (POD, zero-initialized at startup)
videoconfig_t DOSForms::video;
boolean DOSForms::video_queried = FALSE;

/*===========================================================================*
 * The text renderer
 *===========================================================================*/

/* _outtext takes a plain char* on MSVC 1.52c (and a const one on Open
 * Watcom), so every string handed to it goes through this rather than a cast
 * repeated at nine call sites. */
static void OutText( const char *s )
{
    _outtext( (char *)s );
}

/* A pointer into the text buffer.  Far in the 16-bit models, a plain flat
 * one under a 32-bit extender, and the difference has to be in the TYPE or
 * the small models silently truncate it to the data segment. */
#ifdef __386__
typedef unsigned short *df_vidptr_t;
#else
typedef unsigned short far *df_vidptr_t;
#endif

class DfTextRenderer : public DfRenderer
{
public:
    virtual short Rows( void ) const;
    virtual short Cols( void ) const;
    virtual short CellW( void ) const { return 8; }
    virtual short CellH( void ) const { return 8; }
    virtual boolean IsGraphics( void ) const { return FALSE; }
    virtual boolean CanDraw( void ) const;

    virtual void TextOut( short row, short col, const char *s,
                          foreground_color_t fg, background_color_t bg );
    virtual void FillRect( short top, short left, short bottom, short right,
                           background_color_t bg );
    virtual void Clear( foreground_color_t fg, background_color_t bg );
    virtual void ClearRow( short row, foreground_color_t fg, background_color_t bg );
    virtual void Shade( short top, short left, short bottom, short right );
    virtual void ProgressBar( short row, short col, short cells,
                              unsigned long done, unsigned long total,
                              foreground_color_t fg, background_color_t bg );
    virtual void CellsRead( short top, short left, short bottom, short right,
                            df_cell_t *buf );
    virtual void CellsWrite( short top, short left, short bottom, short right,
                             const df_cell_t *buf );
    virtual df_cell_t CellAt( short row, short col ) const;

private:
    df_vidptr_t VideoBase( void ) const;
};

DfRenderer::~DfRenderer( void ) { }
boolean DfRenderer::HasIcons( void ) const { return FALSE; }
void DfRenderer::IconOut( short, short, int, background_color_t ) { }
boolean DfRenderer::HasPictures( void ) const { return FALSE; }
boolean DfRenderer::PictureOpen( int, short, short, short, short, int ) { return FALSE; }
void DfRenderer::PictureSpan( int, int, int, int, int ) { }
void DfRenderer::PictureClose( int ) { }
void DfRenderer::Flush( void ) { }

short DfTextRenderer::Rows( void ) const
{
    return DOSForms::video.numtextrows ? DOSForms::video.numtextrows : 25;
}

short DfTextRenderer::Cols( void ) const
{
    return DOSForms::video.numtextcols ? DOSForms::video.numtextcols : 80;
}

boolean DfTextRenderer::CanDraw( void ) const
{
    // text functions and attribute-cell drawing only behave in a text mode;
    // a graphics mode would mis-render (e.g. _setbkcolor changes meaning).
    switch ( DOSForms::video.mode ) {
        case _TEXTBW40:
        case _TEXTC40:
        case _TEXTBW80:
        case _TEXTC80:
        case _TEXTMONO:
            return TRUE;
        default:
            return FALSE;
    }
}

/* Where the character cells live.  Mono adapters put them 32K lower. */
df_vidptr_t DfTextRenderer::VideoBase( void ) const
{
    unsigned vseg = ( DOSForms::video.mode == _TEXTMONO ) ? 0xB000 : 0xB800;
#ifdef __386__
    // DOS/4GW flat model: the text buffer is directly at linear 0xB8000/0xB0000
    return (df_vidptr_t)( (unsigned long)vseg << 4 );
#else
    return (df_vidptr_t)( (unsigned long)vseg << 16 );
#endif
}

void DfTextRenderer::TextOut( short row, short col, const char *s,
                              foreground_color_t fg, background_color_t bg )
{
    if ( !s || !*s ) return;
    _settextcolor( fg );
    _setbkcolor( bg );
    _settextposition( row, col );
    OutText( s );
}

void DfTextRenderer::FillRect( short top, short left, short bottom, short right,
                               background_color_t bg )
{
    if ( bottom < top || right < left ) return;

    // a colored space-fill of a rectangle == clear a text-window region
    _setbkcolor( bg );
    _settextwindow( top, left, bottom, right );
    _clearscreen( _GWINDOW );
    _settextwindow( 1, 1, Rows(), Cols() );
}

void DfTextRenderer::Clear( foreground_color_t fg, background_color_t bg )
{
    _settextcolor( fg );
    _setbkcolor( bg );
    _clearscreen( _GCLEARSCREEN );
}

void DfTextRenderer::ClearRow( short row, foreground_color_t fg, background_color_t bg )
{
    _settextcolor( fg );
    _setbkcolor( bg );
    // 1-row text window + clear: paints the whole row (corner included)
    // with no bottom-right scroll; full-screen window restored after
    _settextwindow( row, 1, row, Cols() );
    _clearscreen( _GWINDOW );
    _settextwindow( 1, 1, Rows(), Cols() );
}

void DfTextRenderer::Shade( short top, short left, short bottom, short right )
{
    // Nothing in a text cell can be made darker, so the toolkit's drop shadow
    // has always been a black fill.  Kept exactly that, on purpose: this is
    // the renderer whose output must not change.
    FillRect( top, left, bottom, right, BG_BLACK );
}

void DfTextRenderer::ProgressBar( short row, short col, short cells,
                                  unsigned long done, unsigned long total,
                                  foreground_color_t fg, background_color_t bg )
{
    char bar[132];
    short filled, i;

    if ( cells < 1 ) return;
    if ( cells > (short)sizeof( bar ) - 1 ) cells = (short)sizeof( bar ) - 1;

    if ( total == 0 )         filled = 0;
    else if ( done >= total ) filled = cells;
    else filled = (short)( ( (unsigned long)cells * done ) / total );

    for ( i = 0; i < cells; ++i )
        bar[i] = (char)( ( i < filled ) ? 0xDB : 0xB0 );   /* full / light block */
    bar[cells] = '\0';

    TextOut( row, col, bar, fg, bg );
}

void DfTextRenderer::CellsRead( short top, short left, short bottom, short right,
                                df_cell_t *buf )
{
    df_vidptr_t vid = VideoBase();
    short cols = Cols();
    short r, c;

    if ( !buf || bottom < top || right < left ) return;
    for ( r = top; r <= bottom; ++r ) {
        unsigned off = (unsigned)( r - 1 ) * cols + ( left - 1 );
        for ( c = left; c <= right; ++c )
            *buf++ = (df_cell_t)vid[ off++ ];
    }
}

void DfTextRenderer::CellsWrite( short top, short left, short bottom, short right,
                                 const df_cell_t *buf )
{
    df_vidptr_t vid = VideoBase();
    short cols = Cols();
    short r, c;

    if ( !buf || bottom < top || right < left ) return;
    for ( r = top; r <= bottom; ++r ) {
        unsigned off = (unsigned)( r - 1 ) * cols + ( left - 1 );
        for ( c = left; c <= right; ++c )
            vid[ off++ ] = (unsigned short)( *buf++ & 0xFFFFUL );
    }
}

df_cell_t DfTextRenderer::CellAt( short row, short col ) const
{
    df_vidptr_t vid = VideoBase();
    return (df_cell_t)vid[ (unsigned)( row - 1 ) * Cols() + ( col - 1 ) ];
}

/*===========================================================================*
 * Dispatch, the icon registry, and the draw-depth flush
 *===========================================================================*/

static DfTextRenderer  s_text;
static DfRenderer     *s_active = &s_text;
static int             s_draw_depth = 0;

DfRenderer *DfScreen( void ) { return s_active; }

void DfSetScreen( DfRenderer *r )
{
    s_active = r ? r : (DfRenderer *)&s_text;

    /* Every popup in the toolkit centres itself with these two, so they have
     * to follow the renderer and not the card: a graphics renderer may hand
     * out a grid the BIOS never heard of (80x30, where text mode is 80x25),
     * and a dialog placed for 25 rows on a 30-row screen sits visibly high. */
    DOSForms::SyncGrid( s_active->Rows(), s_active->Cols() );
}

void DfTextOut( short row, short col, const char *s,
                foreground_color_t fg, background_color_t bg )
{
    s_active->TextOut( row, col, s, fg, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfFillRect( short top, short left, short bottom, short right,
                 background_color_t bg )
{
    s_active->FillRect( top, left, bottom, right, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfClear( foreground_color_t fg, background_color_t bg )
{
    s_active->Clear( fg, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfClearRow( short row, foreground_color_t fg, background_color_t bg )
{
    s_active->ClearRow( row, fg, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfShade( short top, short left, short bottom, short right )
{
    s_active->Shade( top, left, bottom, right );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfProgressBar( short row, short col, short cells,
                    unsigned long done, unsigned long total,
                    foreground_color_t fg, background_color_t bg )
{
    s_active->ProgressBar( row, col, cells, done, total, fg, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

void DfIconOut( short row, short col, int icon, background_color_t bg )
{
    s_active->IconOut( row, col, icon, bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
}

boolean DfPictureOpen( int id, short top, short left, short bottom, short right,
                       background_color_t bg )
{
    boolean ok = s_active->PictureOpen( id, top, left, bottom, right, (int)bg );
    if ( s_draw_depth == 0 ) s_active->Flush();
    return ok;
}

/* The one forwarder that never flushes - see Pictures in SCREEN.HPP. */
void DfPictureSpan( int id, int y, int x0, int x1, int color )
{
    s_active->PictureSpan( id, y, x0, x1, color );
}

void DfPictureClose( int id ) { s_active->PictureClose( id ); }

boolean   DfHasPictures( void ) { return s_active->HasPictures(); }
boolean   DfHasIcons( void )   { return s_active->HasIcons(); }
short     DfRows( void )       { return s_active->Rows(); }
short     DfCols( void )       { return s_active->Cols(); }
boolean   DfIsGraphics( void ) { return s_active->IsGraphics(); }
boolean   DfCanDraw( void )    { return s_active->CanDraw(); }
df_cell_t DfCellAt( short row, short col ) { return s_active->CellAt( row, col ); }

short DfMouseCol( int pixel_x )
{
    short w = s_active->CellW();
    return (short)( ( w > 0 ) ? pixel_x / w : pixel_x / 8 );
}

short DfMouseRow( int pixel_y )
{
    short h = s_active->CellH();
    return (short)( ( h > 0 ) ? pixel_y / h : pixel_y / 8 );
}

void DfDrawEnter( void ) { ++s_draw_depth; }

void DfDrawLeave( void )
{
    if ( s_draw_depth > 0 && --s_draw_depth == 0 ) s_active->Flush();
}

int  DfDrawDepth( void ) { return s_draw_depth; }
void DfFlush( void )     { s_active->Flush(); }

/*---- Icon registry -------------------------------------------------------- *
 * Kept here, next to the dispatch, so that registering works with or without
 * a graphics renderer installed and across a switch between them: the art is
 * the application's static data and all this holds is a pointer to it.  A
 * renderer that can draw icons compiles what it needs the first time it is
 * asked for one.
 *---------------------------------------------------------------------------*/
static const char * const *s_icon_art[DF_ICON_MAX];
static int                 s_icon_rows[DF_ICON_MAX];
static int                 s_icon_count = 0;

int DfIconRegister( const char * const *art, int rows )
{
    if ( !art || rows <= 0 ) return DF_ICON_NONE;
    if ( s_icon_count >= DF_ICON_MAX ) return DF_ICON_NONE;
    s_icon_art[s_icon_count]  = art;
    s_icon_rows[s_icon_count] = rows;
    return s_icon_count++;
}

const char * const *DfIconArt( int icon, int *rows )
{
    if ( icon < 0 || icon >= s_icon_count ) return 0;
    if ( rows ) *rows = s_icon_rows[icon];
    return s_icon_art[icon];
}

/*===========================================================================*
 * DOSForms
 *===========================================================================*/

void DOSForms::SyncGrid( short rows, short cols )
{
    // Before the first widget exists there is nothing to sync TO: the query
    // has not run, and EnsureVideoConfig takes the renderer's grid the moment
    // it does.  Syncing here anyway would be overwritten a moment later.
    if ( !video_queried ) return;
    if ( rows > 0 ) video.numtextrows = rows;
    if ( cols > 0 ) video.numtextcols = cols;
}

void DOSForms::EnsureVideoConfig( void )
{
    // one-time display setup; the app must have set the mode before the first form
    if ( !video_queried ) {
        _getvideoconfig( &video );
        _wrapon( _GWRAPOFF );   // no line-wrap -> no bottom-right scroll on _outtext
        video_queried = TRUE;
        // A graphics renderer installed before the first widget was built has
        // already decided the grid; _getvideoconfig has just overwritten it
        // with the card's own idea, so put the renderer's back.
        if ( DfIsGraphics() ) {
            video.numtextrows = DfRows();
            video.numtextcols = DfCols();
        }
    }
}

DOSForms::DOSForms( void )
{
    fg_color = FG_BLUE;
    bg_color = BG_WHITE;
    mouse = 0;
    EnsureVideoConfig();
}

DOSForms::DOSForms( const foreground_color_t foreground, const background_color_t background )
{
    fg_color = foreground;
    bg_color = background;
    mouse = 0;
    EnsureVideoConfig();
}

DOSForms::~DOSForms( void )
{
    // destructor code here
}

void DOSForms::SetMouse( MSMouse *m )
{
    mouse = m;
}

boolean DOSForms::IsTextMode( void ) const
{
    // Historically "is the card in a text mode", and every widget's Draw
    // opens with it.  It now asks the renderer whether it can paint at all,
    // which is the question those Draws were really asking: a graphics
    // renderer answers yes, and the text one still answers no when the
    // application has left the card somewhere it cannot write.
    return DfCanDraw();
}

void DOSForms::ClearScreen( void )
{
    if ( !IsTextMode() ) return;   // app left us in a mode we cannot paint

    MouseHide guard( mouse );   // cursor hidden until this scope exits

    DfClear( fg_color, bg_color );
}

void DOSForms::ClearRow( short row )
{
    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    DfClearRow( row, fg_color, bg_color );
}

void DOSForms::FillBox( short top, short left, short bottom, short right, background_color_t bg )
{
    if ( !IsTextMode() ) return;
    if ( bottom < top || right < left ) return;

    MouseHide guard( mouse );

    DfFillRect( top, left, bottom, right, bg );
}

void DOSForms::DrawBox( short top, short left, short bottom, short right, border_style_t style )
{
    if ( !IsTextMode() ) return;
    if ( style == BORDER_NONE ) return;
    if ( bottom < top || right < left ) return;

    MouseHide guard( mouse );

    // CP437 line-drawing chars: top-left, top-right, bottom-left,
    // bottom-right, horizontal, vertical
    unsigned char tl, tr, bl, br, hz, vt;
    switch ( style ) {
        case BORDER_DOUBLE_LINE:
            tl = 201; tr = 187; bl = 200; br = 188; hz = 205; vt = 186;
            break;
        case BORDER_SOLID:
            tl = tr = bl = br = hz = vt = 219;   // full block
            break;
        case BORDER_DOUBLE_HORIZONTAL_SINGLE_VERTICAL:
            tl = 213; tr = 184; bl = 212; br = 190; hz = 205; vt = 179;
            break;
        case BORDER_SINGLE_HORIZONTAL_DOUBLE_VERTICAL:
            tl = 214; tr = 183; bl = 211; br = 189; hz = 196; vt = 186;
            break;
        case BORDER_SINGLE_LINE:
        default:
            tl = 218; tr = 191; bl = 192; br = 217; hz = 196; vt = 179;
            break;
    }

    short width = right - left + 1;
    char edge[82];
    short i;

    if ( width > (short)sizeof( edge ) - 1 ) width = sizeof( edge ) - 1;

    // top edge
    edge[0] = (char)tl;
    for ( i = 1; i < width - 1; ++i ) edge[i] = (char)hz;
    edge[width - 1] = (char)tr;
    edge[width] = '\0';
    DfTextOut( top, left, edge, fg_color, bg_color );

    // vertical sides
    {
        char v[2];
        v[0] = (char)vt;
        v[1] = '\0';
        for ( short r = top + 1; r < bottom; ++r ) {
            DfTextOut( r, left, v, fg_color, bg_color );
            DfTextOut( r, right, v, fg_color, bg_color );
        }
    }

    // bottom edge
    edge[0] = (char)bl;
    for ( i = 1; i < width - 1; ++i ) edge[i] = (char)hz;
    edge[width - 1] = (char)br;
    edge[width] = '\0';
    DfTextOut( bottom, left, edge, fg_color, bg_color );
}

boolean DOSForms::HandleEvent( const Event & )
{
    return FALSE;   // base forms ignore input; widgets override to respond
}

/*===========================================================================*
 * Modal plumbing.  See DOSFORMS.HPP for what each of these is for.
 *===========================================================================*/

static DfPeekFn df_modal_peek = 0;
static DfPeekFn df_modal_sync = 0;

void DfSetModalPeek( DfPeekFn fn ) { df_modal_peek = fn; }
DfPeekFn DfModalPeek( void )       { return df_modal_peek; }
void DfSetModalSync( DfPeekFn fn ) { df_modal_sync = fn; }

void DfModalFinished( void )
{
    if ( df_modal_sync ) df_modal_sync();
}

/*===========================================================================*
 * DfVectorGuard - the real-mode interrupt vectors.  See DOSFORMS.HPP for
 * which ones move, why, and why this has to be declared before the mouse.
 *
 * 32-bit only, and DPMI only.  INT 21h AH=35h/25h are no use here: under an
 * extender they read and write the PROTECTED mode vector, and the table the
 * extender is complaining about is the REAL mode one.  DPMI 0200h/0201h are
 * the calls that address it - BL is the vector, CX:DX the far address.  A
 * host that refuses either (carry set) just means there is nothing to put
 * back, so a failed save disarms the restore rather than guessing.
 *===========================================================================*/

#ifdef __386__

#include <dos.h>                // union REGS
#include <i86.h>                // int386

/* INT 33h first so the numbers read in the order the extender reports them. */
static const unsigned char df_guard_vec[] = { 0x33, 0x43 };
#define DF_GUARD_N ( (int)( sizeof( df_guard_vec ) / sizeof( df_guard_vec[0] ) ) )

static unsigned short df_guard_seg[DF_GUARD_N];
static unsigned short df_guard_off[DF_GUARD_N];
static boolean        df_guard_held[DF_GUARD_N];

DfVectorGuard::DfVectorGuard( void )
{
    union REGS r;
    int        i;

    for ( i = 0; i < DF_GUARD_N; ++i ) {
        df_guard_held[i] = FALSE;
        memset( &r, 0, sizeof( r ) );
        r.x.eax = 0x0200;                       // get real mode vector
        r.x.ebx = df_guard_vec[i];
        int386( 0x31, &r, &r );
        if ( r.x.cflag ) continue;
        df_guard_seg[i] = r.w.cx;
        df_guard_off[i] = r.w.dx;
        df_guard_held[i] = TRUE;
    }
}

DfVectorGuard::~DfVectorGuard( void )
{
    union REGS r;
    int        i;

    for ( i = 0; i < DF_GUARD_N; ++i ) {
        if ( !df_guard_held[i] ) continue;
        memset( &r, 0, sizeof( r ) );
        r.x.eax = 0x0201;                       // set real mode vector
        r.x.ebx = df_guard_vec[i];
        r.x.ecx = df_guard_seg[i];
        r.x.edx = df_guard_off[i];
        int386( 0x31, &r, &r );
    }
}

#else   /* 16-bit real mode: no extender, no second table, nothing to do. */

DfVectorGuard::DfVectorGuard( void ) {}
DfVectorGuard::~DfVectorGuard( void ) {}

#endif
