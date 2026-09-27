/*===========================================================================
 * SCREEN.HPP  -  The renderer behind every DOSForms widget
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * WHY THIS EXISTS.  Every widget in the toolkit paints with the same four
 * calls - set the colours, position the cursor, push a string of CP437 - and
 * every one of them assumed those calls were graph.h's, which means they
 * assumed a TEXT mode.  This header is the one seam between "what a widget
 * wants on the screen" and "how the screen is made to show it", so a second
 * back end can put the same character grid up in a GRAPHICS mode without a
 * single widget knowing.  See VGAGFX.HPP for that back end.
 *
 * THE GRID IS THE CONTRACT.  A renderer is a rectangle of character cells,
 * each with a foreground and a background colour, and nothing here is
 * expressed in pixels.  A graphics renderer draws the same grid with a pixel
 * font; it may be a different SIZE (the VGA one is 80x30 against text mode's
 * 80x25), which is why nothing may assume 25 rows - ask Rows()/Cols().
 *
 * COORDINATES ARE 1-BASED, like _settextposition, because that is what every
 * call site in the toolkit was already passing.  One rule, no exceptions:
 * the cell save/restore calls take 1-based corners too, even though the
 * buffer they fill is laid out like video memory.
 *
 * FLUSHING.  A text renderer writes straight to the screen; a graphics one
 * paints into a shadow grid and blits the cells that CHANGED, because a
 * planar VGA cell costs sixty-four byte writes where a text cell costs one,
 * and the widgets here redraw whole rows for a one-row change.  Nothing has
 * to call DfFlush by hand: MouseHide's destructor flushes when the outermost
 * draw guard goes out of scope, and any Df* call made outside a guard
 * flushes itself.  Do not "optimise" either of those away - a missed flush
 * is a screen that stops updating, which looks like a hang.
 *===========================================================================*/

#ifndef SCREEN_HPP
#define SCREEN_HPP

#include "structs.hpp"

/*---- A screen cell -------------------------------------------------------- *
 * Low 16 bits are the video-memory pair (character in 0..7, attribute in
 * 8..15) so a text renderer can hand one straight to and from 0xB8000.  Bits
 * 16..23 are an ICON id plus one, zero for an ordinary character cell: an
 * icon has to survive a Popup saving the screen and putting it back, and it
 * cannot do that if the only thing saved is a character and a colour.  Bits
 * 24..31 are flags; the one in use is DF_CELL_SHADED, below.
 *---------------------------------------------------------------------------*/
typedef unsigned long df_cell_t;

#define DF_CELL_CHAR( c )   ( (unsigned char)( (c) & 0xFF ) )
#define DF_CELL_ATTR( c )   ( (unsigned char)( ( (c) >> 8 ) & 0xFF ) )
#define DF_CELL_EXTRA( c )  ( (unsigned char)( ( (c) >> 16 ) & 0xFF ) )
#define DF_CELL_MAKE( ch, at ) \
    ( (df_cell_t)( (unsigned char)(ch) ) | ( (df_cell_t)( (unsigned char)(at) ) << 8 ) )
#define DF_CELL_WITH( c, ex ) \
    ( ( (c) & 0x0000FFFFUL ) | ( (df_cell_t)( (unsigned char)(ex) ) << 16 ) )

/*---- What the extra byte says --------------------------------------------- *
 * 0                  an ordinary character cell.
 * 1 .. DF_ICON_MAX   an icon, id = value - 1.
 * 0x40 | ...         a slice of a progress bar: the low nibble is how many of
 *                    the cell's eight pixel columns are filled, and the two
 *                    bits above it mark the bar's left and right ENDS (which
 *                    are the only cells that get an upright of the frame).
 * 0x80 | id          one cell of PICTURE 'id' (see Pictures, below).  Which
 *                    cell of the picture it is follows from where it sits:
 *                    a Popup puts cells back where it found them.
 *
 * A progress bar is encoded INTO the grid rather than painted straight at
 * the screen, and that is the whole reason it survives: a dialog can open on
 * top of one mid-extraction (the overwrite prompt does exactly that), and
 * what a Popup saves and puts back is cells.  Pixels drawn behind the grid's
 * back would be saved as blanks and restored as holes.
 *---------------------------------------------------------------------------*/
#define DF_EXTRA_NONE        0
#define DF_EXTRA_ICON( id )  ( (unsigned char)( (id) + 1 ) )
#define DF_EXTRA_IS_ICON( e )( (e) >= 1 && (e) <= DF_ICON_MAX )
#define DF_EXTRA_BAR         0x40       /* | fill 0..8 | ends */
#define DF_EXTRA_BAR_LEFT    0x10
#define DF_EXTRA_BAR_RIGHT   0x20
#define DF_EXTRA_IS_BAR( e ) ( ( (e) & 0xC0 ) == DF_EXTRA_BAR )
#define DF_EXTRA_BAR_FILL( e ) ( (e) & 0x0F )
#define DF_EXTRA_PICTURE( id )   ( (unsigned char)( 0x80 | (id) ) )
#define DF_EXTRA_IS_PICTURE( e ) ( ( (e) & 0x80 ) != 0 )
#define DF_EXTRA_PICTURE_ID( e ) ( (e) & 0x7F )

/* A drop shadow has fallen on this cell.  Only a picture cell carries it: a
 * character cell under a shadow is simply recoloured, but a picture has no
 * colour pair to change, so the renderer dims its pixels when it paints it. */
#define DF_CELL_SHADED       0x01000000UL

/*---- Icons ---------------------------------------------------------------- *
 * An icon is 8 pixels wide and as tall as one cell, given as CellH() strings
 * of 8 characters: '.' is transparent (the cell's background shows through)
 * and '0'..'9' / 'A'..'F' are the sixteen EGA colours.  Art that is shorter
 * than the cell is padded with transparent rows, so one 16-row drawing works
 * on any cell height.
 *
 * Registering is always allowed - in text mode it simply records the art and
 * answers an id that nothing will ever draw - so an application may register
 * unconditionally at startup and let HasIcons() decide whether to ask for
 * them.  A renderer compiles the art to planes the first time it is asked to
 * draw one, which is what lets icons be registered before, after, or across
 * a switch between renderers.
 *---------------------------------------------------------------------------*/
#define DF_ICON_MAX     24          /* ids 0..23 */
#define DF_ICON_NONE    (-1)

/* Register 'art' (an array of 'rows' strings, each 8 characters).  The array
 * must live for as long as the program does - static data, not a local.
 * Returns the icon id, or DF_ICON_NONE if the table is full. */
int DfIconRegister( const char * const *art, int rows );

/* The art behind an id, for a renderer compiling it.  Returns 0 for an id
 * that was never registered; 'rows' receives the number of strings. */
const char * const *DfIconArt( int icon, int *rows );

/*---- Pictures ------------------------------------------------------------- *
 * A picture is a rectangle of cells whose PIXELS the application draws - a
 * chart, which no arrangement of characters can say.  It is the icon idea
 * made bigger, and for the same reason it lives IN THE GRID: every cell of
 * it carries DF_EXTRA_PICTURE in the extra byte, so a menu or a dialog that
 * opens over it saves those cells, and putting them back puts the picture
 * back.  Pixels painted straight at the card, behind the grid, would be
 * saved as whatever the grid thought was there and restored as a hole.
 *
 * Open it over a rectangle of cells (which fills it with a background colour
 * and puts its cells on the screen), then draw it with horizontal runs of
 * pixels.  Pixel coordinates are the picture's own: (0,0) is the top-left of
 * its first cell, and it is CellW() x CellH() pixels to a cell.  Colours are
 * the sixteen EGA ones, as in icon art.
 *
 * DRAW ONE INSIDE A DRAW GUARD (a MouseHide).  A span changes pixels and
 * never flushes on its own - a chart is hundreds of spans, and flushing
 * after each would repaint the picture hundreds of times.  The guard's end
 * flushes once, and the flush repaints every cell of a picture whose pixels
 * changed.
 *
 * Only a graphics renderer has pictures; the text one answers FALSE to
 * DfHasPictures and ignores the rest, so ask before choosing what to draw.
 * The ids are the application's to assign, 0 .. DF_PICTURE_MAX-1; opening an
 * id again reuses its pixels when the size is unchanged and replaces them
 * when it is not.
 *---------------------------------------------------------------------------*/
#define DF_PICTURE_MAX  4

/*---- The renderer --------------------------------------------------------- */
class DfRenderer
{
public:
    virtual ~DfRenderer( void );

    /* geometry */
    virtual short Rows( void ) const = 0;
    virtual short Cols( void ) const = 0;
    /* Pixels per cell as the INT 33h mouse driver counts them, which is what
     * turns a driver coordinate into a cell.  Text mode reports 8x8 virtual
     * pixels per cell whatever the font really is; VGA mode 12h reports real
     * ones, so the row divisor is 16 there and the column divisor is 8 in
     * both.  Anything dividing a mouse coordinate by a literal 8 is a bug. */
    virtual short CellW( void ) const = 0;
    virtual short CellH( void ) const = 0;
    virtual boolean IsGraphics( void ) const = 0;
    /* Is the adapter in a mode this renderer can actually paint?  The text
     * renderer answers FALSE if the application left the card in a graphics
     * mode behind its back, which is the check every widget's Draw opens
     * with (DOSForms::IsTextMode). */
    virtual boolean CanDraw( void ) const = 0;

    /* painting (1-based) */
    virtual void TextOut( short row, short col, const char *s,
                          foreground_color_t fg, background_color_t bg ) = 0;
    /* Background only, like the _settextwindow + _clearscreen pair it
     * replaces: the foreground of the filled cells is left as it was. */
    virtual void FillRect( short top, short left, short bottom, short right,
                           background_color_t bg ) = 0;
    virtual void Clear( foreground_color_t fg, background_color_t bg ) = 0;
    virtual void ClearRow( short row, foreground_color_t fg,
                           background_color_t bg ) = 0;

    /* Darken a rectangle without erasing what is under it - a drop shadow.
     * Text mode has no way to dim a cell, so it does what the toolkit has
     * always done and fills the rectangle black; a graphics renderer dims
     * the pixels instead, which is the look this was always imitating. */
    virtual void Shade( short top, short left, short bottom, short right ) = 0;

    /* A horizontal progress bar filling 'cells' cells from (row, col).
     * Text mode draws it in CP437 blocks; a graphics renderer draws it at
     * pixel resolution, so the same call is eight times smoother without the
     * caller knowing which it got. */
    virtual void ProgressBar( short row, short col, short cells,
                              unsigned long done, unsigned long total,
                              foreground_color_t fg, background_color_t bg ) = 0;

    /* save / restore (1-based corners; 'buf' holds (bottom-top+1)*(right-left+1)
     * cells, row-major) */
    virtual void CellsRead( short top, short left, short bottom, short right,
                            df_cell_t *buf ) = 0;
    virtual void CellsWrite( short top, short left, short bottom, short right,
                             const df_cell_t *buf ) = 0;
    virtual df_cell_t CellAt( short row, short col ) const = 0;

    /* icons */
    virtual boolean HasIcons( void ) const;
    virtual void IconOut( short row, short col, int icon, background_color_t bg );

    /* pictures (1-based cell corners; picture-relative pixels) */
    virtual boolean HasPictures( void ) const;
    virtual boolean PictureOpen( int id, short top, short left,
                                 short bottom, short right, int bg );
    virtual void PictureSpan( int id, int y, int x0, int x1, int color );
    virtual void PictureClose( int id );

    virtual void Flush( void );
};

/*---- The active renderer -------------------------------------------------- */

/* Never null: a text renderer over graph.h is installed before the first
 * widget is constructed and is what DfSetScreen(0) goes back to. */
DfRenderer *DfScreen( void );
void        DfSetScreen( DfRenderer *r );

/* Convenience forwarders.  Widgets call these rather than DfScreen()->X() so
 * that the auto-flush below lives in ONE place. */
void DfTextOut( short row, short col, const char *s,
                foreground_color_t fg, background_color_t bg );
void DfFillRect( short top, short left, short bottom, short right,
                 background_color_t bg );
void DfClear( foreground_color_t fg, background_color_t bg );
void DfClearRow( short row, foreground_color_t fg, background_color_t bg );
void DfShade( short top, short left, short bottom, short right );
void DfProgressBar( short row, short col, short cells,
                    unsigned long done, unsigned long total,
                    foreground_color_t fg, background_color_t bg );
void DfIconOut( short row, short col, int icon, background_color_t bg );
boolean DfHasIcons( void );
boolean DfHasPictures( void );
boolean DfPictureOpen( int id, short top, short left, short bottom, short right,
                       background_color_t bg );
void DfPictureSpan( int id, int y, int x0, int x1, int color );  /* no flush */
void DfPictureClose( int id );
short DfRows( void );
short DfCols( void );
boolean DfIsGraphics( void );
boolean DfCanDraw( void );
df_cell_t DfCellAt( short row, short col );

/* INT 33h pixel coordinate -> cell.  Replaces every "/ 8". */
short DfMouseCol( int pixel_x );
short DfMouseRow( int pixel_y );

/*---- Draw nesting and flushing -------------------------------------------- *
 * MouseHide drives these; nothing else should need to.  The count is kept
 * here rather than in MouseHide because a guard with no mouse still marks a
 * draw in progress, while only a guard WITH one may touch the driver.
 *---------------------------------------------------------------------------*/
void DfDrawEnter( void );
void DfDrawLeave( void );       /* flushes when the count reaches zero */
int  DfDrawDepth( void );
void DfFlush( void );

#endif /* SCREEN_HPP */
