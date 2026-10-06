/*===========================================================================
 * DLGBOX.HPP  -  Modal dialog box class definition (OK, OKCancel, YesNo, etc.)
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 * TWO WAYS TO SIZE ONE.  A caller that knows the shape it wants calls
 * SetCoordinates and gets exactly that box, and the message is fitted into
 * it.  A caller that just has something to say sets the message and lets the
 * box size ITSELF around it - AutoSize wraps the text, takes the width from
 * the longest line and the height from the count, and centres the result.
 * Show() auto-sizes when no coordinates were ever set, so the short way is
 * simply to not mention a rectangle.
 *
 * The message may contain '\n', which starts a new line where it stands.
 * Everything else is wrapped at word boundaries.
 *
 * For the common case - say this, wait for OK - there is one call:
 *
 *     DlgBox::Message( FG_BLACK, BG_WHITE, "Error", text, DB_OK, mouse );
 *===========================================================================*/
// class definition file for DlgBox class

#ifndef DLGBOX_HPP
#define DLGBOX_HPP

#include "structs.hpp"
#include "popup.hpp"
#include "button.hpp"

class MSMouse;

/* Long enough for a screenful, because a screenful is what the longest
 * message a dialog can show comes to.  It was 512, which a page of keyboard
 * help is longer than: the text was cut at the 511th character and the box
 * was sized, correctly, around what was left. */
#define DLG_MSG_MAX  1024

/* Four buttons is what a row of them fits on a narrow box, and four is what
 * the question that needed more than three turned out to want: Yes, No, Yes
 * to All, No to All. */
#define DLG_MAX_BUTTONS  4
#define DLG_LABEL_MAX   14

class DlgBox : public Popup
{
private:
    DlgBox( const DlgBox & );
    DlgBox &operator=( const DlgBox & );

    char custom[DLG_MAX_BUTTONS][DLG_LABEL_MAX];
    int  custom_count;              // 0 = use 'buttons' instead

    /* The labels and the enum values for whichever set is in force.  Both
     * Show and ShowEx need it and neither may work it out differently. */
    int  BuildButtons( const char **labels, button_return_value_t *values );

public:
    DlgBox( const foreground_color_t foreground, const background_color_t background );
    DlgBox( const foreground_color_t foreground, const background_color_t background, const char* message );
    DlgBox( const foreground_color_t foreground, const background_color_t background, const dialog_buttons_t* buttons );
    DlgBox( const foreground_color_t foreground, const background_color_t background, const dialog_buttons_t* buttons, const char* message );

    char message[DLG_MSG_MAX];
    dialog_buttons_t buttons;

    /* Place the lines as ONE BLOCK: they share a left edge, and
     * text_justification says where the block goes.  For text with columns
     * in it - a table of keys - which centring line by line takes apart.
     * Off by default, so every existing dialog is laid out as it was. */
    boolean align_block;

    // methods
    virtual void Draw( void );
    button_return_value_t Show( void );

    /*---- Buttons of your own ---------------------------------------------- *
     * dialog_buttons_t covers the five combinations that keep coming up, and
     * a question outside them used to mean writing the whole dialog by hand:
     * XArchive's overwrite prompt - Yes / No / Yes All / No All - was
     * eighty-seven lines sitting next to a class that did everything else it
     * needed.  Name the buttons instead:
     *
     *     static const char *b[] = { "Yes", "No", "Yes All", "No All" };
     *     dlg.SetButtons( b, 4 );
     *     switch ( dlg.ShowEx() ) { ... }
     *
     * The FIRST LETTER of each label is its shortcut, as with the built-in
     * sets, so two buttons starting with the same letter is a mistake the
     * caller has to not make ("Yes All" and "No All" work because Y, N, A
     * and O are picked from what is left - see ShowEx).
     *----------------------------------------------------------------------*/
    void SetButtons( const char * const *labels, int n );

    /* Which button, as an INDEX - 0 for the leftmost.  The answer for a
     * dialog with its own buttons, and it works for the built-in sets too.
     * Enter picks the first, Esc the last: a dialog always has a way out
     * that does nothing, and it is always the one on the right. */
    int ShowEx( void );

    /* Replace the message.  Longer text is cut rather than overrunning - a
     * dialog is not a text viewer, and TextView is. */
    void SetMessage( const char* text );

    /* Size the box to the message and centre it on the screen.  Safe to call
     * more than once, and safe to call before or after SetMessage; Show()
     * calls it for any dialog that never had coordinates set. */
    void AutoSize( void );

    /* Say something and wait.  Builds the dialog, sizes it to the text,
     * shows it, and returns which button was used.  'title' and 'mouse' may
     * both be null. */
    static button_return_value_t Message( const foreground_color_t foreground,
                                          const background_color_t background,
                                          const char* title, const char* text,
                                          const dialog_buttons_t buttons,
                                          MSMouse* mouse );
};

#endif

