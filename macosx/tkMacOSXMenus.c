/*
 * tkMacOSXMenus.c --
 *
 *	These calls set up and manage the menubar for the
 *	Macintosh version of Tk.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 * Copyright 2001, Apple Computer, Inc.
 * Copyright (c) 2005-2007 Daniel A. Steffen <das@users.sourceforge.net>
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkMacOSXPrivate.h"

#define kAppleMenu		256
#define kAppleAboutItem		1
#define kFileMenu		2
#define kEditMenu		3

#define kSourceItem		1
#define kCloseItem		2

#define EDIT_CUT		1
#define EDIT_COPY		2
#define EDIT_PASTE		3
#define EDIT_CLEAR		4

MenuRef tkAppleMenu;
MenuRef tkFileMenu;
MenuRef tkEditMenu;

static Tcl_Interp * gInterp;	    /* Interpreter for this application. */

static void GenerateEditEvent(int flag);
static void SourceDialog(void);


/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXHandleMenuSelect --
 *
 *	Handles events that occur in the Menu bar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXHandleMenuSelect(
    MenuID theMenu,
    MenuItemIndex theItem,
    int optionKeyPressed)
{
    Tk_Window tkwin;
    Window window;
    TkDisplay *dispPtr;

    if (theItem == 0) {
	TkMacOSXClearMenubarActive();
	return;
    }

    switch (theMenu) {
	case kAppleMenu:
	    switch (theItem) {
		case kAppleAboutItem:
		    {
			Tcl_CmdInfo dummy;
			if (optionKeyPressed || gInterp == NULL ||
			    Tcl_GetCommandInfo(gInterp,
				"tkAboutDialog", &dummy) == 0) {
			    TkAboutDlg();
			} else {
			    Tcl_EvalEx(gInterp, "tkAboutDialog", -1,
				    TCL_EVAL_GLOBAL);
			}
			break;
		    }
	    }
	    break;
	case kFileMenu:
	    switch (theItem) {
		case kSourceItem:
		    /*
		     * TODO: source script
		     */

		    SourceDialog();
		    break;
		case kCloseItem:
		    /* Send close event */
		    window = TkMacOSXGetXWindow(ActiveNonFloatingWindow());
		    dispPtr = TkGetDisplayList();
		    tkwin = Tk_IdToWindow(dispPtr->display, window);
		    TkGenWMDestroyEvent(tkwin);
		    break;
	    }
	    break;
	case kEditMenu:
	    /*
	     * This implementation just send the keysyms Tk thinks are
	     * associated with function keys that do Cut, Copy & Paste on
	     * a Sun keyboard.
	     */
	    GenerateEditEvent(theItem);
	    break;
	default:
	    TkMacOSXDispatchMenuEvent(theMenu, theItem);
	    break;
    }
    /*
     * Finally we unhighlight the menu.
     */
    HiliteMenu(0);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitMenus --
 *
 *	This procedure initializes the Macintosh menu bar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkMacOSXInitMenus(
    Tcl_Interp *interp)
{
    OSStatus err;
    gInterp = interp;

    if (TkMacOSXUseMenuID(kAppleMenu) != TCL_OK) {
	Tcl_Panic("Menu ID %d is already in use!", kAppleMenu);
    }
    err = ChkErr(CreateNewMenu, kAppleMenu, kMenuAttrDoNotUseUserCommandKeys,
	    &tkAppleMenu);
    if (err != noErr) {
	Tcl_Panic("CreateNewMenu failed !");
    }
    SetMenuTitle(tkAppleMenu, "\p\024");
    InsertMenu(tkAppleMenu, 0);
    AppendMenu(tkAppleMenu, "\pAbout Tcl & Tk\xc9");
    AppendMenu(tkAppleMenu, "\p(-");

    if (TkMacOSXUseMenuID(kFileMenu) != TCL_OK) {
	Tcl_Panic("Menu ID %d is already in use!", kFileMenu);
    }
    err = ChkErr(CreateNewMenu, kFileMenu, kMenuAttrDoNotUseUserCommandKeys,
	    &tkFileMenu);
    if (err != noErr) {
	Tcl_Panic("CreateNewMenu failed !");
    }
    SetMenuTitle(tkFileMenu, "\pFile");
    InsertMenu(tkFileMenu, 0);
    AppendMenu(tkFileMenu, "\pSource\xc9");
    AppendMenu(tkFileMenu, "\pClose/W");

    if (TkMacOSXUseMenuID(kEditMenu) != TCL_OK) {
	Tcl_Panic("Menu ID %d is already in use!", kEditMenu);
    }
    err = ChkErr(CreateNewMenu, kEditMenu, kMenuAttrDoNotUseUserCommandKeys,
	    &tkEditMenu);
    if (err != noErr) {
	Tcl_Panic("CreateNewMenu failed !");
    }
    SetMenuTitle(tkEditMenu, "\pEdit");
    InsertMenu(tkEditMenu, 0);
    AppendMenu(tkEditMenu, "\pCut/X");
    AppendMenu(tkEditMenu, "\pCopy/C");
    AppendMenu(tkEditMenu, "\pPaste/V");
    AppendMenu(tkEditMenu, "\pClear");
    if (TkMacOSXUseMenuID(kHMHelpMenuID) != TCL_OK) {
	Tcl_Panic("Help menu ID %s is already in use!", kHMHelpMenuID);
    }

    /*
     * Workaround a Carbon bug with kHICommandPreferences: the first call to
     * IsMenuKeyEvent returns false for the preferences menu item key shorcut
     * event (even if the corresponding menu item is dynamically enabled by a
     * kEventCommandUpdateStatus handler), unless the kHICommandPreferences
     * menu item has previously been enabled manually. [Bug 1481503]
     */
    EnableMenuCommand(NULL, kHICommandPreferences);

    DrawMenuBar();
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateEditEvent --
 *
 *	Takes an edit menu item and posts the corasponding a virtual
 *	event to Tk's event queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May place events of queue.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateEditEvent(
    int flag)
{
    XVirtualEvent event;
    int x, y;
    Tk_Window tkwin;
    Window window;
    TkDisplay *dispPtr;

    window = TkMacOSXGetXWindow(ActiveNonFloatingWindow());
    dispPtr = TkGetDisplayList();
    tkwin = Tk_IdToWindow(dispPtr->display, window);
    tkwin = (Tk_Window) ((TkWindow *) tkwin)->dispPtr->focusPtr;
    if (tkwin == NULL) {
	return;
    }

    bzero(&event, sizeof(XVirtualEvent));
    event.type = VirtualEvent;
    event.serial = Tk_Display(tkwin)->request;
    event.send_event = false;
    event.display = Tk_Display(tkwin);
    event.event = Tk_WindowId(tkwin);
    event.root = XRootWindow(Tk_Display(tkwin), 0);
    event.subwindow = None;
    event.time = TkpGetMS();

    XQueryPointer(NULL, None, NULL, NULL,
	    &event.x_root, &event.y_root, &x, &y, &event.state);
    tkwin = Tk_TopCoordsToWindow(tkwin, x, y, &event.x, &event.y);
    event.same_screen = true;

    switch (flag) {
	case EDIT_CUT:
	    event.name = Tk_GetUid("Cut");
	    break;
	case EDIT_COPY:
	    event.name = Tk_GetUid("Copy");
	    break;
	case EDIT_PASTE:
	    event.name = Tk_GetUid("Paste");
	    break;
	case EDIT_CLEAR:
	    event.name = Tk_GetUid("Clear");
	    break;
    }
    Tk_QueueWindowEvent((XEvent *) &event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * SourceDialog --
 *
 *	Presents a dialog to the user for selecting a Tcl file. The
 *	selected file will be sourced into the main interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SourceDialog(void)
{
    int result;
    const char *path;
    const char *openCmd = "tk_getOpenFile -filetypes {\
	    {{TCL Scripts} {.tcl} TEXT} {{Text Files} {} TEXT}}";

    if (gInterp == NULL) {
	return;
    }
    if (Tcl_EvalEx(gInterp, openCmd, -1, TCL_EVAL_GLOBAL) != TCL_OK) {
	return;
    }
    path = Tcl_GetStringResult(gInterp);
    if (strlen(path) == 0) {
	return;
    }
    result = Tcl_EvalFile(gInterp, path);
    if (result == TCL_ERROR) {
	Tcl_BackgroundError(gInterp);
    }
}
