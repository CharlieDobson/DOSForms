/*===========================================================================
 * DFDEMO.CPP - Every DOSForms widget, in one program.
 * Target: MSVC 1.52c / Open Watcom 1.9  DOS   (16-bit AND 32-bit)
 *
 * TWO JOBS, and the second is the one that matters.
 *
 * As a DEMO it is a worked example of the whole toolkit: an App with a menu
 * bar and a status line, a TREE on the left, a field pane and a segmented
 * bar on the right, and a menu item for each of the modal widgets.  A
 * program being brought over to DOSForms can be read off it - including the
 * two things the toolkit deliberately does not do for you, which are how a
 * tree model answers the list's questions (see the callbacks below) and how
 * a settings dialog is assembled out of a Popup, some CheckBoxes and a
 * RadioGroup (see ActOptions).
 *
 * As a TEST it is the only thing that proves the toolkit LINKS.  Compiling
 * every file says the headers agree; only a program that calls into all of
 * them says the objects do - that nothing reaches for int386 in a 16-bit
 * build, that no widget quietly needs a class its build script does not
 * list.  MKDEMO.CMD builds it BOTH WAYS for exactly that reason, and the
 * 16-bit half is the half that has caught things.
 *
 * Run it with /T and it drives itself: every modal is opened through the
 * toolkit's peek hook, which paints the box, photographs nothing, and closes
 * it - so the whole thing runs headless in a few seconds and exits 0.
 *===========================================================================*/
#include "app.hpp"
#include "titlebar.hpp"
#include "statbar.hpp"
#include "menu.hpp"
#include "dlgbox.hpp"
#include "inputbox.hpp"
#include "listbox.hpp"
#include "progbox.hpp"
#include "textview.hpp"
#include "filebox.hpp"
#include "fieldpan.hpp"
#include "segbar.hpp"
#include "checkbox.hpp"
#include "msmouse.hpp"
#include "moushide.hpp"
#include "screen.hpp"
#include <graph.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>              // clock() - the demo's stand-in for real work

static App        *g_app   = 0;
static ListBox    *g_list  = 0;
static FieldPane  *g_pane  = 0;
static SegmentBar *g_bar   = 0;
static StatusBar  *g_stat  = 0;
static MSMouse    *g_mouse = 0;
static int         g_auto  = 0;     /* /T: drive every dialog from the peek */

/*===========================================================================*
 * A tree model, because the list does not hold one
 *
 * The widget asks about the row in front of it and nothing else, so the
 * model stays whatever the program already had.  This is the usual shape:
 * a flat array of nodes with a depth each, plus a list of which of them are
 * currently VISIBLE - a node's children are visible when every ancestor is
 * open.  Rebuild that list whenever something opens or shuts, and the row
 * count the toggle callback returns is its length.
 *===========================================================================*/
struct DemoNode
{
    const char   *name;
    int           depth;
    int           open;
    unsigned long size;
};

static DemoNode g_node[] = {
    { "DOSFORMS",      0, 1,      0L },
    {   "Frame",       1, 1,      0L },
    {     "DOSFORMS.CPP", 2, 0, 21456L },
    {     "APP.CPP",      2, 0,  8844L },
    {     "SCREEN.HPP",   2, 0,  9102L },
    {   "Widgets",     1, 0,      0L },
    {     "LISTBOX.CPP",  2, 0, 17330L },
    {     "MENU.CPP",     2, 0, 12008L },
    {     "BUTTON.CPP",   2, 0, 10521L },
    {   "Dialogs",     1, 0,      0L },
    {     "DLGBOX.CPP",   2, 0, 16700L },
    {     "INPUTBOX.CPP", 2, 0, 11240L },
    {     "FILEBOX.CPP",  2, 0, 15980L },
    { "XARCHIVE",      0, 0,      0L },
    {   "XARCTUI.CPP",  1, 0,142330L },
    {   "ARCVIEW.CPP",  1, 0, 18220L },
    { "DISKSTAT",      0, 0,      0L },
    {   "SCAN.CPP",     1, 0, 14900L },
    {   "FOLDTREE.CPP", 1, 0, 11760L }
};
#define NODE_COUNT ( (int)( sizeof( g_node ) / sizeof( g_node[0] ) ) )

static int g_vis[NODE_COUNT];       /* model index of each visible row */
static int g_visCount = 0;

/* Does node 'i' have children?  The next node being deeper is the whole
 * test, which is what a flat array with depths buys you. */
static int NodeHasKids( int i )
{
    return ( i + 1 < NODE_COUNT && g_node[i+1].depth > g_node[i].depth );
}

static int RebuildVisible( void )
{
    int i, skipDepth = -1;

    g_visCount = 0;
    for ( i = 0; i < NODE_COUNT; ++i )
    {
        /* Inside a closed node: skip until we come back out to its level. */
        if ( skipDepth >= 0 )
        {
            if ( g_node[i].depth > skipDepth ) continue;
            skipDepth = -1;
        }
        g_vis[g_visCount++] = i;
        if ( NodeHasKids( i ) && !g_node[i].open ) skipDepth = g_node[i].depth;
    }
    return g_visCount;
}

/*---- what the ListBox asks -------------------------------------------------*/

static int TreeDepth( void *user, int row )
{
    (void)user;
    return ( row >= 0 && row < g_visCount ) ? g_node[g_vis[row]].depth : 0;
}

static int TreeHasKids( void *user, int row )
{
    (void)user;
    return ( row >= 0 && row < g_visCount ) ? NodeHasKids( g_vis[row] ) : 0;
}

static int TreeExpanded( void *user, int row )
{
    (void)user;
    return ( row >= 0 && row < g_visCount ) ? g_node[g_vis[row]].open : 0;
}

/* Open or shut it, and answer with the NEW number of rows - the widget
 * cannot work that out for itself and must not guess. */
static int TreeToggle( void *user, int row )
{
    (void)user;
    if ( row < 0 || row >= g_visCount ) return g_visCount;
    g_node[g_vis[row]].open = !g_node[g_vis[row]].open;
    return RebuildVisible();
}

static void TreeLabel( void *user, int row, char *buf, int buflen )
{
    (void)user;
    buf[0] = '\0';
    if ( row < 0 || row >= g_visCount ) return;
    strncpy( buf, g_node[g_vis[row]].name, (size_t)buflen - 1 );
    buf[buflen - 1] = '\0';
}

/* The columns to the RIGHT of the name field - the widget owns everything
 * left of it, so this never has to know how deep the row is. */
static void TreeTail( void *user, int row, char *buf, int buflen )
{
    (void)user;
    buf[0] = '\0';
    if ( row < 0 || row >= g_visCount ) return;
    if ( g_node[g_vis[row]].size > 0 )
        sprintf( buf, "%10lu bytes", g_node[g_vis[row]].size );
    buf[buflen - 1] = '\0';
}

/*===========================================================================*
 * The rest of the demo
 *===========================================================================*/

static void Status( const char *s )
{
    if ( !g_stat ) return;
    g_stat->Print( s );
}

/* The headless driver.  Installed as the toolkit's modal peek, so every
 * dialog in the program calls it instead of waiting for a key - and counts
 * the calls, so the run can say how many modals actually PAINTED rather
 * than only that nothing crashed. */
static int g_peeks = 0;
static void AutoPeek( void ) { ++g_peeks; }

/* The two panes on the right.  Filled once; a real program refills them
 * whenever the thing they describe changes. */
static void FillPanes( void )
{
    if ( g_pane )
    {
        g_pane->Clear();
        g_pane->AddHeading( "Toolkit" );
        g_pane->Add(  "Classes",  "18" );
        g_pane->AddF( "Widgets",  "%d", 12 );
        g_pane->Add(  "Renderers","text, VGA 640x480" );
        g_pane->AddBlank();
        g_pane->AddHeading( "This build" );
#ifdef __386__
        g_pane->Add( "Model",   "32-bit flat" );
        g_pane->Add( "Extender","DOS/32A" );
#else
        g_pane->Add( "Model",   "16-bit large" );
        g_pane->Add( "Extender","none - real mode" );
#endif
        g_pane->AddF( "Screen",  "%d x %d", (int)DfCols(), (int)DfRows() );
        g_pane->AddF( "Graphics","%s", DfIsGraphics() ? "yes" : "no" );
    }

    if ( g_bar )
    {
        g_bar->Clear();
        g_bar->Add( "Used",   765900UL, "765.9 MB", FG_BLUE );
        g_bar->Add( "Folder",  53200UL,  "53.2 MB", FG_YELLOW );
        g_bar->Add( "Free",   249900UL, "249.9 MB", FG_GREEN );
    }
}

/*---- The menu actions ----------------------------------------------------- */

static void ActMessage( void *user )
{
    (void)user;
    DlgBox::Message( FG_BLACK, BG_WHITE, "Message",
        "This dialog was never given a rectangle, so it sized itself around "
        "these words.\n"
        "\n"
        "The blank line above is a '\\n' in the string: a paragraph break is "
        "something the caller says, not something the wrapper guesses.",
        DB_OK, g_mouse );
    Status( "Message shown" );
}

static void ActAsk( void *user )
{
    dialog_buttons_t yn = DB_YESNO;
    DlgBox dlg( FG_BRIGHT_WHITE, BG_BLUE, &yn,
        "Two buttons, and Esc picks the last one.  Is that what you expected?" );

    (void)user;
    strcpy( dlg.title, "Question" );
    dlg.border_style = BORDER_DOUBLE_LINE;
    dlg.SetMouse( g_mouse );
    Status( ( dlg.Show() == BUTTON_YES ) ? "You said Yes" : "You said No" );
}

/* Four buttons, named here rather than chosen from an enum - the question
 * an overwrite prompt asks, which used to mean writing the dialog by hand. */
static void ActFourWay( void *user )
{
    static const char *labels[] = { "Yes", "No", "Yes All", "No All" };
    static const char *said[]   = { "Yes", "No", "Yes to all", "No to all" };
    DlgBox dlg( FG_BLACK, BG_WHITE );
    int    which;

    (void)user;
    strcpy( dlg.title, "Overwrite?" );
    dlg.border_style = BORDER_DOUBLE_LINE;
    dlg.SetMouse( g_mouse );
    dlg.SetMessage( "C:\\DU\\README.TXT already exists.\n"
                    "\n"
                    "The shortcut letters are worked out from the labels: Y, "
                    "N, A and O, because \"No All\" can have neither N nor A." );
    dlg.SetButtons( labels, 4 );

    which = dlg.ShowEx();
    Status( ( which >= 0 && which < 4 ) ? said[which] : "Nothing" );
}

static void ActInput( void *user )
{
    InputBox box( FG_BLUE, BG_WHITE, "Input", "Type something, and edit it:" );
    char     line[96];

    (void)user;
    box.SetMouse( g_mouse );
    box.SetText( "C:\\DU\\EDIT ME" );
    if ( box.Show() ) sprintf( line, "You typed: %.60s", box.Text() );
    else              strcpy( line, "Cancelled" );
    Status( line );
}

static void ActPassword( void *user )
{
    InputBox box( FG_BLUE, BG_WHITE, "Password", "Nothing is echoed:" );

    (void)user;
    box.SetMouse( g_mouse );
    box.SetMasked();
    box.SetMaxLength( 32 );
    Status( box.Show() ? "Password taken" : "Cancelled" );
}

/*---- A settings dialog, assembled by hand --------------------------------- *
 * There is no dialog-container class in the toolkit, and this is what one
 * would have to do for you: put up a Popup, place the controls inside it,
 * and run a loop that offers each event to each of them until one takes it.
 * Forty lines, and it is the same forty lines every time - which is the
 * argument for a container class, if a third program ever wants one.
 *---------------------------------------------------------------------------*/
static void ActOptions( void *user )
{
    Popup      box( FG_BLACK, BG_WHITE, "Options" );
    CheckBox   cbPath( FG_BLACK, BG_WHITE );
    CheckBox   cbIcons( FG_BLACK, BG_WHITE );
    RadioGroup rgMode( FG_BLACK, BG_WHITE );
    Button     btnOk( FG_WHITE, BG_CYAN );
    Button     btnCancel( FG_WHITE, BG_CYAN );
    short      rows = DfRows() ? DfRows() : 25;
    short      cols = DfCols() ? DfCols() : 80;
    short      w = 46, h = 13;
    short      left = (short)( ( cols - w ) / 2 );
    short      top  = (short)( ( rows - h ) / 2 );
    int        lastB, done = 0;
    char       line[96];

    (void)user;

    box.SetMouse( g_mouse );
    box.border_style = BORDER_DOUBLE_LINE;
    box.SetCoordinates( top, left, (short)( top + h - 1 ), (short)( left + w - 1 ) );

    cbPath.SetMouse( g_mouse );
    cbPath.SetLabel( "Add the utilities to my PATH" );
    cbPath.SetHotkey( 'P' );
    cbPath.SetPosition( (short)( top + 2 ), (short)( left + 3 ) );
    cbPath.SetChecked( TRUE );

    cbIcons.SetMouse( g_mouse );
    cbIcons.SetLabel( "Show icons in lists" );
    cbIcons.SetHotkey( 'I' );
    cbIcons.SetPosition( (short)( top + 3 ), (short)( left + 3 ) );

    rgMode.SetMouse( g_mouse );
    rgMode.Add( "Typical", 'T' );
    rgMode.Add( "Compact", 'C' );
    rgMode.Add( "Custom",  'U' );
    rgMode.SetPosition( (short)( top + 7 ), (short)( left + 3 ) );

    btnOk.SetMouse( g_mouse );
    btnCancel.SetMouse( g_mouse );
    strcpy( btnOk.label, "OK" );
    strcpy( btnCancel.label, "Cancel" );
    btnOk.SetSingleRow( (unsigned short)( top + h - 2 ), (unsigned short)( left + 12 ) );
    btnCancel.SetSingleRow( (unsigned short)( top + h - 2 ), (unsigned short)( left + 22 ) );

    {
        MouseHide guard( g_mouse );
        box.Draw();
        DfTextOut( (short)( top + 6 ), (short)( left + 3 ), "Install:",
                   FG_BLUE, BG_WHITE );
        cbPath.Draw();
        cbIcons.Draw();
        rgMode.Draw();
        btnOk.Draw();
        btnCancel.Draw();
    }

    if ( DfModalPeek() )
    {
        DfModalPeek()();
        box.RestoreWindow();
        DfModalFinished();
        Status( "Options (headless)" );
        return;
    }

    lastB = ( g_mouse && g_mouse->IsInited() ) ? g_mouse->Status() : 0;

    while ( !done )
    {
        Event ev;

        if ( kbhit() )
        {
            int k = getch();

            if ( k == 27 ) { done = -1; break; }
            if ( k == 13 ) { done = 1;  break; }
            if ( k == 0 || k == 0xE0 )
            {
                ev.kind = Event::EV_KEY;
                ev.key  = 0x100 | getch();
                ev.x = ev.y = 0; ev.buttons = 0;
            }
            else
            {
                ev.kind = Event::EV_KEY;
                ev.key  = k;
                ev.x = ev.y = 0; ev.buttons = 0;
            }
            /* Offer it to each control in turn; the first one that wants it
             * takes it.  This is what App::Run does for the main screen. */
            if ( cbPath.HandleEvent( ev ) )  continue;
            if ( cbIcons.HandleEvent( ev ) ) continue;
            if ( rgMode.HandleEvent( ev ) )  continue;
            continue;
        }

        if ( g_mouse && g_mouse->IsInited() )
        {
            int b = g_mouse->Status();
            if ( ( b & LEFT_BUTTON ) && !( lastB & LEFT_BUTTON ) )
            {
                short mx = DfMouseCol( g_mouse->X() );
                short my = DfMouseRow( g_mouse->Y() );

                if ( btnOk.HitTest( mx, my ) )     { done = 1;  lastB = b; break; }
                if ( btnCancel.HitTest( mx, my ) ) { done = -1; lastB = b; break; }

                ev.kind = Event::EV_MOUSE;
                ev.key  = 0;
                ev.x = mx; ev.y = my; ev.buttons = b;
                if ( !cbPath.HandleEvent( ev ) )
                    if ( !cbIcons.HandleEvent( ev ) )
                        rgMode.HandleEvent( ev );
            }
            lastB = b;
        }
    }

    box.RestoreWindow();
    DfModalFinished();

    if ( done == 1 )
    {
        static const char *mode[] = { "Typical", "Compact", "Custom" };
        sprintf( line, "%s install, PATH %s, icons %s",
                 mode[ rgMode.Selected() ],
                 cbPath.Checked()  ? "yes" : "no",
                 cbIcons.Checked() ? "on"  : "off" );
    }
    else
        strcpy( line, "Options cancelled" );
    Status( line );
}

static void ActProgress( void *user )
{
    ProgressBox bar( FG_BLACK, BG_WHITE, "Working" );
    char        name[64];
    int         i;

    (void)user;
    bar.SetMouse( g_mouse );
    bar.SetCancelButton();
    bar.Open();
    for ( i = 0; i < 60; ++i )
    {
        sprintf( name, "FILE%04d.DAT", i + 1 );
        if ( !bar.Update( name, (unsigned long)( i + 1 ), 60UL ) ) break;
        if ( g_auto ) break;            /* headless: one frame is the point */

        /* A demo with nothing to do finishes before the box has been read.
         * Stand still for a moment per step so there is something to watch -
         * and so Cancel has a chance of being aimed at. */
        {
            clock_t until = clock() + (clock_t)( CLOCKS_PER_SEC / 20 );
            while ( clock() < until ) { /* the work a real caller would do */ }
        }
    }
    bar.Close();
    Status( bar.Cancelled() ? "Cancelled" : "Finished" );
}

static void ActWait( void *user )
{
    WaitBox wait( FG_BLACK, BG_WHITE, "Please Wait" );

    (void)user;
    wait.SetMouse( g_mouse );
    wait.Open( "Counting something that cannot say how far along it is..." );
    wait.Close();
    Status( "Waited" );
}

static void ActView( void *user )
{
    TextView tv( FG_BRIGHT_WHITE, BG_BLUE, "DFDEMO.CPP" );

    (void)user;
    tv.SetMouse( g_mouse );
    if ( tv.Load( "DFDEMO.CPP" ) < 0 )
    {
        /* Not next to the exe: show something rather than nothing, which is
         * also the LoadText path getting exercised. */
        tv.LoadText( "DFDEMO.CPP was not in the current directory.\n"
                     "\n"
                     "This is the same viewer over a string in memory - the\n"
                     "other way in, and the way an archive comment or a help\n"
                     "text reaches it.\n" );
    }
    tv.Show();
    Status( "Viewer closed" );
}

static void ActOpen( void *user )
{
    FileBox fb( FG_BLACK, BG_WHITE, "Open" );
    char    line[96];

    (void)user;
    fb.SetMouse( g_mouse );
    fb.SetMode( FileBox::FB_OPEN );
    fb.SetFilter( "*.CPP;*.HPP;*.TXT" );
    if ( fb.Show() ) sprintf( line, "Chose: %.60s", fb.Path() );
    else             strcpy( line, "Nothing chosen" );
    Status( line );
}

static void ActFolder( void *user )
{
    FileBox fb( FG_BLACK, BG_WHITE, "Choose a Folder" );
    char    line[96];

    (void)user;
    fb.SetMouse( g_mouse );
    fb.SetMode( FileBox::FB_DIR );
    if ( fb.Show() ) sprintf( line, "Folder: %.60s", fb.Path() );
    else             strcpy( line, "No folder chosen" );
    Status( line );
}

static void ActSave( void *user )
{
    FileBox fb( FG_BLACK, BG_WHITE, "Save As" );
    char    line[96];

    (void)user;
    fb.SetMouse( g_mouse );
    fb.SetMode( FileBox::FB_SAVE );
    fb.SetName( "UNTITLED.TXT" );
    if ( fb.Show() ) sprintf( line, "Save to: %.60s", fb.Path() );
    else             strcpy( line, "Save cancelled" );
    Status( line );
}

static void ActExpandAll( void *user )
{
    int i;

    (void)user;
    for ( i = 0; i < NODE_COUNT; ++i ) g_node[i].open = 1;
    if ( g_list ) { g_list->SetCount( RebuildVisible() ); g_list->Draw(); }
    Status( "Everything open" );
}

static void ActCollapseAll( void *user )
{
    int i;

    (void)user;
    for ( i = 0; i < NODE_COUNT; ++i ) g_node[i].open = 0;
    if ( g_list ) { g_list->SetCount( RebuildVisible() ); g_list->Draw(); }
    Status( "Everything shut" );
}

static void ActExit( void *user )
{
    (void)user;
    if ( g_app ) g_app->Quit();
}

/*---- main ----------------------------------------------------------------- */

int main( int argc, char **argv )
{
    short savedCursor;
    short fm, dg, wk, tr;
    int   i;

    printf( "\nDOSForms widget demo\n" );

    for ( i = 1; i < argc; ++i )
        if ( argv[i][0] == '/' || argv[i][0] == '-' )
            if ( argv[i][1] == 't' || argv[i][1] == 'T' ) g_auto = 1;

    /* Before the first mode set and before the mouse - see DOSFORMS.HPP. */
    DfVectorGuard vectors;

    _setvideomode( _TEXTC80 );
    savedCursor = _gettextcursor();
    _settextcursor( 0x2000 );

    {
        App        app( FG_WHITE, BG_BLUE );
        TitleBar   title( FG_BLACK, BG_CYAN, "DOSForms" );
        Menu       menu( FG_BLACK, BG_WHITE );
        ListBox    list( FG_LIGHT_CYAN, BG_BLUE );
        FieldPane  pane( FG_BLACK, BG_WHITE );
        SegmentBar bar( FG_BLACK, BG_WHITE );
        StatusBar  stat( FG_BLACK, BG_CYAN );
#ifdef USE_MOUSE
        MSMouse    mouse;
        g_mouse = &mouse;
#endif
        short rows = DfRows() ? DfRows() : 25;
        short cols = DfCols() ? DfCols() : 80;
        short split = (short)( cols - 34 );      /* left edge of the panes */

        g_app = &app; g_list = &list; g_pane = &pane; g_bar = &bar;
        g_stat = &stat;

        fm = menu.AddMenu( "File", 'F' );
        menu.AddItem( fm, "View this source",  'V', ActView,   0 );
        menu.AddItem( fm, "Open...",           'O', ActOpen,   0 );
        menu.AddItem( fm, "Save As...",        'S', ActSave,   0 );
        menu.AddItem( fm, "Choose Folder...",  'C', ActFolder, 0 );
        menu.AddItem( fm, "Exit",              'X', ActExit,   0 );
        tr = menu.AddMenu( "Tree", 'T' );
        menu.AddItem( tr, "Expand All",        'E', ActExpandAll,   0 );
        menu.AddItem( tr, "Collapse All",      'C', ActCollapseAll, 0 );
        dg = menu.AddMenu( "Dialogs", 'D' );
        menu.AddItem( dg, "Message",           'M', ActMessage,  0 );
        menu.AddItem( dg, "Question",          'Q', ActAsk,      0 );
        menu.AddItem( dg, "Four buttons",      'F', ActFourWay,  0 );
        menu.AddItem( dg, "Input",             'I', ActInput,    0 );
        menu.AddItem( dg, "Password",          'P', ActPassword, 0 );
        menu.AddItem( dg, "Options...",        'O', ActOptions,  0 );
        wk = menu.AddMenu( "Work", 'W' );
        menu.AddItem( wk, "Progress",          'P', ActProgress, 0 );
        menu.AddItem( wk, "Please Wait",       'W', ActWait,     0 );

        /* The list is a TREE: five callbacks and the width of the name
         * field, which is the part of every row the widget owns. */
        list.SetRegion( 2, 0, (short)( rows - 3 ), (short)( split - 2 ) );
        list.SetData( RebuildVisible(), TreeTail, 0 );
        list.SetTreeProvider( TreeDepth, TreeHasKids, TreeExpanded,
                              TreeToggle, TreeLabel, 26 );
        list.SetHeader( " Name                          Size" );

        pane.SetRegion( 2, split, 13, (short)( cols - 1 ) );
        pane.SetTitle( "About" );
        bar.SetRegion( 14, split, (short)( rows - 3 ), (short)( cols - 1 ) );
        bar.SetTitle( "Drive C:" );
        FillPanes();

        stat.Print( "Arrows move  +/- open and shut  Alt for the menus  Esc quits" );

        app.Add( &title );
        app.Add( &menu );
        app.Add( &list );
        app.Add( &pane );
        app.Add( &bar );
        app.Add( &stat );
        app.SetMouse( g_mouse );
        app.Draw();

        if ( g_auto )
        {
            /* Every modal in the toolkit asks the peek whether anyone is
             * there.  With one installed they paint and close, so this walks
             * the whole program without a keyboard. */
            DfSetModalPeek( AutoPeek );
            ActMessage( 0 );  ActAsk( 0 );      ActFourWay( 0 );
            ActInput( 0 );    ActPassword( 0 ); ActOptions( 0 );
            ActProgress( 0 ); ActWait( 0 );     ActView( 0 );
            ActOpen( 0 );     ActSave( 0 );     ActFolder( 0 );
            ActExpandAll( 0 );
            ActCollapseAll( 0 );
            DfSetModalPeek( 0 );
        }
        else
            app.Run();

        g_app = 0; g_list = 0; g_pane = 0; g_bar = 0; g_stat = 0; g_mouse = 0;
    }

    _settextcursor( savedCursor );
    _setvideomode( _DEFAULTMODE );
    /* The /T walk opens twelve boxes, but only TEN of them are modal: the
     * progress box and the wait box do not wait for input, so there is
     * nobody for them to ask and they have no peek to call.  Printing the
     * count turns "it did not crash" into "every dialog that waits painted
     * and came down again", which is what a headless run can prove. */
    if ( g_auto ) printf( "modals painted: %d of 10\n", g_peeks );
    printf( "DOSForms demo done.\n" );
    return 0;
}
