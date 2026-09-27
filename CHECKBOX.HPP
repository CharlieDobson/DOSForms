/*===========================================================================
 * CHECKBOX.HPP - CheckBox and RadioGroup.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *
 *     [X] Add the utilities to my PATH          a CheckBox
 *
 *     (*) Typical                               a RadioGroup
 *     ( ) Compact
 *     ( ) Custom
 *
 * The two settings controls a text UI needs, and the reason they were
 * missing is that both programs that wanted one said it in prose instead -
 * Setup asks about PATH with a sentence and a keystroke, and XArchive puts
 * its options on the menu bar with a star beside them.  Neither is wrong;
 * both stop working the moment there are four settings rather than one, and
 * that is when a dialog wants these.
 *
 * FOCUS, OR THE LACK OF IT.  DOSForms has no focus manager: App offers each
 * event to every child until one takes it.  So these work the way the rest
 * of the toolkit does -
 *
 *   the MOUSE always works: a click anywhere on the row toggles a CheckBox
 *     or picks a RadioGroup's line;
 *   a HOTKEY always works, if the caller sets one - SetHotkey('P') makes a
 *     bare P toggle it, which is the DOS-dialog idiom;
 *   the ARROW KEYS work on a RadioGroup only AFTER it has been clicked on,
 *     because until something has been pointed at there is no answer to
 *     "which group do the arrows mean?".
 *
 * A caller that wants Tab to move between controls writes that itself - it
 * knows the order they are in and the toolkit does not.
 *
 * SETTERS DO NOT PAINT.  SetChecked and Select change the state and fire
 * the change callback; Draw is what puts it on the screen, and HandleEvent
 * calls Draw itself because a click has to show its effect at once.  That
 * is the rule everywhere in this toolkit, and here it is load-bearing: a
 * control that painted itself from its setter would appear on the screen
 * before the dialog it belongs to had saved what was underneath, and a copy
 * of it would be left behind when that dialog closed.  (It was, and that is
 * how this note came to be written.)
 *===========================================================================*/
#ifndef CHECKBOX_HPP
#define CHECKBOX_HPP

#include "dosforms.hpp"
#include "structs.hpp"

#define CB_LABEL_MAX   48
#define RG_MAX_ITEMS    8

class CheckBox : public DOSForms
{
    coordinates_t coordinates;      // one row; end_column is the label's end
    char    label[CB_LABEL_MAX];
    boolean checked;
    char    hotkey;                 // 0 for none; matched case-insensitively

    CheckBox( const CheckBox & );
    CheckBox &operator=( const CheckBox & );

public:
    CheckBox( const foreground_color_t foreground, const background_color_t background );

    /* 0-based row and column of the '[' .  The width follows from the label,
     * so there is no way to draw one and hit-test another. */
    void SetPosition( short row, short col );
    void SetLabel( const char *text );
    void SetHotkey( char key );

    boolean Checked( void ) const { return checked; }
    void    SetChecked( boolean on );
    void    Toggle( void );

    /* Fired after the state changes, by any route. */
    typedef void (*Changed)( CheckBox *box, void *user );
    Changed on_change;
    void   *on_change_user;

    boolean HitTest( short x, short y ) const;

    virtual void Draw( void );
    virtual boolean HandleEvent( const Event &e );
};

class RadioGroup : public DOSForms
{
    coordinates_t coordinates;      // top row, left column of the first '('
    char    item[RG_MAX_ITEMS][CB_LABEL_MAX];
    char    key[RG_MAX_ITEMS];      // per-item hotkey, or 0
    int     count;
    int     chosen;
    int     focused;                // arrows work once this is set

    RadioGroup( const RadioGroup & );
    RadioGroup &operator=( const RadioGroup & );

public:
    RadioGroup( const foreground_color_t foreground, const background_color_t background );

    void SetPosition( short row, short col );
    boolean Add( const char *label, char hotkey );   // hotkey 0 for none

    int  Selected( void ) const { return chosen; }
    void Select( int index );

    typedef void (*Changed)( RadioGroup *group, void *user );
    Changed on_change;
    void   *on_change_user;

    int  HitRow( short x, short y ) const;   // item under (x,y), or -1

    virtual void Draw( void );
    virtual boolean HandleEvent( const Event &e );
};

#endif
