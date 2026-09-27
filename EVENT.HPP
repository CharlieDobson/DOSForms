/*===========================================================================
 * EVENT.HPP  -  Input event passed to DOSForms::HandleEvent
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS
 *===========================================================================*/
// input event definition

#ifndef EVENT_HPP
#define EVENT_HPP

// One input event. App::Run() builds these from the keyboard and mouse and
// offers them to forms via HandleEvent(). Mouse coordinates are text cells.
struct Event
{
    enum Kind { EV_NONE, EV_KEY, EV_MOUSE };

    Kind  kind;
    int   key;        // EV_KEY: ASCII, or 0x100|scancode for extended keys
    short x;          // EV_MOUSE: text column (0-based)
    short y;          // EV_MOUSE: text row (0-based)
    int   buttons;    // EV_MOUSE: LEFT_BUTTON / RIGHT_BUTTON / CENTER_BUTTON bits
};

// common keys (App::Run() reports extended keys as 0x100 | BIOS scancode)
#define KEY_ESC     27
#define KEY_ENTER   13
#define KEY_UP      0x148
#define KEY_DOWN    0x150
#define KEY_LEFT    0x14B
#define KEY_RIGHT   0x14D

// A bare Alt, pressed and released without another key in between - the
// keystroke that puts the highlight on a menu bar.  There is no scan code for
// it: Alt is a modifier, so the BIOS never queues anything.  App::PollEvent
// watches the BIOS shift-state byte for the release edge and synthesizes this
// instead, which is why it lives above the 0x100 extended-key range rather
// than in it.  A form that does not want it simply never tests for it.
#define KEY_ALT     0x200

#endif
