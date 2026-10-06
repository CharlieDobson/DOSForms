/*===========================================================================
 * TEXTFLD.HPP - One line of editable text, as a control.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * The field InputBox has, without the box: something a dialog with other
 * controls in it can place on a row of its own.  Left/Right/Home/End move
 * within the text, characters are inserted at the cursor, Backspace takes
 * the one behind it and Del the one under it, and the text scrolls sideways
 * to keep the cursor in view, so it may be longer than the field is wide.
 * A click in the field puts the cursor where it was clicked.
 *
 * FOCUS IS THE CALLER'S TO GIVE.  A dialog has several controls and one
 * keyboard, and only the dialog knows which of them the keys are for.  The
 * field takes keys only while it has the focus, and shows its cursor only
 * then - the cursor is how the user knows where typing will go.  A click in
 * the field takes the focus itself, since that is what clicking in a field
 * means; the dialog asks HasFocus afterwards and tells the other controls.
 *
 * SetText, SetFocus and the rest change state and do not paint - the rule
 * for every setter in the toolkit.  HandleEvent paints what it changed.
 *===========================================================================*/
#ifndef TEXTFLD_HPP
#define TEXTFLD_HPP

#include "dosforms.hpp"
#include "structs.hpp"

#define TF_TEXT_MAX  256        /* buffer, including the NUL               */

class TextField : public DOSForms
{
    short row, col, width;      // 0-based cell of the first column, and cells
    char  buffer[TF_TEXT_MAX];
    int   len;                  // characters in buffer
    int   cur;                  // cursor: 0..len (len = past the last one)
    int   scroll;               // first character shown
    int   maxlen;
    int   focus;
    int   masked;               // 1 = draw dots instead of the characters

    void ClampCursor( void );   // keep 'cur' in range and in view
    void InsertChar( char c );
    void DeleteAt( int index );

    TextField( const TextField & );
    TextField &operator=( const TextField & );

public:
    TextField( const foreground_color_t foreground,
               const background_color_t background );

    void SetPosition( short fieldRow, short fieldCol, short fieldWidth );

    /* Replace the text and leave the cursor at the end of it, ready to add
     * to - which is the usual reason for putting text in a field. */
    void SetText( const char *initial );
    const char *Text( void ) const  { return buffer; }
    int  Length( void ) const       { return len; }

    void SetMaxLength( int n );     // clamped to the buffer
    void SetMasked( boolean on );   // passwords: dots, the text is unchanged

    void SetFocus( boolean on );
    boolean HasFocus( void ) const  { return (boolean)( focus ? TRUE : FALSE ); }

    boolean HitTest( short x, short y ) const;

    virtual void Draw( void );
    virtual boolean HandleEvent( const Event &e );
};

#endif
