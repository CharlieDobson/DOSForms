DOSFORMS - a text-mode UI toolkit for DOS
=========================================

    S:\CPP\DUSOURCE\MISC\DOSFORMS

    Open Watcom 1.9 and Microsoft Visual C++ 1.52c, 16-bit real mode (large
    model) and 32-bit protected mode (DOS/32A) from the same sources.  Used
    by DiskStat, REVEAL, Setup and XArchive.

    Every class is documented in its own header, and the headers are the
    reference - this file is the map.


WHAT IS IN IT
-------------

    The frame

      DOSFORMS    the base class every widget derives from: colours, the
                  mouse, the screen grid, DrawBox/FillBox, and DfVectorGuard
                  (put the real-mode interrupt vectors back at exit).
      SCREEN      THE RENDERER SEAM.  A widget paints by calling DfTextOut
                  and friends, never graph.h, so the same widget draws in a
                  text mode or a graphics one without knowing which.
      VGAGFX      the graphics back end: VGA mode 12h, 640x480, an 80x30
                  grid of 8x16 cells, icons, a pixel-resolution progress
                  bar, and PICTURES - a rectangle of cells whose pixels the
                  program draws itself, in horizontal runs (DfPictureOpen /
                  DfPictureSpan; see SCREEN.HPP).  A picture lives in the
                  grid like an icon does, so a menu or dialog over it saves
                  and restores it, and a drop shadow dims it.  DiskStat's pie
                  chart is one.  BOTH BITNESSES: XArchive uses it 32-bit,
                  DiskStat and REVEAL 16-bit (each behind a /G switch).
                  Linked only by a program that wants it - one that does not
                  cannot end up in a mode it cannot paint.
      EVENT       one input event: a key, or a mouse press with a cell.
      APP         the whole screen: a desktop backdrop plus child widgets,
                  and the event loop that feeds them.
      DESKTOP     the backdrop on its own.
      MSMOUSE     the INT 33h driver.
      MOUSHIDE    a scope guard that hides the pointer while you paint.  Use
                  it; do not call Hide/Show by hand.

    Widgets on the screen

      TITLEBAR    the top row, with a [-] close box.
      STATBAR     the bottom row.  Print / Printf - and it REMEMBERS what it
                  says, so a repaint puts it back.  (It did not, and every
                  program using one had grown a "now restore the status
                  line" call to work around that.)
      MENU        the menu bar and its dropdowns.
      LABEL       a line of text.
      BUTTON      a push button, or a flat one-row "[ OK ]" (SetSingleRow).
      LISTBOX     a scrolling list: rows from a callback, a header, marks
                  (Space/Ins/+/-/*), per-row icons, a scrollbar, and
                  double-click.  ALSO A TREE: give it five callbacks
                  (SetTreeProvider) and it draws indentation and [+]/[-]
                  markers into a name field it owns at the left, so the
                  columns to the right of the name stay put whatever depth
                  a row is at.  '+' and '-' then expand and collapse
                  rather than marking everything.
      FIELDPAN    a panel of label / value rows with the value column lined
                  up, plus headings, blanks and rules.  The shape of every
                  "here is what this machine has" page, which was being
                  drawn by hand with tab stops and absolute coordinates.
      SEGBAR      a proportional bar split into coloured segments, with a
                  legend under it - what a pie chart becomes on a character
                  grid.  The caller supplies each figure ALREADY WRITTEN
                  OUT, so the widget never has to decide between KB and MB.
      CHECKBOX    CheckBox and RadioGroup.  Mouse anywhere on the row, a
                  hotkey letter, and (for a radio group that has been
                  clicked on) the arrow keys.

    Boxes that take over

      POPUP       a bordered window that SAVES what it covers and puts it
                  back.  Everything below is one.
      DLGBOX      a message or a question: OK / OKCancel / YesNo /
                  NextBack / InstallCancel, or up to four buttons you name
                  yourself (SetButtons + ShowEx, which answers with an
                  index).  Sizes itself around the text unless you give it
                  a rectangle.  DlgBox::Message is the one-call version.
      INPUTBOX    one line of text, properly editable - Left/Right/Home/End,
                  insert, Del, a sideways scroll, and an optional mask for
                  passwords.
      TEXTVIEW    a read-only viewer over a file or a string, with Find.
      FILEBOX     choose a file, choose a folder, or name a file to save.
      PROGBOX     ProgressBox (a bar and a way out) and WaitBox (one line,
                  for work that cannot say how far along it is).

    The model behind the chooser

      DIRSCAN     a directory as a list of rows - drives, the way up,
                  subdirectories, files - with a wildcard filter.  FileBox
                  sits on one; so does a program whose own main window is a
                  browser, which is how XArchive uses it.


WHAT LINKS AGAINST WHAT
-----------------------

    Build scripts name the .CPP files one by one, so these matter:

      everything          needs DOSFORMS.CPP
      any widget          needs MOUSHIDE.CPP (and MSMOUSE.CPP if -dUSE_MOUSE)
      DLGBOX              needs POPUP + BUTTON
      INPUTBOX            needs POPUP + BUTTON
      TEXTVIEW            needs POPUP + BUTTON + INPUTBOX  (Find asks for a
                          string, and asking for a string is an InputBox)
      FILEBOX             needs POPUP + BUTTON + LISTBOX + DIRSCAN
      PROGBOX             needs POPUP + BUTTON
      APP                 needs DESKTOP
      FIELDPAN            nothing but the base
      SEGBAR              nothing but the base
      CHECKBOX            nothing but the base

    Nothing needs VGAGFX.CPP.  It is an option, not a layer.


THREE RULES THAT ARE NOT OBVIOUS
--------------------------------

    SETTERS CHANGE STATE; DRAW PAINTS.  SetChecked, Select, SetData, Add -
    none of them put anything on the screen.  It is not only tidiness: a
    control that painted from its setter would appear BEFORE the Popup it
    belongs to had saved what was underneath it, and a copy of the control
    would be left on the screen when that dialog closed.  HandleEvent is
    the exception and calls Draw itself, because a click has to show its
    effect at once.

    A MODAL BOX MUST RESYNC THE MOUSE ON THE WAY OUT.  A box that runs its
    own input loop eats the press and release edges App::Run never sees, so
    the next poll after it closes can invent a click on whatever the pointer
    is over - which, the moment a dialog closes, is usually the thing that
    opened it.  Every Show() here ends with DfModalFinished(), and App
    registers itself to be told.  A new modal widget must do the same.

    THE CARD IS NOT YOURS ALONE.  The mouse driver paints its cursor through
    the same VGA registers the graphics renderer writes, from an interrupt,
    and expects to find them as the BIOS left them.  VGAGFX hands them back
    at the end of every flush for that reason.  This cost a release: a
    pointer you could see through, which no emulator could reproduce because
    an emulated INT 33h cursor never reads a VGA register.


BUILDING AND CHECKING IT
------------------------

    MKDEMO.CMD builds DFDEMO - every widget in one program - BOTH WAYS:

        DFDEMO.EXE     16-bit real mode, large model, 8086
        DFDEMO32.EXE   32-bit DOS/32A

    Run it and use the menus.  Run "DFDEMO /T" and it drives itself: every
    modal opens through the toolkit's peek hook, paints, and closes, so the
    whole thing runs without a keyboard and prints how many dialogs it got
    through.

    THIS IS THE BUILD CHECK, not just a sample.  Compiling every file only
    proves the headers agree.  A program that calls into all of them is what
    proves the objects do - that nothing has quietly picked up a 32-bit-only
    call, and that no widget needs a class its caller's build script does
    not list.  The 16-bit half is the half that catches things.


WHAT IS STILL DRAWN BY HAND, AND WHERE
--------------------------------------

    The widgets above exist so that the programs using this toolkit can
    stop drawing their own.  Three modules still do, but since 2026-09-25
    they draw THROUGH THE RENDERER - DfTextOut, never graph.h - which is
    what lets DiskStat and REVEAL put up the graphics screen with /G:

      DISKSTAT\FOLDTREE.CPP   a scrolling folder tree      -> LISTBOX's tree
      DISKSTAT\USAGEBAR.CPP   a bar with a legend          -> SEGBAR
      REVEAL\DOSMAIN.CPP      eight pages of label/value   -> FIELDPAN
                              (67 RvPrintf calls, and its own emulation of
                              Symantec's tab stops to place them)

    Moving them onto those widgets is still worth doing, but it is a
    change to what the text screens look like, and nothing waits on it.

    The rule that made /G possible, for anything written from here on:
    A MODULE THAT PAINTS BEHIND THE RENDERER'S BACK CANNOT BE ON THE
    GRAPHICS SCREEN.  One _outtext is enough - on the graphics screen it
    draws with graph.h's own text routine, straight onto the card, into a
    cell the renderer's shadow grid knows nothing about, and the next flush
    or Popup restore paints over it or puts back what was there before.

    Setup is a separate problem: it does not link DOSForms at all.
