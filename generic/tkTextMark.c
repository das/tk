/* 
 * tkTextMark.c --
 *
 *	This file contains the procedure that implement marks for
 *	text widgets.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkInt.h"
#include "tkText.h"
#include "tkPort.h"

/*
 * Macro that determines the size of a mark segment:
 */

#define MSEG_SIZE ((unsigned) (Tk_Offset(TkTextSegment, body) \
	+ sizeof(TkTextMark)))

/*
 * Forward references for procedures defined in this file:
 */

static void		InsertUndisplayProc _ANSI_ARGS_((TkText *textPtr,
			    TkTextDispChunk *chunkPtr));
static int		MarkDeleteProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr, int treeGone));
static TkTextSegment *	MarkCleanupProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static void		MarkCheckProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static int		MarkLayoutProc _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, TkTextSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, TkWrapMode wrapMode,
			    TkTextDispChunk *chunkPtr));
static int		MarkFindNext _ANSI_ARGS_((Tcl_Interp *interp,
			    TkText *textPtr, CONST char *markName));
static int		MarkFindPrev _ANSI_ARGS_((Tcl_Interp *interp,
			    TkText *textPtr, CONST char *markName));


/*
 * The following structures declare the "mark" segment types.
 * There are actually two types for marks, one with left gravity
 * and one with right gravity.  They are identical except for
 * their gravity property.
 */

Tk_SegType tkTextRightMarkType = {
    "mark",					/* name */
    0,						/* leftGravity */
    (Tk_SegSplitProc *) NULL,			/* splitProc */
    MarkDeleteProc,				/* deleteProc */
    MarkCleanupProc,				/* cleanupProc */
    (Tk_SegLineChangeProc *) NULL,		/* lineChangeProc */
    MarkLayoutProc,				/* layoutProc */
    MarkCheckProc				/* checkProc */
};

Tk_SegType tkTextLeftMarkType = {
    "mark",					/* name */
    1,						/* leftGravity */
    (Tk_SegSplitProc *) NULL,			/* splitProc */
    MarkDeleteProc,				/* deleteProc */
    MarkCleanupProc,				/* cleanupProc */
    (Tk_SegLineChangeProc *) NULL,		/* lineChangeProc */
    MarkLayoutProc,				/* layoutProc */
    MarkCheckProc				/* checkProc */
};

/*
 *--------------------------------------------------------------
 *
 * TkTextMarkCmd --
 *
 *	This procedure is invoked to process the "mark" options of
 *	the widget command for text widgets. See the user documentation
 *	for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
TkTextMarkCmd(textPtr, interp, objc, objv)
    register TkText *textPtr;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. Someone else has already
				 * parsed this command enough to know that
				 * objv[1] is "mark". */
{
    Tcl_HashEntry *hPtr;
    TkTextSegment *markPtr;
    Tcl_HashSearch search;
    TkTextIndex index;
    Tk_SegType *newTypePtr;

    int optionIndex;
    
    static CONST char *markOptionStrings[] = {
	"gravity", "names", "next", "previous", "set", 
	"unset", (char *) NULL 
    };
    enum markOptions {
	MARK_GRAVITY, MARK_NAMES, MARK_NEXT, MARK_PREVIOUS, 
	MARK_SET, MARK_UNSET
    };

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[2], markOptionStrings, 
			    "mark option", 0, &optionIndex) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum markOptions)optionIndex) {
	case MARK_GRAVITY: {
	    char c;
	    int length;
	    char *str;
	    
	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "markName ?gravity?");
		return TCL_ERROR;
	    }
	    hPtr = Tcl_FindHashEntry(&textPtr->markTable, Tcl_GetString(objv[3]));
	    if (hPtr == NULL) {
		Tcl_AppendResult(interp, "there is no mark named \"",
				 Tcl_GetString(objv[3]), "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    markPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
	    if (objc == 4) {
		if (markPtr->typePtr == &tkTextRightMarkType) {
		    Tcl_SetResult(interp, "right", TCL_STATIC);
		} else {
		    Tcl_SetResult(interp, "left", TCL_STATIC);
		}
		return TCL_OK;
	    }
	    str = Tcl_GetStringFromObj(objv[4],&length);
	    c = str[0];
	    if ((c == 'l') && (strncmp(str, "left", (unsigned)length) == 0)) {
		newTypePtr = &tkTextLeftMarkType;
	    } else if ((c == 'r') &&
		    (strncmp(str, "right", (unsigned)length) == 0)) {
		newTypePtr = &tkTextRightMarkType;
	    } else {
		Tcl_AppendResult(interp, "bad mark gravity \"", str, 
				 "\": must be left or right", (char *) NULL);
		return TCL_ERROR;
	    }
	    TkTextMarkSegToIndex(textPtr, markPtr, &index);
	    TkBTreeUnlinkSegment(textPtr->tree, markPtr,
		    markPtr->body.mark.linePtr);
	    markPtr->typePtr = newTypePtr;
	    TkBTreeLinkSegment(markPtr, &index);
	    break;
	}
	case MARK_NAMES: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, NULL);
		return TCL_ERROR;
	    }
	    for (hPtr = Tcl_FirstHashEntry(&textPtr->markTable, &search);
		    hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
		Tcl_AppendElement(interp,
			Tcl_GetHashKey(&textPtr->markTable, hPtr));
	    }
	    break;
	}
	case MARK_NEXT: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "index");
		return TCL_ERROR;
	    }
	    return MarkFindNext(interp, textPtr, Tcl_GetString(objv[3]));
	}
	case MARK_PREVIOUS: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 3, objv, "index");
		return TCL_ERROR;
	    }
	    return MarkFindPrev(interp, textPtr, Tcl_GetString(objv[3]));
	}
	case MARK_SET: {
	    if (objc != 5) {
		Tcl_WrongNumArgs(interp, 3, objv, "markName index");
		return TCL_ERROR;
	    }
	    if (TkTextGetObjIndex(interp, textPtr, objv[4], &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    TkTextSetMark(textPtr, Tcl_GetString(objv[3]), &index);
	    return TCL_OK;
	}
	case MARK_UNSET: {
	    int i;
	    for (i = 3; i < objc; i++) {
		hPtr = Tcl_FindHashEntry(&textPtr->markTable, Tcl_GetString(objv[i]));
		if (hPtr != NULL) {
		    markPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
		    if ((markPtr == textPtr->insertMarkPtr)
			    || (markPtr == textPtr->currentMarkPtr)) {
			continue;
		    }
		    TkBTreeUnlinkSegment(textPtr->tree, markPtr,
			    markPtr->body.mark.linePtr);
		    Tcl_DeleteHashEntry(hPtr);
		    ckfree((char *) markPtr);
		}
	    }
	    break;
	}
	
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkTextSetMark --
 *
 *	Set a mark to a particular position, creating a new mark if
 *	one doesn't already exist.
 *
 * Results:
 *	The return value is a pointer to the mark that was just set.
 *
 * Side effects:
 *	A new mark is created, or an existing mark is moved.
 *
 *----------------------------------------------------------------------
 */

TkTextSegment *
TkTextSetMark(textPtr, name, indexPtr)
    TkText *textPtr;		/* Text widget in which to create mark. */
    CONST char *name;		/* Name of mark to set. */
    TkTextIndex *indexPtr;	/* Where to set mark. */
{
    Tcl_HashEntry *hPtr;
    TkTextSegment *markPtr;
    TkTextIndex insertIndex;
    int new;

    hPtr = Tcl_CreateHashEntry(&textPtr->markTable, name, &new);
    markPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
    if (!new) {
	/*
	 * If this is the insertion point that's being moved, be sure
	 * to force a display update at the old position.  Also, don't
	 * let the insertion cursor be after the final newline of the
	 * file.
	 */

	if (markPtr == textPtr->insertMarkPtr) {
	    TkTextIndex index, index2;
	    TkTextMarkSegToIndex(textPtr, textPtr->insertMarkPtr, &index);
	    TkTextIndexForwChars(&index, 1, &index2, COUNT_INDICES);
	    /* 
	     * While we wish to redisplay, no heights have changed, so
	     * no need to call TkTextInvalidateLineMetrics.
	     */
	    TkTextChanged(textPtr, &index, &index2);
	    if (TkBTreeLineIndex(indexPtr->linePtr)
		    == TkBTreeNumLines(textPtr->tree))  {
		TkTextIndexBackChars(indexPtr, 1, &insertIndex, COUNT_INDICES);
		indexPtr = &insertIndex;
	    }
	}
	TkBTreeUnlinkSegment(textPtr->tree, markPtr,
		markPtr->body.mark.linePtr);
    } else {
	markPtr = (TkTextSegment *) ckalloc(MSEG_SIZE);
	markPtr->typePtr = &tkTextRightMarkType;
	markPtr->size = 0;
	markPtr->body.mark.textPtr = textPtr;
	markPtr->body.mark.linePtr = indexPtr->linePtr;
	markPtr->body.mark.hPtr = hPtr;
	Tcl_SetHashValue(hPtr, markPtr);
    }
    TkBTreeLinkSegment(markPtr, indexPtr);

    /*
     * If the mark is the insertion cursor, then update the screen at the
     * mark's new location.
     */

    if (markPtr == textPtr->insertMarkPtr) {
	TkTextIndex index2;

	TkTextIndexForwChars(indexPtr, 1, &index2, COUNT_INDICES);
	/* 
	 * While we wish to redisplay, no heights have changed, so
	 * no need to call TkTextInvalidateLineMetrics
	 */
	TkTextChanged(textPtr, indexPtr, &index2);
    }
    return markPtr;
}

/*
 *--------------------------------------------------------------
 *
 * TkTextMarkSegToIndex --
 *
 *	Given a segment that is a mark, create an index that
 *	refers to the next text character (or other text segment
 *	with non-zero size) after the mark.
 *
 * Results:
 *	*IndexPtr is filled in with index information.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

void
TkTextMarkSegToIndex(textPtr, markPtr, indexPtr)
    TkText *textPtr;		/* Text widget containing mark. */
    TkTextSegment *markPtr;	/* Mark segment. */
    TkTextIndex *indexPtr;	/* Index information gets stored here.  */
{
    TkTextSegment *segPtr;

    indexPtr->tree = textPtr->tree;
    indexPtr->linePtr = markPtr->body.mark.linePtr;
    indexPtr->byteIndex = 0;
    for (segPtr = indexPtr->linePtr->segPtr; segPtr != markPtr;
	    segPtr = segPtr->nextPtr) {
	indexPtr->byteIndex += segPtr->size;
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkTextMarkNameToIndex --
 *
 *	Given the name of a mark, return an index corresponding
 *	to the mark name.
 *
 * Results:
 *	The return value is TCL_OK if "name" exists as a mark in
 *	the text widget.  In this case *indexPtr is filled in with
 *	the next segment whose after the mark whose size is
 *	non-zero.  TCL_ERROR is returned if the mark doesn't exist
 *	in the text widget.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkTextMarkNameToIndex(textPtr, name, indexPtr)
    TkText *textPtr;		/* Text widget containing mark. */
    CONST char *name;		/* Name of mark. */
    TkTextIndex *indexPtr;	/* Index information gets stored here. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&textPtr->markTable, name);
    if (hPtr == NULL) {
	return TCL_ERROR;
    }
    TkTextMarkSegToIndex(textPtr, (TkTextSegment *) Tcl_GetHashValue(hPtr),
	    indexPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MarkDeleteProc --
 *
 *	This procedure is invoked by the text B-tree code whenever
 *	a mark lies in a range of characters being deleted.
 *
 * Results:
 *	Returns 1 to indicate that deletion has been rejected.
 *
 * Side effects:
 *	None (even if the whole tree is being deleted we don't
 *	free up the mark;  it will be done elsewhere).
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
MarkDeleteProc(segPtr, linePtr, treeGone)
    TkTextSegment *segPtr;		/* Segment being deleted. */
    TkTextLine *linePtr;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * MarkCleanupProc --
 *
 *	This procedure is invoked by the B-tree code whenever a
 *	mark segment is moved from one line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The linePtr field of the segment gets updated.
 *
 *--------------------------------------------------------------
 */

static TkTextSegment *
MarkCleanupProc(markPtr, linePtr)
    TkTextSegment *markPtr;		/* Mark segment that's being moved. */
    TkTextLine *linePtr;		/* Line that now contains segment. */
{
    markPtr->body.mark.linePtr = linePtr;
    return markPtr;
}

/*
 *--------------------------------------------------------------
 *
 * MarkLayoutProc --
 *
 *	This procedure is the "layoutProc" for mark segments.
 *
 * Results:
 *	If the mark isn't the insertion cursor then the return
 *	value is -1 to indicate that this segment shouldn't be
 *	displayed.  If the mark is the insertion character then
 *	1 is returned and the chunkPtr structure is filled in.
 *
 * Side effects:
 *	None, except for filling in chunkPtr.
 *
 *--------------------------------------------------------------
 */

	/*ARGSUSED*/
static int
MarkLayoutProc(textPtr, indexPtr, segPtr, offset, maxX, maxChars,
	noCharsYet, wrapMode, chunkPtr)
    TkText *textPtr;		/* Text widget being layed out. */
    TkTextIndex *indexPtr;	/* Identifies first character in chunk. */
    TkTextSegment *segPtr;	/* Segment corresponding to indexPtr. */
    int offset;			/* Offset within segPtr corresponding to
				 * indexPtr (always 0). */
    int maxX;			/* Chunk must not occupy pixels at this
				 * position or higher. */
    int maxChars;		/* Chunk must not include more than this
				 * many characters. */
    int noCharsYet;		/* Non-zero means no characters have been
				 * assigned to this line yet. */
    TkWrapMode wrapMode;	/* Not used. */
    register TkTextDispChunk *chunkPtr;
				/* Structure to fill in with information
				 * about this chunk.  The x field has already
				 * been set by the caller. */
{
    if (segPtr != textPtr->insertMarkPtr) {
	return -1;
    }

    chunkPtr->displayProc = TkTextInsertDisplayProc;
    chunkPtr->undisplayProc = InsertUndisplayProc;
    chunkPtr->measureProc = (Tk_ChunkMeasureProc *) NULL;
    chunkPtr->bboxProc = (Tk_ChunkBboxProc *) NULL;
    chunkPtr->numBytes = 0;
    chunkPtr->minAscent = 0;
    chunkPtr->minDescent = 0;
    chunkPtr->minHeight = 0;
    chunkPtr->width = 0;

    /*
     * Note: can't break a line after the insertion cursor:  this
     * prevents the insertion cursor from being stranded at the end
     * of a line.
     */

    chunkPtr->breakIndex = -1;
    chunkPtr->clientData = (ClientData) textPtr;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * TkTextInsertDisplayProc --
 *
 *	This procedure is called to display the insertion
 *	cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Graphics are drawn.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
void
TkTextInsertDisplayProc(chunkPtr, x, y, height, baseline, display, dst, screenY)
    TkTextDispChunk *chunkPtr;		/* Chunk that is to be drawn. */
    int x;				/* X-position in dst at which to
					 * draw this chunk (may differ from
					 * the x-position in the chunk because
					 * of scrolling). */
    int y;				/* Y-position at which to draw this
					 * chunk in dst (x-position is in
					 * the chunk itself). */
    int height;				/* Total height of line. */
    int baseline;			/* Offset of baseline from y. */
    Display *display;			/* Display to use for drawing. */
    Drawable dst;			/* Pixmap or window in which to draw
					 * chunk. */
    int screenY;			/* Y-coordinate in text window that
					 * corresponds to y. */
{
    TkText *textPtr = (TkText *) chunkPtr->clientData;
    TkTextIndex index;
    int halfWidth = textPtr->insertWidth/2;
    int rightSideWidth;
    int ix = 0, iy = 0, iw = 0, ih = 0, charWidth = 0;
      
    if(textPtr->insertCursorType) {
	TkTextMarkSegToIndex(textPtr, textPtr->insertMarkPtr, &index);
	TkTextCharBbox(textPtr, &index, &ix, &iy, &iw, &ih, &charWidth);
	rightSideWidth = charWidth + halfWidth;
    } else {
	rightSideWidth = halfWidth;
    }
    
    if ((x + rightSideWidth) < 0) {
	/*
	 * The insertion cursor is off-screen.
	 * Indicate caret at 0,0 and return.
	 */

	Tk_SetCaretPos(textPtr->tkwin, 0, 0, height);
	return;
    }

    Tk_SetCaretPos(textPtr->tkwin, x - halfWidth, screenY, height);

    /*
     * As a special hack to keep the cursor visible on mono displays
     * (or anywhere else that the selection and insertion cursors
     * have the same color) write the default background in the cursor
     * area (instead of nothing) when the cursor isn't on.  Otherwise
     * the selection might hide the cursor.
     */

    if (textPtr->flags & INSERT_ON) {
	Tk_Fill3DRectangle(textPtr->tkwin, dst, textPtr->insertBorder,
		x - halfWidth, y, charWidth + textPtr->insertWidth, height,
		textPtr->insertBorderWidth, TK_RELIEF_RAISED);
    } else if (textPtr->selBorder == textPtr->insertBorder) {
	Tk_Fill3DRectangle(textPtr->tkwin, dst, textPtr->border,
		x - halfWidth, y, charWidth + textPtr->insertWidth, height,
		0, TK_RELIEF_FLAT);
    }
}

/*
 *--------------------------------------------------------------
 *
 * InsertUndisplayProc --
 *
 *	This procedure is called when the insertion cursor is no
 *	longer at a visible point on the display.  It does nothing
 *	right now.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
InsertUndisplayProc(textPtr, chunkPtr)
    TkText *textPtr;			/* Overall information about text
					 * widget. */
    TkTextDispChunk *chunkPtr;		/* Chunk that is about to be freed. */
{
    return;
}

/*
 *--------------------------------------------------------------
 *
 * MarkCheckProc --
 *
 *	This procedure is invoked by the B-tree code to perform
 *	consistency checks on mark segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The procedure panics if it detects anything wrong with
 *	the mark.
 *
 *--------------------------------------------------------------
 */

static void
MarkCheckProc(markPtr, linePtr)
    TkTextSegment *markPtr;		/* Segment to check. */
    TkTextLine *linePtr;		/* Line containing segment. */
{
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;

    if (markPtr->body.mark.linePtr != linePtr) {
	panic("MarkCheckProc: markPtr->body.mark.linePtr bogus");
    }

    /*
     * Make sure that the mark is still present in the text's mark
     * hash table.
     */

    for (hPtr = Tcl_FirstHashEntry(&markPtr->body.mark.textPtr->markTable,
	    &search); hPtr != markPtr->body.mark.hPtr;
	    hPtr = Tcl_NextHashEntry(&search)) {
	if (hPtr == NULL) {
	    panic("MarkCheckProc couldn't find hash table entry for mark");
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * MarkFindNext --
 *
 *	This procedure searches forward for the next mark.
 *
 * Results:
 *	A standard Tcl result, which is a mark name or an empty string.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
MarkFindNext(interp, textPtr, string)
    Tcl_Interp *interp;			/* For error reporting */
    TkText *textPtr;			/* The widget */
    CONST char *string;			/* The starting index or mark name */
{
    TkTextIndex index;
    Tcl_HashEntry *hPtr;
    register TkTextSegment *segPtr;
    int offset;


    hPtr = Tcl_FindHashEntry(&textPtr->markTable, string);
    if (hPtr != NULL) {
	/*
	 * If given a mark name, return the next mark in the list of
	 * segments, even if it happens to be at the same character position.
	 */
	segPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
	TkTextMarkSegToIndex(textPtr, segPtr, &index);
	segPtr = segPtr->nextPtr;
    } else {
	/*
	 * For non-mark name indices we want to return any marks that
	 * are right at the index.
	 */
	if (TkTextGetIndex(interp, textPtr, string, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (offset = 0, segPtr = index.linePtr->segPtr; 
		segPtr != NULL && offset < index.byteIndex;
		offset += segPtr->size,	segPtr = segPtr->nextPtr) {
	    /* Empty loop body */ ;
	}
    }
    while (1) {
	/*
	 * segPtr points at the first possible candidate,
	 * or NULL if we ran off the end of the line.
	 */
	for ( ; segPtr != NULL ; segPtr = segPtr->nextPtr) {
	    if (segPtr->typePtr == &tkTextRightMarkType ||
		    segPtr->typePtr == &tkTextLeftMarkType) {
		Tcl_SetResult(interp,
		    Tcl_GetHashKey(&textPtr->markTable, segPtr->body.mark.hPtr),
		    TCL_STATIC);
		return TCL_OK;
	    }
	}
	index.linePtr = TkBTreeNextLine(index.linePtr);
	if (index.linePtr == (TkTextLine *) NULL) {
	    return TCL_OK;
	}
	index.byteIndex = 0;
	segPtr = index.linePtr->segPtr;
    }
}

/*
 *--------------------------------------------------------------
 *
 * MarkFindPrev --
 *
 *	This procedure searches backwards for the previous mark.
 *
 * Results:
 *	A standard Tcl result, which is a mark name or an empty string.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
MarkFindPrev(interp, textPtr, string)
    Tcl_Interp *interp;			/* For error reporting */
    TkText *textPtr;			/* The widget */
    CONST char *string;			/* The starting index or mark name */
{
    TkTextIndex index;
    Tcl_HashEntry *hPtr;
    register TkTextSegment *segPtr, *seg2Ptr, *prevPtr;
    int offset;


    hPtr = Tcl_FindHashEntry(&textPtr->markTable, string);
    if (hPtr != NULL) {
	/*
	 * If given a mark name, return the previous mark in the list of
	 * segments, even if it happens to be at the same character position.
	 */
	segPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
	TkTextMarkSegToIndex(textPtr, segPtr, &index);
    } else {
	/*
	 * For non-mark name indices we do not return any marks that
	 * are right at the index.
	 */
	if (TkTextGetIndex(interp, textPtr, string, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	for (offset = 0, segPtr = index.linePtr->segPtr; 
		segPtr != NULL && offset < index.byteIndex;
		offset += segPtr->size, segPtr = segPtr->nextPtr) {
	    /* Empty loop body */ ;
	}
    }
    while (1) {
	/*
	 * segPtr points just past the first possible candidate,
	 * or at the begining of the line.
	 */
	for (prevPtr = NULL, seg2Ptr = index.linePtr->segPtr; 
		seg2Ptr != NULL && seg2Ptr != segPtr;
		seg2Ptr = seg2Ptr->nextPtr) {
	    if (seg2Ptr->typePtr == &tkTextRightMarkType ||
		    seg2Ptr->typePtr == &tkTextLeftMarkType) {
		prevPtr = seg2Ptr;
	    }
	}
	if (prevPtr != NULL) {
	    Tcl_SetResult(interp, 
		Tcl_GetHashKey(&textPtr->markTable, prevPtr->body.mark.hPtr),
		TCL_STATIC);
	    return TCL_OK;
	}
	index.linePtr = TkBTreePreviousLine(index.linePtr);
	if (index.linePtr == (TkTextLine *) NULL) {
	    return TCL_OK;
	}
	segPtr = NULL;
    }
}
