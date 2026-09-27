/*===========================================================================
 * PROGBOX.HPP - Progress box and "please wait" box.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * TWO BOXES FOR TWO KINDS OF WAITING, and the difference between them is
 * whether the work can say how far along it is.
 *
 * ProgressBox is for work that CAN: a title, a line naming what is being
 * done, a bar, and a way out.  It is not modal in the usual sense - there is
 * no input loop, because the work is the loop.  The caller opens it, calls
 * Update from wherever it already reports progress, and closes it; Update
 * returns FALSE when the user has asked to stop, which is the whole of the
 * cancel protocol.
 *
 *     ProgressBox bar( FG_BLACK, BG_WHITE, "Extracting" );
 *     bar.Open();
 *     for ( i = 0; i < n; ++i ) {
 *         if ( !bar.Update( name[i], i + 1, n ) ) break;   // cancelled
 *         ...
 *     }
 *     bar.Close();
 *
 * WaitBox is for work that CANNOT - a directory scan, a directory listing, a
 * solid archive being wound forward - where all there is to say is that
 * something is happening.  One line, no bar, no way out.
 *
 * THE BAR IS DRAWN THROUGH THE RENDERER, not as characters, so a graphics
 * screen gets one at pixel resolution (eight steps per cell) without this
 * code knowing which screen it is on.
 *===========================================================================*/
#ifndef PROGBOX_HPP
#define PROGBOX_HPP

#include "popup.hpp"
#include "button.hpp"
#include "structs.hpp"

#define PB_WIDTH   62           /* box width in cells, borders included     */
#define PB_ROOM    ( PB_WIDTH - 6 )   /* text starts 3 in, right border 1   */

class ProgressBox : public Popup
{
    unsigned long shown_done;   // what the bar is currently drawn at
    unsigned long shown_total;
    int           hasCancel;    // draw a Cancel button as well as the hint
    int           cancelled;    // the user has asked to stop
    int           opened;
    int           lastB;        // mouse edge detector for the Cancel button
    Button        btnCancel;

    ProgressBox( const ProgressBox & );
    ProgressBox &operator=( const ProgressBox & );

    void DrawFrame( void );
    int  PollCancel( void );    // non-blocking: 1 = the user wants out

public:
    ProgressBox( const foreground_color_t foreground,
                 const background_color_t background, const char *titleText );
    ~ProgressBox( void );

    /* A clickable [ Cancel ] as well as the "Press Esc" hint.  Off by
     * default: Esc alone is enough for work that finishes in a second, and a
     * button that is only up for a moment is a button nobody aims at. */
    void SetCancelButton( void );

    /* Put the box on the screen with an empty bar.  Safe to call twice. */
    void Open( void );

    /* Repaint the name line and the bar, then look (without blocking) for a
     * cancel.  Returns FALSE once the user has asked to stop - and keeps
     * returning FALSE, so a caller that checks only every tenth item cannot
     * lose the answer.  'text' may be null to leave the line alone.
     *
     * 'done' counts things FINISHED when this returns: pass i + 1 inside a
     * loop over i, so the bar and any counter in 'text' agree. */
    boolean Update( const char *text, unsigned long done, unsigned long total );

    boolean Cancelled( void ) const { return (boolean)( cancelled ? TRUE : FALSE ); }

    /* Take the box down and put back what was under it. */
    void Close( void );
};

/*---- WaitBox -------------------------------------------------------------- */
class WaitBox : public Popup
{
    int opened;

    WaitBox( const WaitBox & );
    WaitBox &operator=( const WaitBox & );

public:
    WaitBox( const foreground_color_t foreground,
             const background_color_t background, const char *titleText );
    ~WaitBox( void );

    void Open( const char *message );   // size to the message, draw, return
    void Close( void );
};

#endif
