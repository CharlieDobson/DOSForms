/*===========================================================================
 * INPUTBOX.HPP - Modal single-line text input.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * A centered Popup with a prompt and one editable field.  Show() saves the
 * screen, runs its own keystroke loop, restores the screen, and returns TRUE
 * on accept.
 *
 * IT IS A REAL FIELD, not a typing buffer: Left/Right/Home/End move within
 * what has been typed, characters are INSERTED at the cursor, Backspace
 * takes the one behind it and Del the one under it.  The field scrolls
 * sideways to keep the cursor in view, so the text may be longer than the
 * box.  This is the difference between correcting the third character of a
 * path and retyping the path, and the widget it replaced could only do the
 * second.
 *
 * THE FIELD IS A TextField.  It was this class's own code until the toolkit
 * had a second dialog with a field in it; now there is one field, in
 * TEXTFLD.CPP, and a build that includes INPUTBOX.CPP needs that as well.
 *
 * The box sizes itself to its prompt rather than the other way round: the
 * width starts at IB_W_MIN, grows to hold a longer prompt, and stops at the
 * screen.  Whatever is left over is CUT, so a prompt built around a file
 * name cannot run off the right border and paint over the frame.
 *===========================================================================*/
#ifndef INPUTBOX_HPP
#define INPUTBOX_HPP

#include "popup.hpp"
#include "button.hpp"
#include "textfld.hpp"
#include "structs.hpp"

#define IB_TEXT_MAX  TF_TEXT_MAX    /* buffer, including the NUL           */
#define IB_W_MIN      62            /* narrowest box we will build         */

class InputBox : public Popup
{
    char prompt[128];       // already clipped to what boxW can show
    TextField field;
    int  boxW;              // decided in the constructor, from the prompt

    Button btnOk;           // flat single-row "[ OK ]"     (see LayOutButtons)
    Button btnCancel;       // flat single-row "[ Cancel ]"
    Button btnSkip;         // optional third button, off unless SetSkipButton
    int    hasSkip;

    int  FieldWidth( void ) const;
    void LayOutField( void );
    void LayOutButtons( void );
    void DrawButtons( void );
    void Place( void );     // centre the box for the current screen size

    InputBox( const InputBox & );
    InputBox &operator=( const InputBox & );

public:
    InputBox( const foreground_color_t foreground, const background_color_t background,
              const char *titleText, const char *promptText );

    void SetText( const char *initial );
    const char *Text( void ) const { return field.Text(); }

    /* Cap what may be typed.  Clamped to the buffer, so a caller may ask for
     * a DOS path's 79 characters without knowing how big the buffer is. */
    void SetMaxLength( int n );

    /* Add a third button between OK and Cancel.  Off by default, so every
     * existing two-button caller is unaffected.  F3 works it from the
     * keyboard: the field eats printable keys, so a letter cannot be the
     * shortcut and a function key is what is left. */
    void SetSkipButton( void );

    /* Draw the field as dots rather than characters.  For passwords: the
     * buffer is unaffected, only the painting. */
    void SetMasked( void );

    boolean Show( void );   // TRUE = accepted, FALSE = cancelled

    /* The three-way answer.  Show() is this with SKIP folded into CANCEL, kept
     * because most callers have nothing to skip. */
    enum { IB_CANCEL = 0, IB_OK = 1, IB_SKIP = 2 };
    int ShowEx( void );

    /* Self-test only: draw exactly what Show() draws, minus the modal loop, so
     * a headless harness can snapshot it (then call RestoreWindow).  Reports
     * the rectangles the two buttons will hit-test against, so the harness can
     * check them against the cells actually painted on screen. */
    void TestOpen( int *okRow, int *okCol, int *okEndCol,
                   int *cancelCol, int *cancelEndCol );
};

#endif
