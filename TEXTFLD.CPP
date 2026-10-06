/*===========================================================================
 * TEXTFLD.CPP - One line of editable text.  See TEXTFLD.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "textfld.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON
#include "moushide.hpp"         // hide the cursor while painting
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>

TextField::TextField( const foreground_color_t foreground,
                      const background_color_t background )
    : DOSForms( foreground, background )
{
    row = col = 0;
    width = 0;
    buffer[0] = '\0';
    len = 0;
    cur = 0;
    scroll = 0;
    maxlen = (int)sizeof( buffer ) - 1;
    focus = 0;
    masked = 0;
}

void TextField::SetPosition( short fieldRow, short fieldCol, short fieldWidth )
{
    row   = fieldRow;
    col   = fieldCol;
    width = fieldWidth;
    if ( width > (short)( sizeof( buffer ) - 1 ) ) width = (short)( sizeof( buffer ) - 1 );
    scroll = 0;                 /* worked out afresh for the new width */
    ClampCursor();
}

void TextField::SetText( const char *initial )
{
    buffer[0] = '\0';
    if ( initial )
    {
        strncpy( buffer, initial, (size_t)maxlen );
        buffer[maxlen] = '\0';
    }
    len = (int)strlen( buffer );
    cur = len;
    scroll = 0;
    ClampCursor();
}

void TextField::SetMaxLength( int n )
{
    if ( n < 1 ) n = 1;
    if ( n > (int)sizeof( buffer ) - 1 ) n = (int)sizeof( buffer ) - 1;
    maxlen = n;
    if ( len > maxlen ) { len = maxlen; buffer[len] = '\0'; }
    ClampCursor();
}

void TextField::SetMasked( boolean on ) { masked = on ? 1 : 0; }
void TextField::SetFocus( boolean on )  { focus  = on ? 1 : 0; }

boolean TextField::HitTest( short x, short y ) const
{
    return (boolean)( ( y == row && x >= col && x < col + width ) ? TRUE : FALSE );
}

/* Keep the cursor inside the text and the text scrolled so the cursor is on
 * screen.  One column of the field is kept free at the right so the cursor
 * standing at the END of the text has a cell of its own to sit in - the
 * same arithmetic InputBox used when this code was its own, so a box built
 * on the field scrolls exactly where it always did. */
void TextField::ClampCursor( void )
{
    if ( cur < 0 )   cur = 0;
    if ( cur > len ) cur = len;

    /* No width yet: the text was set before the field was placed, which is
     * the usual order.  Nothing is scrolled until there is a width to
     * scroll within - or it would be scrolled right out of the field. */
    if ( width < 1 ) { scroll = 0; return; }
    if ( width < 3 ) { scroll = cur; return; }

    if ( cur < scroll )              scroll = cur;
    if ( cur > scroll + width - 2 )  scroll = cur - ( width - 2 );
    if ( scroll < 0 )                scroll = 0;
}

void TextField::InsertChar( char c )
{
    int i;

    if ( len >= maxlen ) return;
    for ( i = len; i > cur; --i ) buffer[i] = buffer[i-1];
    buffer[cur] = c;
    ++len;
    ++cur;
    buffer[len] = '\0';
}

void TextField::DeleteAt( int index )
{
    int i;

    if ( index < 0 || index >= len ) return;
    for ( i = index; i < len - 1; ++i ) buffer[i] = buffer[i+1];
    --len;
    buffer[len] = '\0';
}

/* The whole field, then the one cell the cursor is on in the opposite
 * colours - and that second part only with the focus. */
void TextField::Draw( void )
{
    char out[TF_TEXT_MAX];
    char cell[2];
    int  i;

    if ( !IsTextMode() ) return;
    if ( width < 1 ) return;

    MouseHide guard( mouse );

    ClampCursor();

    for ( i = 0; i < width; ++i )
    {
        int src = scroll + i;
        if ( src < len ) out[i] = masked ? (char)250 : buffer[src];
        else             out[i] = ' ';
    }
    out[width] = '\0';

    DfTextOut( (short)( row + 1 ), (short)( col + 1 ), out, fg_color, bg_color );

    if ( focus && cur - scroll >= 0 && cur - scroll < width )
    {
        cell[0] = ( cur < len ) ? ( masked ? (char)250 : buffer[cur] ) : ' ';
        cell[1] = '\0';
        DfTextOut( (short)( row + 1 ), (short)( col + 1 + ( cur - scroll ) ),
                   cell, FG_WHITE, BG_BLACK );
    }
}

boolean TextField::HandleEvent( const Event &e )
{
    if ( e.kind == Event::EV_MOUSE )
    {
        if ( !( e.buttons & LEFT_BUTTON ) || !HitTest( e.x, e.y ) ) return FALSE;

        focus = 1;
        cur = scroll + ( e.x - col );
        if ( cur > len ) cur = len;
        Draw();
        return TRUE;
    }

    if ( e.kind != Event::EV_KEY || !focus ) return FALSE;

    switch ( e.key )
    {
    case KEY_LEFT:              if ( cur > 0 )   --cur;   break;
    case KEY_RIGHT:             if ( cur < len ) ++cur;   break;
    case ( 0x100 | _KB_HOME ):  cur = 0;                  break;
    case ( 0x100 | _KB_END ):   cur = len;                break;
    case ( 0x100 | _KB_DEL ):   DeleteAt( cur );          break;
    case 8:
        if ( cur > 0 ) { DeleteAt( cur - 1 ); --cur; }
        break;
    default:
        if ( e.key < 32 || e.key > 126 ) return FALSE;
        InsertChar( (char)e.key );
        break;
    }

    Draw();
    return TRUE;
}
