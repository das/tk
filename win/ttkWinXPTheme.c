/*
 * $Id$
 *
 * Tk theme engine which uses the Windows XP "Visual Styles" API
 * Adapted from Georgios Petasis' XP theme patch.
 *
 * Copyright (c) 2003 by Georgios Petasis, petasis@iit.demokritos.gr.
 * Copyright (c) 2003 by Joe English
 * Copyright (c) 2003 by Pat Thoyts
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * See also:
 *
 * <URL: http://msdn.microsoft.com/library/en-us/
 *  	shellcc/platform/commctls/userex/refentry.asp >
 */

#ifndef HAVE_UXTHEME_H
/* Stub for platforms that lack the XP theme API headers: */
#include <windows.h>
#include <tcl.h>
int TtkXPTheme_Init(Tcl_Interp *interp, HWND hwnd) { return TCL_OK; }
#else

#define WINVER 0x0501	/* Requires Windows XP APIs */

#include <windows.h>
#include <uxtheme.h>
#include <tmschema.h>

#include <tkWinInt.h>

#include "ttk/ttkTheme.h"

typedef HTHEME  (STDAPICALLTYPE OpenThemeDataProc)(HWND hwnd,
		 LPCWSTR pszClassList);
typedef HRESULT (STDAPICALLTYPE CloseThemeDataProc)(HTHEME hTheme);
typedef HRESULT (STDAPICALLTYPE DrawThemeBackgroundProc)(HTHEME hTheme,
                 HDC hdc, int iPartId, int iStateId, const RECT *pRect,
                 OPTIONAL const RECT *pClipRect);
typedef HRESULT	(STDAPICALLTYPE GetThemePartSizeProc)(HTHEME,HDC,
		 int iPartId, int iStateId,
		 RECT *prc, enum THEMESIZE eSize, SIZE *psz);
/* GetThemeTextExtent and DrawThemeText only used with BROKEN_TEXT_ELEMENT */ 
typedef HRESULT (STDAPICALLTYPE GetThemeTextExtentProc)(HTHEME hTheme, HDC hdc,
		 int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
		 DWORD dwTextFlags, const RECT *pBoundingRect, RECT *pExtent);
typedef HRESULT (STDAPICALLTYPE DrawThemeTextProc)(HTHEME hTheme, HDC hdc,
		 int iPartId, int iStateId, LPCWSTR pszText, int iCharCount,
		 DWORD dwTextFlags, DWORD dwTextFlags2, const RECT *pRect);
typedef BOOL    (STDAPICALLTYPE IsThemeActiveProc)(VOID);
typedef BOOL    (STDAPICALLTYPE IsAppThemedProc)(VOID);

typedef struct
{
    OpenThemeDataProc			*OpenThemeData;
    CloseThemeDataProc			*CloseThemeData;
    GetThemePartSizeProc		*GetThemePartSize;
    DrawThemeBackgroundProc		*DrawThemeBackground;
    DrawThemeTextProc		        *DrawThemeText;
    GetThemeTextExtentProc		*GetThemeTextExtent;
    IsThemeActiveProc			*IsThemeActive;
    IsAppThemedProc			*IsAppThemed;

    HWND                                stubWindow;
} XPThemeProcs;

typedef struct
{
    HINSTANCE hlibrary;
    XPThemeProcs *procs;
} XPThemeData;

/*
 *----------------------------------------------------------------------
 *
 * LoadXPThemeProcs --
 *	Initialize XP theming support.
 *
 *	XP theme support is included in UXTHEME.DLL
 *	We dynamically load this DLL at runtime instead of linking
 *	to it at build-time.
 *
 * Returns:
 *	A pointer to an XPThemeProcs table if successful, NULL otherwise.
 */

static XPThemeProcs *
LoadXPThemeProcs(HINSTANCE *phlib)
{
    /*
     * Load the library "uxtheme.dll", where the native widget
     * drawing routines are implemented.  This will only succeed 
     * if we are running at least on Windows XP.
     */
    HINSTANCE handle;
    *phlib = handle = LoadLibrary("uxtheme.dll");
    if (handle != 0)
    {
	/*
	 * We have successfully loaded the library. Proceed in storing the
	 * addresses of the functions we want to use.
	 */
	XPThemeProcs *procs = (XPThemeProcs*)ckalloc(sizeof(XPThemeProcs));
#define LOADPROC(name) \
	(0 != (procs->name = (name ## Proc *)GetProcAddress(handle, #name) ))

	if (   LOADPROC(OpenThemeData)
	    && LOADPROC(CloseThemeData)
	    && LOADPROC(GetThemePartSize)
	    && LOADPROC(DrawThemeBackground)
	    && LOADPROC(GetThemeTextExtent)
	    && LOADPROC(DrawThemeText)
	    && LOADPROC(IsThemeActive)
	    && LOADPROC(IsAppThemed)
	)
	{
	    return procs;
	}
#undef LOADPROC
	ckfree((char*)procs);
    }
    return 0;
}

/*
 * XPThemeDeleteProc --
 *
 *      Release any theme allocated resources.
 */

static void
XPThemeDeleteProc(void *clientData)
{
    XPThemeData *themeData = clientData;
    FreeLibrary(themeData->hlibrary);
    ckfree(clientData);
}

static int
XPThemeEnabled(Ttk_Theme theme, void *clientData)
{
    XPThemeData *themeData = clientData;
    int active = themeData->procs->IsThemeActive();
    int themed = themeData->procs->IsAppThemed();
    return (active && themed);
}

/*
 * BoxToRect --
 * 	Helper routine.  Returns a RECT data structure.
 */
static RECT
BoxToRect(Ttk_Box b)
{
    RECT rc;
    rc.top = b.y;
    rc.left = b.x;
    rc.bottom = b.y + b.height;
    rc.right = b.x + b.width;
    return rc;
}

/*
 * Map Tk state bitmaps to XP style enumerated values.
 */
static Ttk_StateTable null_statemap[] = { {0,0,0} };

/*
 * Pushbuttons (Tk: "Button")
 */
static Ttk_StateTable pushbutton_statemap[] =
{
    { PBS_DISABLED, 	TTK_STATE_DISABLED, 0 },
    { PBS_PRESSED, 	TTK_STATE_PRESSED, 0 },
    { PBS_HOT,		TTK_STATE_ACTIVE, 0 },
    { PBS_DEFAULTED,	TTK_STATE_ALTERNATE, 0 },
    { PBS_NORMAL, 	0, 0 }
};

/*
 * Checkboxes (Tk: "Checkbutton")
 */
static Ttk_StateTable checkbox_statemap[] =
{
{CBS_MIXEDDISABLED, 	TTK_STATE_ALTERNATE|TTK_STATE_DISABLED, 0},
{CBS_MIXEDPRESSED, 	TTK_STATE_ALTERNATE|TTK_STATE_PRESSED, 0},
{CBS_MIXEDHOT,  	TTK_STATE_ALTERNATE|TTK_STATE_ACTIVE, 0},
{CBS_MIXEDNORMAL, 	TTK_STATE_ALTERNATE, 0},
{CBS_CHECKEDDISABLED,	TTK_STATE_SELECTED|TTK_STATE_DISABLED, 0},
{CBS_CHECKEDPRESSED,	TTK_STATE_SELECTED|TTK_STATE_PRESSED, 0},
{CBS_CHECKEDHOT,	TTK_STATE_SELECTED|TTK_STATE_ACTIVE, 0},
{CBS_CHECKEDNORMAL,	TTK_STATE_SELECTED, 0},
{CBS_UNCHECKEDDISABLED,	TTK_STATE_DISABLED, 0},
{CBS_UNCHECKEDPRESSED,	TTK_STATE_PRESSED, 0},
{CBS_UNCHECKEDHOT,	TTK_STATE_ACTIVE, 0},
{CBS_UNCHECKEDNORMAL,	0,0 }
};

/*
 * Radiobuttons:
 */
static Ttk_StateTable radiobutton_statemap[] =
{
{RBS_CHECKEDDISABLED,	TTK_STATE_SELECTED|TTK_STATE_DISABLED, 0},
{RBS_CHECKEDPRESSED,	TTK_STATE_SELECTED|TTK_STATE_PRESSED, 0},
{RBS_CHECKEDHOT,	TTK_STATE_SELECTED|TTK_STATE_ACTIVE, 0},
{RBS_CHECKEDNORMAL,	TTK_STATE_SELECTED, 0},
{RBS_UNCHECKEDDISABLED,	TTK_STATE_DISABLED, 0},
{RBS_UNCHECKEDPRESSED,	TTK_STATE_PRESSED, 0},
{RBS_UNCHECKEDHOT,	TTK_STATE_ACTIVE, 0},
{RBS_UNCHECKEDNORMAL,	0,0 }
};

/*
 * Groupboxes (tk: "frame")
 */
static Ttk_StateTable groupbox_statemap[] =
{
{GBS_DISABLED,	TTK_STATE_DISABLED, 0},
{GBS_NORMAL,	0,0 }
};

/*
 * Edit fields (tk: "entry")
 */
static Ttk_StateTable edittext_statemap[] = 
{
    { ETS_DISABLED,	TTK_STATE_DISABLED, 0 },
    { ETS_READONLY,	TTK_STATE_READONLY, 0 },
    { ETS_FOCUSED,	TTK_STATE_FOCUS, 0 },
    { ETS_HOT,		TTK_STATE_ACTIVE, 0 },
    { ETS_NORMAL,	0, 0 }
/* NOT USED: ETS_ASSIST, ETS_SELECTED */
};

/*
 * Combobox text field statemap: 
 * Same as edittext_statemap, but doesn't use ETS_READONLY 
 * (fixes: #1032409)
 */
static Ttk_StateTable combotext_statemap[] = 
{
    { ETS_DISABLED,	TTK_STATE_DISABLED, 0 },
    { ETS_FOCUSED,	TTK_STATE_FOCUS, 0 },
    { ETS_HOT,		TTK_STATE_ACTIVE, 0 },
    { ETS_NORMAL,	0, 0 }
};

/*
 * Combobox button: (CBP_DROPDOWNBUTTON)
 */
static Ttk_StateTable combobox_statemap[] = {
    { CBXS_DISABLED,	TTK_STATE_DISABLED, 0 },
    { CBXS_PRESSED, 	TTK_STATE_PRESSED, 0 },
    { CBXS_HOT, 	TTK_STATE_ACTIVE, 0 },
    { CBXS_NORMAL, 	0, 0 }
};

/*
 * Toolbar buttons (TP_BUTTON):
 */
static Ttk_StateTable toolbutton_statemap[] =  {
    { TS_DISABLED, 	TTK_STATE_DISABLED, 0 },
    { TS_PRESSED,	TTK_STATE_PRESSED, 0 },
    { TS_HOTCHECKED,	TTK_STATE_SELECTED|TTK_STATE_ACTIVE, 0 },
    { TS_CHECKED, 	TTK_STATE_SELECTED, 0 },
    { TS_HOT,  		TTK_STATE_ACTIVE, 0 },
    { TS_NORMAL, 	0,0 }
};

/*
 * Scrollbars (Tk: "Scrollbar.thumb")
 */
static Ttk_StateTable scrollbar_statemap[] =
{
    { SCRBS_DISABLED, 	TTK_STATE_DISABLED, 0 },
    { SCRBS_PRESSED, 	TTK_STATE_PRESSED, 0 },
    { SCRBS_HOT,	TTK_STATE_ACTIVE, 0 },
    { SCRBS_NORMAL, 	0, 0 }
};

static Ttk_StateTable uparrow_statemap[] =
{
    { ABS_UPDISABLED,	TTK_STATE_DISABLED, 0 },
    { ABS_UPPRESSED, 	TTK_STATE_PRESSED, 0 },
    { ABS_UPHOT,	TTK_STATE_ACTIVE, 0 },
    { ABS_UPNORMAL, 	0, 0 }
};

static Ttk_StateTable downarrow_statemap[] =
{
    { ABS_DOWNDISABLED,	TTK_STATE_DISABLED, 0 },
    { ABS_DOWNPRESSED, 	TTK_STATE_PRESSED, 0 },
    { ABS_DOWNHOT,	TTK_STATE_ACTIVE, 0 },
    { ABS_DOWNNORMAL, 	0, 0 }
};

static Ttk_StateTable leftarrow_statemap[] =
{
    { ABS_LEFTDISABLED,	TTK_STATE_DISABLED, 0 },
    { ABS_LEFTPRESSED, 	TTK_STATE_PRESSED, 0 },
    { ABS_LEFTHOT,	TTK_STATE_ACTIVE, 0 },
    { ABS_LEFTNORMAL, 	0, 0 }
};

static Ttk_StateTable rightarrow_statemap[] =
{
    { ABS_RIGHTDISABLED,TTK_STATE_DISABLED, 0 },
    { ABS_RIGHTPRESSED, TTK_STATE_PRESSED, 0 },
    { ABS_RIGHTHOT,	TTK_STATE_ACTIVE, 0 },
    { ABS_RIGHTNORMAL, 	0, 0 }
};

/*
 * Trackbar thumb: (Tk: "scale slider")
 */
static Ttk_StateTable scale_statemap[] =
{
    { TUS_DISABLED, 	TTK_STATE_DISABLED, 0 },
    { TUS_PRESSED, 	TTK_STATE_PRESSED, 0 },
    { TUS_FOCUSED, 	TTK_STATE_FOCUS, 0 },
    { TUS_HOT,		TTK_STATE_ACTIVE, 0 },
    { TUS_NORMAL, 	0, 0 }
};

static Ttk_StateTable tabitem_statemap[] =
{
    { TIS_DISABLED,     TTK_STATE_DISABLED, 0 },
    { TIS_SELECTED,     TTK_STATE_SELECTED, 0 },
    { TIS_HOT,          TTK_STATE_ACTIVE,   0 },
    { TIS_FOCUSED,      TTK_STATE_FOCUS,    0 },
    { TIS_NORMAL,       0,                  0 },
};


/*
 *----------------------------------------------------------------------
 * +++ Element data:
 *
 * The following structure is passed as the 'clientData' pointer
 * to most elements in this theme.  It contains data relevant
 * to a single XP Theme "part".
 *
 * <<NOTE-GetThemeMargins>>: 
 *	In theory, we should be call GetThemeMargins(...TMT_CONTENTRECT...)
 *	to calculate the internal padding.  In practice, this routine
 *	only seems to work properly for BP_PUSHBUTTON.  So we hardcode
 *	the required padding at element registration time instead.
 *
 *	The PAD_MARGINS flag bit determines whether the padding
 *	should be added on the inside (0) or outside (1) of the element.
 *
 * <<NOTE-GetThemePartSize>>: 
 *	This gives bogus metrics for some parts (in particular,
 *	BP_PUSHBUTTONS).  Set the IGNORE_THEMESIZE flag to skip this call.
 */

typedef struct 	/* XP element specifications */
{
    const char	*elementName;	/* Tk theme engine element name */
    Ttk_ElementSpec *elementSpec;	
    				/* Element spec (usually GenericElementSpec) */
    LPCWSTR	className;	/* Windows window class name */
    int 	partId;		/* BP_PUSHBUTTON, BP_CHECKBUTTON, etc. */
    Ttk_StateTable *statemap;	/* Map Tk states to XP states */
    Ttk_Padding	padding;	/* See NOTE-GetThemeMargins */
    int  	flags;		
#   define 	IGNORE_THEMESIZE 	0x1 /* See NOTE-GetThemePartSize */
#   define 	PAD_MARGINS		0x2 /* See NOTE-GetThemeMargins */
} ElementInfo;

typedef struct
{
    /*
     * Static data, initialized when element is registered:
     */
    ElementInfo	*info;
    XPThemeProcs *procs;	/* Pointer to theme procedure table */

    /*
     * Dynamic data, allocated by InitElementData:
     */
    HTHEME	hTheme;
    HDC		hDC;
    HWND	hwnd;

    /* For TkWinDrawableReleaseDC: */
    Drawable	drawable;
    TkWinDCState dcState;
} ElementData;

static ElementData *
NewElementData(XPThemeProcs *procs, ElementInfo *info)
{
    ElementData *elementData = (ElementData*)ckalloc(sizeof(ElementData));

    elementData->procs = procs;
    elementData->info = info;
    elementData->hTheme = elementData->hDC = 0;

    return elementData;
}

static void DestroyElementData(void *elementData)
{
    ckfree(elementData);
}

/*
 * InitElementData --
 * 	Looks up theme handle.  If Drawable argument is non-NULL,
 * 	also initializes DC.
 *
 * Returns:
 * 	1 on success, 0 on error.
 * 	Caller must later call FreeElementData() so this element
 * 	can be reused.
 */

static int
InitElementData(ElementData *elementData, Tk_Window tkwin, Drawable d)
{
    Window win = Tk_WindowId(tkwin);

    if (win != None) {
	elementData->hwnd = Tk_GetHWND(win);
    } else  {
	elementData->hwnd = elementData->procs->stubWindow;
    }

    elementData->hTheme = elementData->procs->OpenThemeData(
	elementData->hwnd, elementData->info->className);

    if (!elementData->hTheme)
	return 0;

    elementData->drawable = d;
    if (d != 0) {
	elementData->hDC = TkWinGetDrawableDC(Tk_Display(tkwin), d,
	    &elementData->dcState);
    }

    return 1;
}

static void
FreeElementData(ElementData *elementData)
{
    elementData->procs->CloseThemeData(elementData->hTheme);
    if (elementData->drawable != 0) {
	TkWinReleaseDrawableDC(
	    elementData->drawable, elementData->hDC, &elementData->dcState);
    }
}

/*----------------------------------------------------------------------
 * +++ Generic element implementation.
 * 
 * Used for elements which are handled entirely by the XP Theme API,
 * such as radiobutton and checkbutton indicators, scrollbar arrows, etc.
 */

static void GenericElementSize(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    int *widthPtr, int *heightPtr, Ttk_Padding *paddingPtr)
{
    ElementData *elementData = clientData;
    HRESULT result;
    SIZE size;

    if (!InitElementData(elementData, tkwin, 0))
	return;

    if (!(elementData->info->flags & IGNORE_THEMESIZE)) {
	result = elementData->procs->GetThemePartSize(
	    elementData->hTheme,
	    elementData->hDC,
	    elementData->info->partId,
	    Ttk_StateTableLookup(elementData->info->statemap, 0),
	    NULL /*RECT *prc*/,
	    TS_TRUE,
	    &size);

	if (SUCCEEDED(result)) {
	    *widthPtr = size.cx;
	    *heightPtr = size.cy;
	} 
    }

    /* See NOTE-GetThemeMargins 
     */
    *paddingPtr = elementData->info->padding;
}

static void GenericElementDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, unsigned int state)
{
    ElementData *elementData = clientData;
    RECT rc;

    if (!InitElementData(elementData, tkwin, d)) {
	return;
    }

    if (elementData->info->flags & PAD_MARGINS) {
    	b = Ttk_PadBox(b, elementData->info->padding);
    }
    rc = BoxToRect(b);

    elementData->procs->DrawThemeBackground(
	elementData->hTheme,
	elementData->hDC,
	elementData->info->partId,
	Ttk_StateTableLookup(elementData->info->statemap, state),
	&rc,
	NULL/*pContentRect*/);

    FreeElementData(elementData);
}

static Ttk_ElementSpec GenericElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(NullElement),
    TtkNullElementOptions,
    GenericElementSize,
    GenericElementDraw
};

/*----------------------------------------------------------------------
 * +++ Scrollbar thumb element.
 *     Same as a GenericElement, but don't draw in the disabled state.
 */

static void ThumbElementDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, unsigned int state)
{
    ElementData *elementData = clientData;
    unsigned stateId = Ttk_StateTableLookup(elementData->info->statemap, state);
    RECT rc = BoxToRect(b);

    /*
     * Don't draw the thumb if we are disabled.
     */
    if (state & TTK_STATE_DISABLED)
	return;

    if (!InitElementData(elementData, tkwin, d))
	return;

    elementData->procs->DrawThemeBackground(elementData->hTheme,
	elementData->hDC, elementData->info->partId, stateId,
	&rc, NULL);

    FreeElementData(elementData);
}

static Ttk_ElementSpec ThumbElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(NullElement),
    TtkNullElementOptions,
    GenericElementSize,
    ThumbElementDraw
};

/*----------------------------------------------------------------------
 * +++ Progress bar element.
 *	Increases the requested length of PP_CHUNK and PP_CHUNKVERT parts
 *	so that indeterminate progress bars show 3 bars instead of 1.   
 */

static void PbarElementSize(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    int *widthPtr, int *heightPtr, Ttk_Padding *paddingPtr)
{
    ElementData *elementData = clientData;
    int nBars = 3;

    GenericElementSize(clientData, elementRecord, tkwin,
    	widthPtr, heightPtr, paddingPtr);

    if (elementData->info->partId == PP_CHUNK) {
    	*widthPtr *= nBars;
    } else if (elementData->info->partId == PP_CHUNKVERT) {
    	*heightPtr *= nBars;
    }
}

static Ttk_ElementSpec PbarElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(NullElement),
    TtkNullElementOptions,
    PbarElementSize,
    GenericElementDraw
};

/*----------------------------------------------------------------------
 * +++  Notebook tab element.
 *	Same as generic element, with additional logic to select
 *	proper iPartID for the leftmost tab.
 * 	
 *	Notes: TABP_TABITEMRIGHTEDGE (or TABP_TOPTABITEMRIGHTEDGE, 
 * 	which appears to be identical) should be used if the
 *	tab is exactly at the right edge of the notebook, but
 *	not if it's simply the rightmost tab.  This information
 * 	is not available.
 *
 *	The TIS_* and TILES_* definitions are identical, so 
 * 	we can use the same statemap no matter what the partId.
 */
static void TabElementDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, unsigned int state)
{
    ElementData *elementData = clientData;
    int partId = elementData->info->partId;
    RECT rc = BoxToRect(b);

    if (!InitElementData(elementData, tkwin, d))
	return;
    if (state & TTK_STATE_USER1)
	partId = TABP_TABITEMLEFTEDGE;
    elementData->procs->DrawThemeBackground(
	elementData->hTheme, elementData->hDC, partId,
	Ttk_StateTableLookup(elementData->info->statemap, state), &rc, NULL);
    FreeElementData(elementData);
}

static Ttk_ElementSpec TabElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(NullElement),
    TtkNullElementOptions,
    GenericElementSize,
    TabElementDraw
};

/*----------------------------------------------------------------------
 * +++  Tree indicator element.
 *
 *	Generic element, but don't display at all if TTK_STATE_LEAF (=USER2) set
 */

#define TTK_STATE_OPEN TTK_STATE_USER1
#define TTK_STATE_LEAF TTK_STATE_USER2

static Ttk_StateTable header_statemap[] = 
{
    { HIS_PRESSED, 	TTK_STATE_PRESSED, 0 },
    { HIS_HOT,  	TTK_STATE_ACTIVE, 0 },
    { HIS_NORMAL, 	0,0 },
};

static Ttk_StateTable tvpglyph_statemap[] = 
{
    { GLPS_OPENED, 	TTK_STATE_OPEN, 0 },
    { GLPS_CLOSED, 	0,0 },
};

static void TreeIndicatorElementDraw(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, unsigned int state)
{
    if (!(state & TTK_STATE_LEAF)) {
        GenericElementDraw(clientData,elementRecord,tkwin,d,b,state);
    }
}

static Ttk_ElementSpec TreeIndicatorElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(NullElement),
    TtkNullElementOptions,
    GenericElementSize,
    TreeIndicatorElementDraw
};

#if BROKEN_TEXT_ELEMENT

/*
 *----------------------------------------------------------------------
 * Text element (does not work yet).
 *
 * According to "Using Windows XP Visual Styles",  we need to select 
 * a font into the DC before calling DrawThemeText().
 * There's just no easy way to get an HFONT out of a Tk_Font.
 * Maybe GetThemeFont() would work?
 * 
 */

typedef struct
{
    Tcl_Obj *textObj;
    Tcl_Obj *fontObj;
} TextElement;

static Ttk_ElementOptionSpec TextElementOptions[] =
{
    { "-text", TK_OPTION_STRING,
	Tk_Offset(TextElement,textObj), "" },
    { "-font", TK_OPTION_FONT,
	Tk_Offset(TextElement,fontObj), DEFAULT_FONT },
    { NULL }
};

static void TextElementSize(
    void *clientData, void *elementRecord, Tk_Window tkwin,
    int *widthPtr, int *heightPtr, Ttk_Padding *paddingPtr)
{
    TextElement *element = elementRecord;
    ElementData *elementData = clientData;
    RECT rc = {0, 0};
    HRESULT hr = S_OK;

    if (!InitElementData(elementData, tkwin, 0))
	return;

    hr = elementData->procs->GetThemeTextExtent(
	    elementData->hTheme,
	    elementData->hDC,
	    elementData->info->partId,
	    Ttk_StateTableLookup(elementData->info->statemap, 0),
	    Tcl_GetUnicode(element->textObj),
	    -1,
	    DT_LEFT,// | DT_BOTTOM | DT_NOPREFIX,
	    NULL,
	    &rc);

    if (SUCCEEDED(hr)) {
	*widthPtr = rc.right - rc.left;
	*heightPtr = rc.bottom - rc.top;
    }
    if (*widthPtr < 80) *widthPtr = 80;
    if (*heightPtr < 20) *heightPtr = 20;

    FreeElementData(elementData);
}

static void TextElementDraw(
    ClientData clientData, void *elementRecord, Tk_Window tkwin,
    Drawable d, Ttk_Box b, unsigned int state)
{
    TextElement *element = elementRecord;
    ElementData *elementData = clientData;
    RECT rc = BoxToRect(b);
    HRESULT hr = S_OK;

    if (!InitElementData(elementData, tkwin, d))
	return;

    hr = elementData->procs->DrawThemeText(
	    elementData->hTheme,
	    elementData->hDC,
	    elementData->info->partId,
	    Ttk_StateTableLookup(elementData->info->statemap, state),
	    Tcl_GetUnicode(element->textObj),
	    -1,
	    DT_LEFT,// | DT_BOTTOM | DT_NOPREFIX,
	    (state & TTK_STATE_DISABLED) ? DTT_GRAYED : 0,
	    &rc);
    FreeElementData(elementData);
}

static Ttk_ElementSpec TextElementSpec =
{
    TK_STYLE_VERSION_2,
    sizeof(TextElement),
    TextElementOptions,
    TextElementSize,
    TextElementDraw
};

#endif	/* BROKEN_TEXT_ELEMENT */

/*----------------------------------------------------------------------
 * +++ Widget layouts:
 */

TTK_BEGIN_LAYOUT(ButtonLayout)
    TTK_GROUP("Button.button", TTK_FILL_BOTH,
	TTK_GROUP("Button.focus", TTK_FILL_BOTH, 
	    TTK_GROUP("Button.padding", TTK_FILL_BOTH,
		TTK_NODE("Button.label", TTK_FILL_BOTH))))
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(MenubuttonLayout)
    TTK_NODE("Menubutton.dropdown", TTK_PACK_RIGHT|TTK_FILL_Y)
    TTK_GROUP("Menubutton.button", TTK_PACK_RIGHT|TTK_EXPAND|TTK_FILL_BOTH,
	    TTK_GROUP("Menubutton.padding", TTK_PACK_LEFT|TTK_EXPAND|TTK_FILL_X,
	        TTK_NODE("Menubutton.label", 0)))
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(HorizontalScrollbarLayout)
    TTK_GROUP("Horizontal.Scrollbar.trough", TTK_FILL_X,
	TTK_NODE("Horizontal.Scrollbar.leftarrow", TTK_PACK_LEFT)
	TTK_NODE("Horizontal.Scrollbar.rightarrow", TTK_PACK_RIGHT)
	TTK_GROUP("Horizontal.Scrollbar.thumb", TTK_FILL_BOTH|TTK_UNIT,
	    TTK_NODE("Horizontal.Scrollbar.grip", 0)))
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(VerticalScrollbarLayout)
    TTK_GROUP("Vertical.Scrollbar.trough", TTK_FILL_Y,
	TTK_NODE("Vertical.Scrollbar.uparrow", TTK_PACK_TOP)
	TTK_NODE("Vertical.Scrollbar.downarrow", TTK_PACK_BOTTOM)
	TTK_GROUP("Vertical.Scrollbar.thumb", TTK_FILL_BOTH|TTK_UNIT,
	    TTK_NODE("Vertical.Scrollbar.grip", 0)))
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(VerticalScaleLayout)
    TTK_GROUP("Scale.focus", TTK_EXPAND|TTK_FILL_BOTH,
	TTK_GROUP("Vertical.Scale.trough", TTK_EXPAND|TTK_FILL_BOTH,
	    TTK_NODE("Vertical.Scale.track", TTK_FILL_Y)
	    TTK_NODE("Vertical.Scale.slider", TTK_PACK_TOP) ))
TTK_END_LAYOUT

TTK_BEGIN_LAYOUT(HorizontalScaleLayout)
    TTK_GROUP("Scale.focus", TTK_EXPAND|TTK_FILL_BOTH,
	TTK_GROUP("Horizontal.Scale.trough", TTK_EXPAND|TTK_FILL_BOTH,
	    TTK_NODE("Horizontal.Scale.track", TTK_FILL_X) 
	    TTK_NODE("Horizontal.Scale.slider", TTK_PACK_LEFT) ))
TTK_END_LAYOUT

/*----------------------------------------------------------------------
 * +++ XP element info table: 
 */

#define PAD(l,t,r,b) {l,t,r,b}
#define NOPAD {0,0,0,0}

/* name spec className partId statemap padding flags */

static ElementInfo ElementInfoTable[] = {
    { "Checkbutton.indicator", &GenericElementSpec, L"BUTTON",
    	BP_CHECKBOX, checkbox_statemap, PAD(0, 0, 4, 0), PAD_MARGINS },
    { "Radiobutton.indicator", &GenericElementSpec, L"BUTTON",
    	BP_RADIOBUTTON, radiobutton_statemap, PAD(0, 0, 4, 0), PAD_MARGINS },
    { "Button.button", &GenericElementSpec, L"BUTTON",
    	BP_PUSHBUTTON, pushbutton_statemap, PAD(3, 3, 3, 3), IGNORE_THEMESIZE },
    { "Labelframe.border", &GenericElementSpec, L"BUTTON",
    	BP_GROUPBOX, groupbox_statemap, PAD(2, 2, 2, 2), 0 },
    { "Entry.field", &GenericElementSpec, L"EDIT", EP_EDITTEXT,
    	edittext_statemap, PAD(1, 1, 1, 1), 0 },
    { "Combobox.field", &GenericElementSpec, L"EDIT",
	EP_EDITTEXT, combotext_statemap, PAD(1, 1, 1, 1), 0 },
    { "Combobox.downarrow", &GenericElementSpec, L"COMBOBOX",
	CP_DROPDOWNBUTTON, combobox_statemap, NOPAD, 0 },
    { "Vertical.Scrollbar.trough", &GenericElementSpec, L"SCROLLBAR",
    	SBP_UPPERTRACKVERT, scrollbar_statemap, NOPAD, 0 },
    { "Vertical.Scrollbar.thumb", &ThumbElementSpec, L"SCROLLBAR",
    	SBP_THUMBBTNVERT, scrollbar_statemap, NOPAD, 0 },
    { "Vertical.Scrollbar.grip", &GenericElementSpec, L"SCROLLBAR",
    	SBP_GRIPPERVERT, scrollbar_statemap, NOPAD, 0 },
    { "Horizontal.Scrollbar.trough", &GenericElementSpec, L"SCROLLBAR",
    	SBP_UPPERTRACKHORZ, scrollbar_statemap, NOPAD, 0 },
    { "Horizontal.Scrollbar.thumb", &ThumbElementSpec, L"SCROLLBAR",
   	SBP_THUMBBTNHORZ, scrollbar_statemap, NOPAD, 0 },
    { "Horizontal.Scrollbar.grip", &GenericElementSpec, L"SCROLLBAR",
    	SBP_GRIPPERHORZ, scrollbar_statemap, NOPAD, 0 },
    { "Scrollbar.uparrow", &GenericElementSpec, L"SCROLLBAR",
    	SBP_ARROWBTN, uparrow_statemap, NOPAD, 0 },
    { "Scrollbar.downarrow", &GenericElementSpec, L"SCROLLBAR",
    	SBP_ARROWBTN, downarrow_statemap, NOPAD, 0 },
    { "Scrollbar.leftarrow", &GenericElementSpec, L"SCROLLBAR",
    	SBP_ARROWBTN, leftarrow_statemap, NOPAD, 0 },
    { "Scrollbar.rightarrow", &GenericElementSpec, L"SCROLLBAR",
    	SBP_ARROWBTN, rightarrow_statemap, NOPAD, 0 },
    { "Horizontal.Scale.slider", &GenericElementSpec, L"TRACKBAR",
    	TKP_THUMB, scale_statemap, NOPAD, 0 },
    { "Vertical.Scale.slider", &GenericElementSpec, L"TRACKBAR",
    	TKP_THUMBVERT, scale_statemap, NOPAD, 0 },
    { "Horizontal.Scale.track", &GenericElementSpec, L"TRACKBAR",
    	TKP_TRACK, scale_statemap, NOPAD, 0 },
    { "Vertical.Scale.track", &GenericElementSpec, L"TRACKBAR",
    	TKP_TRACKVERT, scale_statemap, NOPAD, 0 },
    /* ttk::progressbar elements */
    { "Horizontal.Progressbar.pbar", &PbarElementSpec, L"PROGRESS",
    	PP_CHUNK, null_statemap, NOPAD, 0 },
    { "Vertical.Progressbar.pbar", &PbarElementSpec, L"PROGRESS",
    	PP_CHUNKVERT, null_statemap, NOPAD, 0 },
    { "Horizontal.Progressbar.trough", &GenericElementSpec, L"PROGRESS",
    	PP_BAR, null_statemap, PAD(3,3,3,3), IGNORE_THEMESIZE },
    { "Vertical.Progressbar.trough", &GenericElementSpec, L"PROGRESS",
    	PP_BARVERT, null_statemap, PAD(3,3,3,3), IGNORE_THEMESIZE },
    /* ttk::notebook */
    { "tab", &TabElementSpec, L"TAB",
    	TABP_TABITEM, tabitem_statemap, PAD(3,3,3,0), 0 },
    { "client", &GenericElementSpec, L"TAB", 
    	TABP_PANE, null_statemap, PAD(1,1,3,3), 0 },
    { "NotebookPane.background", &GenericElementSpec, L"TAB",
    	TABP_BODY, null_statemap, NOPAD, 0 },
    { "Toolbutton.border", &GenericElementSpec, L"TOOLBAR",
    	TP_BUTTON, toolbutton_statemap, NOPAD,0 },
    { "Menubutton.button", &GenericElementSpec, L"TOOLBAR",
    	TP_SPLITBUTTON,toolbutton_statemap, NOPAD,0 },
    { "Menubutton.dropdown", &GenericElementSpec, L"TOOLBAR",
    	TP_SPLITBUTTONDROPDOWN,toolbutton_statemap, NOPAD,0 },
    { "Treeitem.indicator", &TreeIndicatorElementSpec, L"TREEVIEW",
    	TVP_GLYPH, tvpglyph_statemap, PAD(1,1,6,0), PAD_MARGINS },
    { "Treeheading.border", &GenericElementSpec, L"HEADER", 
    	HP_HEADERITEM, header_statemap, PAD(4,0,4,0),0 },
    { "sizegrip", &GenericElementSpec, L"STATUS", 
    	SP_GRIPPER, null_statemap, NOPAD,0 },

#if BROKEN_TEXT_ELEMENT
    { "Labelframe.text", &TextElementSpec, L"BUTTON", 
    	BP_GROUPBOX, groupbox_statemap, NOPAD,0 },
#endif

    { 0,0,0,0,0,NOPAD,0 }
};
#undef PAD

/*----------------------------------------------------------------------
 * +++ Initialization routine:
 */

MODULE_SCOPE int TtkXPTheme_Init(Tcl_Interp *interp, HWND hwnd)
{
    XPThemeData *themeData;
    XPThemeProcs *procs;
    HINSTANCE hlibrary;
    Ttk_Theme themePtr, parentPtr;
    ElementInfo *infoPtr;

    procs = LoadXPThemeProcs(&hlibrary);
    if (!procs)
	return TCL_ERROR;
    procs->stubWindow = hwnd;

    /*
     * Create the new style engine.
     */
    parentPtr = Ttk_GetTheme(interp, "winnative");
    themePtr = Ttk_CreateTheme(interp, "xpnative", parentPtr);

    if (!themePtr)
        return TCL_ERROR;

    /*
     * Set theme data and cleanup proc
     */

    themeData = (XPThemeData *)ckalloc(sizeof(XPThemeData));
    themeData->procs = procs;
    themeData->hlibrary = hlibrary;

    Ttk_SetThemeEnabledProc(themePtr, XPThemeEnabled, themeData);
    Ttk_RegisterCleanup(interp, themeData, XPThemeDeleteProc);

    /*
     * New elements:
     */
    for (infoPtr = ElementInfoTable; infoPtr->elementName != 0; ++infoPtr) {
	ClientData clientData = NewElementData(procs, infoPtr);
	Ttk_RegisterElementSpec(
	    themePtr, infoPtr->elementName, infoPtr->elementSpec, clientData);
	Ttk_RegisterCleanup(interp, clientData, DestroyElementData);
    }

    Ttk_RegisterElementSpec(themePtr, "Scale.trough", &ttkNullElementSpec, 0);

    /*
     * Layouts:
     */
    Ttk_RegisterLayout(themePtr, "TButton", ButtonLayout);
    Ttk_RegisterLayout(themePtr, "TMenubutton", MenubuttonLayout);
    Ttk_RegisterLayout(themePtr, "Vertical.TScrollbar",
    	VerticalScrollbarLayout);
    Ttk_RegisterLayout(themePtr, "Horizontal.TScrollbar",
    	HorizontalScrollbarLayout);
    Ttk_RegisterLayout(themePtr, "Vertical.TScale", VerticalScaleLayout);
    Ttk_RegisterLayout(themePtr, "Horizontal.TScale", HorizontalScaleLayout);

    Tcl_PkgProvide(interp, "ttk::theme::xpnative", TTK_VERSION);

    return TCL_OK;
}

#endif /* HAVE_UXTHEME_H */