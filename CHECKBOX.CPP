/*===========================================================================
 * CHECKBOX.CPP - CheckBox and RadioGroup.  See CHECKBOX.HPP.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
#include "checkbox.hpp"
#include "moushide.hpp"
#include "msmouse.hpp"          // LEFT_BUTTON
#include "screen.hpp"           // DfTextOut - the renderer, text or graphics
#include <string.h>
#include <ctype.h>

/*===========================================================================*
 * CheckBox
 *===========================================================================*/

CheckBox::CheckBox( const foreground_color_t foreground,
                    const background_color_t background )
    : DOSForms( foreground, background )
{
    coordinates.row = coordinates.column = 0;
    coordinates.end_row = coordinates.end_column = 0;
    label[0] = '\0';
    checked  = FALSE;
    hotkey   = 0;
    on_change = 0;
    on_change_user = 0;
}

/* "[X] " is four cells, then the label.  end_column is worked out here and
 * nowhere else, so the cells that are drawn and the cells HitTest accepts
 * are the same cells by construction - the same rule as Button. */
void CheckBox::SetPosition( short row, short col )
{
    coordinates.row = coordinates.end_row = row;
    coordinates.column = col;
    coordinates.end_column = (short)( col + 3 + (short)strlen( label ) );
}

void CheckBox::SetLabel( const char *text )
{
    label[0] = '\0';
    if ( text ) { strncpy( label, text, sizeof( label ) - 1 );
                  label[sizeof( label ) - 1] = '\0'; }
    coordinates.end_column = (short)( coordinates.column + 3 + (short)strlen( label ) );
}

void CheckBox::SetHotkey( char key ) { hotkey = key; }

/* Changes the state and tells whoever asked - but does NOT paint.  Every
 * other setter in the toolkit works that way (ListBox::SetData, FieldPane::
 * Add), and a setter that painted would put the control on the screen
 * before the window it belongs to has saved what was underneath, which
 * leaves a copy of the control behind when that window closes. */
void CheckBox::SetChecked( boolean on )
{
    if ( ( on != FALSE ) == ( checked != FALSE ) ) return;
    checked = (boolean)( on ? TRUE : FALSE );
    if ( on_change ) on_change( this, on_change_user );
}

void CheckBox::Toggle( void )
{
    SetChecked( (boolean)( checked ? FALSE : TRUE ) );
}

boolean CheckBox::HitTest( short x, short y ) const
{
    if ( y != coordinates.row ) return FALSE;
    if ( x < coordinates.column || x > coordinates.end_column ) return FALSE;
    return TRUE;
}

void CheckBox::Draw( void )
{
    char out[CB_LABEL_MAX + 8];

    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    out[0] = '[';
    out[1] = (char)( checked ? 'X' : ' ' );
    out[2] = ']';
    out[3] = ' ';
    out[4] = '\0';
    strncat( out, label, sizeof( out ) - 5 );

    DfTextOut( (short)( coordinates.row + 1 ),        /* 0-based -> 1-based */
               (short)( coordinates.column + 1 ),
               out, fg_color, bg_color );
}

boolean CheckBox::HandleEvent( const Event &e )
{
    if ( e.kind == Event::EV_MOUSE )
    {
        if ( ( e.buttons & LEFT_BUTTON ) && HitTest( e.x, e.y ) )
        {
            Toggle();
            Draw();
            return TRUE;
        }
        return FALSE;
    }

    if ( e.kind == Event::EV_KEY && hotkey &&
         e.key < 0x100 &&
         toupper( (unsigned char)e.key ) == toupper( (unsigned char)hotkey ) )
    {
        Toggle();
        Draw();
        return TRUE;
    }
    return FALSE;
}

/*===========================================================================*
 * RadioGroup
 *===========================================================================*/

RadioGroup::RadioGroup( const foreground_color_t foreground,
                        const background_color_t background )
    : DOSForms( foreground, background )
{
    coordinates.row = coordinates.column = 0;
    coordinates.end_row = coordinates.end_column = 0;
    count   = 0;
    chosen  = 0;
    focused = 0;
    on_change = 0;
    on_change_user = 0;
}

void RadioGroup::SetPosition( short row, short col )
{
    coordinates.row = row;
    coordinates.column = col;
    coordinates.end_row = (short)( row + ( count > 0 ? count - 1 : 0 ) );
}

boolean RadioGroup::Add( const char *label, char hotkey )
{
    if ( count >= RG_MAX_ITEMS ) return FALSE;

    item[count][0] = '\0';
    if ( label ) { strncpy( item[count], label, CB_LABEL_MAX - 1 );
                   item[count][CB_LABEL_MAX - 1] = '\0'; }
    key[count] = hotkey;
    ++count;
    coordinates.end_row = (short)( coordinates.row + count - 1 );
    return TRUE;
}

/* Changes the selection without painting - see CheckBox::SetChecked. */
void RadioGroup::Select( int index )
{
    if ( index < 0 || index >= count || index == chosen ) return;
    chosen = index;
    if ( on_change ) on_change( this, on_change_user );
}

int RadioGroup::HitRow( short x, short y ) const
{
    int i = y - coordinates.row;

    if ( i < 0 || i >= count ) return -1;
    if ( x < coordinates.column ) return -1;
    if ( x > coordinates.column + 3 + (short)strlen( item[i] ) ) return -1;
    return i;
}

void RadioGroup::Draw( void )
{
    char out[CB_LABEL_MAX + 8];
    int  i;

    if ( !IsTextMode() ) return;

    MouseHide guard( mouse );

    for ( i = 0; i < count; ++i )
    {
        out[0] = '(';
        out[1] = (char)( ( i == chosen ) ? '*' : ' ' );
        out[2] = ')';
        out[3] = ' ';
        out[4] = '\0';
        strncat( out, item[i], sizeof( out ) - 5 );

        DfTextOut( (short)( coordinates.row + 1 + i ),
                   (short)( coordinates.column + 1 ),
                   out, fg_color, bg_color );
    }
}

boolean RadioGroup::HandleEvent( const Event &e )
{
    int i;

    if ( e.kind == Event::EV_MOUSE )
    {
        if ( e.buttons & LEFT_BUTTON )
        {
            i = HitRow( e.x, e.y );
            if ( i >= 0 )
            {
                /* Clicking IS the focus gesture: the arrows mean this group
                 * from now on, because this is the group that was pointed
                 * at.  See CHECKBOX.HPP. */
                focused = 1;
                Select( i );
                Draw();
                return TRUE;
            }
        }
        return FALSE;
    }

    if ( e.kind != Event::EV_KEY ) return FALSE;

    /* A hotkey works whether or not this group has been pointed at - it
     * names one item, so there is nothing ambiguous about it. */
    if ( e.key < 0x100 )
    {
        for ( i = 0; i < count; ++i )
            if ( key[i] && toupper( (unsigned char)e.key ) ==
                           toupper( (unsigned char)key[i] ) )
            {
                focused = 1;
                Select( i );
                Draw();
                return TRUE;
            }
    }

    if ( !focused ) return FALSE;

    if ( e.key == KEY_UP )
    {
        if ( chosen > 0 ) { Select( chosen - 1 ); Draw(); }
        return TRUE;
    }
    if ( e.key == KEY_DOWN )
    {
        if ( chosen < count - 1 ) { Select( chosen + 1 ); Draw(); }
        return TRUE;
    }
    return FALSE;
}
