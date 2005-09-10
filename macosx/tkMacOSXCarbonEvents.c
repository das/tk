/*
 * tkMacOSXCarbonEvents.c --
 *
 *	This file implements functions that register for and handle
 *      various Carbon Events.  The reason a separate set of handlers 
 *      is necessary is that not all interesting events get delivered
 *      directly to the event queue through ReceiveNextEvent.  Some only
 *      get delivered if you register a Carbon Event Handler for the event.
 *
 *      Copyright 2001, Apple Computer, Inc.
 *
 *      The following terms apply to all files originating from Apple
 *      Computer, Inc. ("Apple") and associated with the software
 *      unless explicitly disclaimed in individual files.
 *
 *
 *      Apple hereby grants permission to use, copy, modify,
 *      distribute, and license this software and its documentation
 *      for any purpose, provided that existing copyright notices are
 *      retained in all copies and that this notice is included
 *      verbatim in any distributions. No written agreement, license,
 *      or royalty fee is required for any of the authorized
 *      uses. Modifications to this software may be copyrighted by
 *      their authors and need not follow the licensing terms
 *      described here, provided that the new terms are clearly
 *      indicated on the first page of each file where they apply.
 *
 *
 *      IN NO EVENT SHALL APPLE, THE AUTHORS OR DISTRIBUTORS OF THE
 *      SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
 *      INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF
 *      THIS SOFTWARE, ITS DOCUMENTATION, OR ANY DERIVATIVES THEREOF,
 *      EVEN IF APPLE OR THE AUTHORS HAVE BEEN ADVISED OF THE
 *      POSSIBILITY OF SUCH DAMAGE.  APPLE, THE AUTHORS AND
 *      DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING,
 *      BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 *      FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS
 *      SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND APPLE,THE
 *      AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 *      MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 *      GOVERNMENT USE: If you are acquiring this software on behalf
 *      of the U.S. government, the Government shall have only
 *      "Restricted Rights" in the software and related documentation
 *      as defined in the Federal Acquisition Regulations (FARs) in
 *      Clause 52.227.19 (c) (2).  If you are acquiring the software
 *      on behalf of the Department of Defense, the software shall be
 *      classified as "Commercial Computer Software" and the
 *      Government shall have only "Restricted Rights" as defined in
 *      Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 *      foregoing, the authors grant the U.S. Government and others
 *      acting in its behalf permission to use and distribute the
 *      software in accordance with the terms specified in this
 *      license.
 *
 * RCS: @(#) $Id$
 */

#include "tkInt.h"
#include "tkMacOSXInt.h"
#include "tkMacOSXEvent.h"
#include "tkMacOSXDebug.h"

/*
#ifdef  TK_MAC_DEBUG
#define TK_MAC_DEBUG_CARBON_EVENTS
#endif
*/

/* Declarations of functions used only in this file */
static OSStatus CarbonEventHandlerProc (
                              EventHandlerCallRef callRef,
                              EventRef inEvent,
                              void *userData);

/*
 *----------------------------------------------------------------------
 *
 * CarbonEventHandlerProc --
 *
 *        This procedure is the handler for all registered CarbonEvents.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Dispatches CarbonEvents.
 *
 *----------------------------------------------------------------------
 */

static OSStatus
CarbonEventHandlerProc (
        EventHandlerCallRef callRef,
        EventRef inEvent,
        void *inUserData)
{
    OSStatus         result = eventNotHandledErr;
    TkMacOSXEvent    macEvent;
    MacEventStatus   eventStatus;

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    char buf [256];
    CarbonEventToAscii(inEvent, buf);
    fprintf(stderr, "CarbonEventHandlerProc started handling %s\n", buf);
    _DebugPrintEvent(inEvent);
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */

    macEvent.eventRef = inEvent;
    macEvent.eClass = GetEventClass(macEvent.eventRef);
    macEvent.eKind = GetEventKind(macEvent.eventRef);
    macEvent.interp = (Tcl_Interp *) inUserData;
    bzero(&eventStatus, sizeof(eventStatus));
    TkMacOSXProcessEvent(&macEvent,&eventStatus);
    if (eventStatus.stopProcessing) {
        result = noErr;
    }

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    fprintf(stderr, "CarbonEventHandlerProc finished handling %s: %s handled\n", buf,
                eventStatus.stopProcessing ? "   " : "not");
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacOSXInitCarbonEvents --
 *
 *        This procedure initializes all CarbonEvent handlers.
 *
 * Results:
 *        None.
 *
 * Side effects:
 *        Handlers for Carbon Events are registered.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE void
TkMacOSXInitCarbonEvents (
        Tcl_Interp *interp)
{
    const EventTypeSpec dispatcherEventTypes[] = {
            {kEventClassMouse,          kEventMouseDown},
            {kEventClassMouse,          kEventMouseUp},
            {kEventClassMouse,          kEventMouseMoved},
            {kEventClassMouse,          kEventMouseDragged},
            {kEventClassMouse,          kEventMouseWheelMoved},
            {kEventClassWindow,         kEventWindowUpdate},
            {kEventClassWindow,         kEventWindowActivated},
            {kEventClassWindow,         kEventWindowDeactivated},
            {kEventClassKeyboard,       kEventRawKeyDown},
            {kEventClassKeyboard,       kEventRawKeyRepeat},
            {kEventClassKeyboard,       kEventRawKeyUp},
            {kEventClassKeyboard,       kEventRawKeyModifiersChanged},
            {kEventClassKeyboard,       kEventRawKeyRepeat},
            {kEventClassApplication,    kEventAppActivated},
            {kEventClassApplication,    kEventAppDeactivated},
            {kEventClassApplication,    kEventAppQuit},
            {kEventClassAppleEvent,     kEventAppleEvent},
    };
    const EventTypeSpec applicationEventTypes[] = {
            {kEventClassWindow,         kEventWindowExpanded},
            {kEventClassApplication,    kEventAppHidden},
            {kEventClassApplication,    kEventAppShown},
    };
    EventHandlerUPP handler = NewEventHandlerUPP(CarbonEventHandlerProc);

    InstallEventHandler(GetEventDispatcherTarget(), handler,
            GetEventTypeCount(dispatcherEventTypes), dispatcherEventTypes,
            (void *) interp, NULL);
    InstallEventHandler(GetApplicationEventTarget(), handler,
            GetEventTypeCount(applicationEventTypes), applicationEventTypes,
            (void *) interp, NULL);

#ifdef TK_MAC_DEBUG_CARBON_EVENTS
    _TraceEventByName(CFSTR("kEventMouseDown"));
    _TraceEventByName(CFSTR("kEventMouseUp"));
    _TraceEventByName(CFSTR("kEventMouseMoved"));
    _TraceEventByName(CFSTR("kEventMouseDragged"));
    _TraceEventByName(CFSTR("kEventMouseWheelMoved"));
    _TraceEventByName(CFSTR("kEventWindowUpdate"));
    _TraceEventByName(CFSTR("kEventWindowActivated"));
    _TraceEventByName(CFSTR("kEventWindowDeactivated"));
    _TraceEventByName(CFSTR("kEventRawKeyDown"));
    _TraceEventByName(CFSTR("kEventRawKeyRepeat"));
    _TraceEventByName(CFSTR("kEventRawKeyUp"));
    _TraceEventByName(CFSTR("kEventRawKeyModifiersChanged"));
    _TraceEventByName(CFSTR("kEventRawKeyRepeat"));
    _TraceEventByName(CFSTR("kEventAppActivated"));
    _TraceEventByName(CFSTR("kEventAppDeactivated"));
    _TraceEventByName(CFSTR("kEventAppQuit"));
    _TraceEventByName(CFSTR("kEventAppleEvent"));
    _TraceEventByName(CFSTR("kEventWindowExpanded"));
    _TraceEventByName(CFSTR("kEventAppHidden"));
    _TraceEventByName(CFSTR("kEventAppShown"));
#endif /* TK_MAC_DEBUG_CARBON_EVENTS */
}

