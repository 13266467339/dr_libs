// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_gui.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <stdio.h>  // For testing. Delete Me.


#if defined(__WIN32__) || defined(_WIN32) || defined(_WIN64)
#define EASYGUI_USE_WIN32_THREADS

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define EASYGUI_USE_POSIX_THREADS
#endif


/////////////////////////////////////////////////////////////////
//
// PRIVATE CORE API
//
/////////////////////////////////////////////////////////////////


// Context Flags
#define IS_INBOUND_EVENTS_LOCKED            (1U << 0)
#define IS_CONTEXT_DEAD                     (1U << 1)

// Element Flags
#define IS_ELEMENT_HIDDEN                   (1U << 0)
#define IS_ELEMENT_CLIPPING_DISABLED        (1U << 1)
#define IS_ELEMENT_DEAD                     (1U << 31)



/// Locks the inbound events.
///
/// This is called from easygui_post_inbound_event().
///
/// If false is returned it means there was an error locking the events and the event should be cancelled.
easygui_bool easygui_lock_inbound_events(easygui_event* pEvent);

/// Unlocks the outbound events.
///
/// This is called from easygui_post_outbound_event()
void easygui_unlock_inbound_events(easygui_event* pEvent);

/// Determines whether or not inbound events are locked.
///
/// This is mainly used for error checking.
easygui_bool easygui_is_inbound_events_locked(easygui_context* pContext);


/// Locks the outbound events.
///
/// This will validate that the given element is allowed to have an event posted. When false is returned, nothing
/// will have been locked and the outbound event should be cancelled.
///
/// This will return false if the given element has been marked as dead, or if there is some other reason it should
/// not be receiving events.
easygui_bool easygui_lock_outbound_events(easygui_element* pElement);

/// Unlocks the outbound events.
void easygui_unlock_outbound_events(easygui_element* pElement);

/// Determines whether or not outbound events are locked.
easygui_bool easygui_is_outbound_events_locked(easygui_context* pContext);


/// Marks the given element as dead.
void easygui_mark_element_as_dead(easygui_element* pElement);

/// Determines whether or not the given element is marked as dead.
easygui_bool easygui_is_element_marked_as_dead(const easygui_element* pElement);

/// Deletes every element that has been marked as dead.
void easygui_delete_elements_marked_as_dead(easygui_context* pContext);


/// Marks the given context as deleted.
void easygui_mark_context_as_dead(easygui_context* pContext);

/// Determines whether or not the given context is marked as dead.
easygui_bool easygui_is_context_marked_as_dead(const easygui_context* pContext);


/// Deletes the given context for real.
///
/// If a context is deleted during the processing of an inbound event it will not be deleting immediately - this
/// will delete the context for real.
void easygui_delete_context_for_real(easygui_context* pContext);

/// Deletes the given element for real.
///
/// Sometimes an element will not be deleted straight away but instead just marked as dead. We use this to delete
/// the given element for real.
void easygui_delete_element_for_real(easygui_element* pElement);


/// Orphans the given element.
///
/// This does NOT turn the element into a top level element. The reason for this is that we will be calling this
/// during the shutdown routine, so we don't want to waste time adding it to the top-level list only to have it
/// removed.
void easygui_orphan_element(easygui_element* pElement);


/// Functions for handling inbound events.
easygui_bool easygui_handle_inbound_mouse_enter(easygui_event* pEvent);
easygui_bool easygui_handle_inbound_mouse_leave(easygui_event* pEvent);
easygui_bool easygui_handle_inbound_mouse_move(easygui_event* pEvent);


/// Functiosn for posting outbound events.
void easygui_post_outbound_event_capture_mouse(easygui_element* pElement);
void easygui_post_outbound_event_capture_mouse_global(easygui_element* pElement);
void easygui_post_outbound_event_release_mouse(easygui_element* pElement);
void easygui_post_outbound_event_release_mouse_global(easygui_element* pElement);
void easygui_post_outbound_event_capture_keyboard(easygui_element* pElement);
void easygui_post_outbound_event_capture_keyboard_global(easygui_element* pElement);
void easygui_post_outbound_event_release_keyboard(easygui_element* pElement);
void easygui_post_outbound_event_release_keyboard_global(easygui_element* pElement);


/// Posts a log message.
void easygui_log(easygui_context* pContext, const char* message);


/// Null implementations of painting callbacks so we can avoid checking for null in the painting functions.
void easygui_draw_begin_null(void*);
void easygui_draw_end_null(void*);
void easygui_draw_clip_null(easygui_rect, void*);
void easygui_draw_line_null(float, float, float, float, float, easygui_color, void*);
void easygui_draw_rect_null(easygui_rect, easygui_color, void*);
void easygui_draw_text_null(const char*, unsigned int, int, int, easygui_font, easygui_color, void*);




easygui_bool easygui_lock_inbound_events(easygui_event* pEvent)
{
    assert(pEvent != NULL);
    assert(pEvent->pContext != NULL);

#ifdef EASYGUI_USE_WIN32_THREADS
    HANDLE hMutex = pEvent->pContext->inboundEventLock;
    assert(hMutex != NULL);

    WaitForSingleObject(hMutex, INFINITE);
#endif

#ifdef EASYGUI_USE_POSIX_THREADS
    pthread_mutex_t* pMutex = pEvent->pContext->inboundEventLock;
    assert(pMutex != NULL);

    pthread_mutex_lock(pMutex);
#endif

    // We need to set a flag so we can do error checking and ensure correctness with event handling.
    pEvent->pContext->flags |= IS_INBOUND_EVENTS_LOCKED;


    return EASYGUI_TRUE;
}

void easygui_unlock_inbound_events(easygui_event* pEvent)
{
    assert(pEvent != NULL);
    assert(pEvent->pContext != NULL);

#ifdef EASYGUI_USE_WIN32_THREADS
    HANDLE hMutex = pEvent->pContext->inboundEventLock;
    assert(hMutex != NULL);
#endif

#ifdef EASYGUI_USE_POSIX_THREADS
    pthread_mutex_t* pMutex = pEvent->pContext->inboundEventLock;
    assert(pMutex != NULL);
#endif


    // Here is where we want to clean up any elements that are marked as dead. When events are being handled elements are not deleted
    // immediately but instead only marked for deletion. This function will be called at the end of event processing which makes it
    // an appropriate place for cleaning up dead elements.
    easygui_delete_elements_marked_as_dead(pEvent->pContext);

    // If the context has been marked for deletion than we will need to delete that too.
    if (easygui_is_context_marked_as_dead(pEvent->pContext))
    {
        easygui_delete_context_for_real(pEvent->pContext);
    }
    else
    {
        // The internal flag needs to be unset so we can do error checking to ensure correctness.
        pEvent->pContext->flags &= ~IS_INBOUND_EVENTS_LOCKED;
    }


    // The inbound event locks need to be unlocked at the end.
#ifdef EASYGUI_USE_WIN32_THREADS
    SetEvent(hMutex);
#endif

#ifdef EASYGUI_USE_POSIX_THREADS
    pthread_mutex_unlock(pMutex);
#endif
}

easygui_bool easygui_is_inbound_events_locked(easygui_context* pContext)
{
    assert(pContext != NULL);

    return (pContext->flags & IS_INBOUND_EVENTS_LOCKED) != 0;
}



easygui_bool easygui_lock_outbound_events(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);

    // If the assert below fails it means an outbound event is trying to get posted while another outbound event is still running. easy_gui
    // should guard against this case, so it is likely a bug with easy_gui.
    assert(!easygui_is_outbound_events_locked(pElement->pContext));


    // We want to cancel the outbound event if the element is marked as dead.
    if (easygui_is_element_marked_as_dead(pElement)) {
        return EASYGUI_FALSE;
    }


    // At this point everything should be fine so we just increment the count (which should never go above 1) and return true.
    pElement->pContext->outboundEventLockCounter += 1;

    return EASYGUI_TRUE;
}

void easygui_unlock_outbound_events(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);

    // If the assert below fails it means we are trying to lock the outbound events when they were never locked beforehand. This is a
    // sign of bad lock()/unlock() matching, so we'll keep the assert here to ensure correctness.
    assert(easygui_is_outbound_events_locked(pElement->pContext));


    pElement->pContext->outboundEventLockCounter -= 1;
}

easygui_bool easygui_is_outbound_events_locked(easygui_context* pContext)
{
    assert(pContext != NULL);
    return pContext->outboundEventLockCounter > 0;
}


void easygui_mark_element_as_dead(easygui_element* pElement)
{
    assert(pElement != NULL);
    assert(pElement->pContext != NULL);

    pElement->flags |= IS_ELEMENT_DEAD;


    if (pElement->pContext->pFirstDeadElement != NULL) {
        pElement->pNextDeadElement = pElement->pContext->pFirstDeadElement;
    }

    pElement->pContext->pFirstDeadElement = pElement;


    // When an element is deleted, so is it's children - they also need to be marked as dead.
    for (easygui_element* pChild = pElement->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        easygui_mark_element_as_dead(pChild);
    }
}

easygui_bool easygui_is_element_marked_as_dead(const easygui_element* pElement)
{
    assert(pElement != NULL);

    return (pElement->flags & IS_ELEMENT_DEAD) != 0;
}

void easygui_delete_elements_marked_as_dead(easygui_context* pContext)
{
    assert(pContext != NULL);

    while (pContext->pFirstDeadElement != NULL)
    {
        easygui_element* pDeadElement = pContext->pFirstDeadElement;
        pContext->pFirstDeadElement = pContext->pFirstDeadElement->pNextDeadElement;

        easygui_delete_element_for_real(pDeadElement);
    }
}


void easygui_mark_context_as_dead(easygui_context* pContext)
{
    assert(pContext != NULL);
    assert(!easygui_is_context_marked_as_dead(pContext));

    pContext->flags |= IS_CONTEXT_DEAD;
}

easygui_bool easygui_is_context_marked_as_dead(const easygui_context* pContext)
{
    assert(pContext != NULL);

    return (pContext->flags & IS_CONTEXT_DEAD) != 0;
}



void easygui_delete_context_for_real(easygui_context* pContext)
{
    assert(pContext != NULL);

    // All elements marked as dead need to be deleted.
    easygui_delete_elements_marked_as_dead(pContext);


    
#ifdef EASYGUI_USE_WIN32_THREADS
    CloseHandle((HANDLE)pContext->inboundEventLock);
#endif

#ifdef EASYGUI_USE_POSIX_THREADS
    pthread_mutex_destroy((pthread_mutex_t*)pContext->inboundEventLock);
    free(pContext->inboundEventLock);
#endif

    free(pContext);
}

void easygui_delete_element_for_real(easygui_element* pElement)
{
    assert(pElement != NULL);

    free(pElement);
}


void easygui_orphan_element(easygui_element* pElement)
{
    if (pElement->pParent != NULL) {
        if (pElement->pParent->pFirstChild == pElement) {
            pElement->pParent->pFirstChild = pElement->pNextSibling;
        }

        if (pElement->pParent->pLastChild == pElement) {
            pElement->pParent->pLastChild = pElement->pPrevSibling;
        }


        if (pElement->pPrevSibling != NULL) {
            pElement->pPrevSibling->pNextSibling = pElement->pNextSibling;
        }

        if (pElement->pNextSibling != NULL) {
            pElement->pNextSibling->pPrevSibling = pElement->pPrevSibling;
        }
    }

    pElement->pParent      = NULL;
    pElement->pPrevSibling = NULL;
    pElement->pNextSibling = NULL;
}



easygui_bool easygui_handle_inbound_mouse_enter(easygui_event* pEvent)
{
    (void)pEvent;
    return EASYGUI_TRUE;
}

easygui_bool easygui_handle_inbound_mouse_leave(easygui_event* pEvent)
{
    (void)pEvent;
    return EASYGUI_TRUE;
}

easygui_bool easygui_handle_inbound_mouse_move(easygui_event* pEvent)
{
    easygui_element* pEventReceiver = pEvent->pContext->pElementWithMouseCapture;
    if (pEventReceiver == NULL)
    {
        pEventReceiver = easygui_find_element_under_point(pEvent->pElement, (float)pEvent->mouse_move.mousePosX, (float)pEvent->mouse_move.mousePosY);
    }

    if (pEventReceiver != NULL && pEventReceiver->onMouseMove != NULL)
    {
        float relativeMousePosX = (float)pEvent->mouse_move.mousePosX;
        float relativeMousePosY = (float)pEvent->mouse_move.mousePosY;
        easygui_make_point_relative_to_element(pEventReceiver, &relativeMousePosX, &relativeMousePosY);

        if (easygui_lock_outbound_events(pEventReceiver))
        {
            pEventReceiver->onMouseMove(pEventReceiver, (int)relativeMousePosX, (int)relativeMousePosY);
            easygui_unlock_outbound_events(pEventReceiver);
        }
        else
        {
            return EASYGUI_FALSE;       // Error locking outbound events.
        }
    }

    return EASYGUI_TRUE;
}


void easygui_post_outbound_event_capture_mouse(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onCaptureMouse) {
            pElement->onCaptureMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_capture_mouse_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalCaptureMouse) {
            pElement->pContext->onGlobalCaptureMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_release_mouse(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onReleaseMouse) {
            pElement->onReleaseMouse(pElement);
        }
    }
}

void easygui_post_outbound_event_release_mouse_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalReleaseMouse) {
            pElement->pContext->onGlobalReleaseMouse(pElement);
        }
    }
}


void easygui_post_outbound_event_capture_keyboard(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onCaptureKeyboard) {
            pElement->onCaptureKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_capture_keyboard_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalCaptureKeyboard) {
            pElement->pContext->onGlobalCaptureKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_release_keyboard(easygui_element* pElement)
{
    if (pElement != NULL)
    {
        if (pElement->onReleaseKeyboard) {
            pElement->onReleaseKeyboard(pElement);
        }
    }
}

void easygui_post_outbound_event_release_keyboard_global(easygui_element* pElement)
{
    if (pElement != NULL && pElement->pContext != NULL)
    {
        if (pElement->pContext->onGlobalReleaseKeyboard) {
            pElement->pContext->onGlobalReleaseKeyboard(pElement);
        }
    }
}


void easygui_log(easygui_context* pContext, const char* message)
{
    if (pContext != NULL)
    {
        if (pContext->onLog) {
            pContext->onLog(pContext, message);
        }
    }
}


void easygui_draw_begin_null(void* pUserData)
{
    (void)pUserData;
}
void easygui_draw_end_null(void* pUserData)
{
    (void)pUserData;
}
void easygui_draw_clip_null(easygui_rect relativeRect, void* pUserData)
{
    (void)relativeRect;
    (void)pUserData;
}
void easygui_draw_line_null(float startX, float startY, float endX, float endY, float width, easygui_color color, void* pUserData)
{
    (void)startX;
    (void)startY;
    (void)endX;
    (void)endY;
    (void)width;
    (void)color;
    (void)pUserData;
}
void easygui_draw_rect_null(easygui_rect relativeRect, easygui_color color, void* pUserData)
{
    (void)relativeRect;
    (void)color;
    (void)pUserData;
}
void easygui_draw_text_null(const char* text, unsigned int textSizeInBytes, int posX, int posY, easygui_font font, easygui_color color, void* pUserData)
{
    (void)text;
    (void)textSizeInBytes;
    (void)posX;
    (void)posY;
    (void)font;
    (void)color;
    (void)pUserData;
}




/////////////////////////////////////////////////////////////////
//
// CORE API
//
/////////////////////////////////////////////////////////////////

easygui_context* easygui_create_context()
{
    easygui_context* pContext = malloc(sizeof(easygui_context));
    if (pContext != NULL) {
        pContext->paintingCallbacks.drawBegin = easygui_draw_begin_null;
        pContext->paintingCallbacks.drawEnd   = easygui_draw_end_null;
        pContext->paintingCallbacks.drawClip  = easygui_draw_clip_null;
        pContext->paintingCallbacks.drawLine  = easygui_draw_line_null;
        pContext->paintingCallbacks.drawRect  = easygui_draw_rect_null;
        pContext->paintingCallbacks.drawText  = easygui_draw_text_null;

#ifdef EASYGUI_USE_WIN32_THREADS
        HANDLE hInboundEventsMutex = CreateEvent(NULL, FALSE, TRUE, NULL);
        if (hInboundEventsMutex == NULL)
        {
            // Failed to create the mutex for inbound events.
            free(pContext);
            pContext = NULL;
        }

        pContext->inboundEventLock = hInboundEventsMutex;
#endif

#ifdef EASYGUI_USE_POSIX_THREADS
        pthread_mutex_t* pInboundEventsMutex = malloc(sizeof(pthread_mutex_t));
        if (pInboundEventsMutex == NULL)
        {
            // Failed to create the mutex for inbound events.
            free(pContext);
            pContext = NULL;
        }

        pthread_mutex_init(pInboundEventsMutex, NULL);
        pContext->inboundEventsMutex = pInboundEventsMutex;
#endif

        pContext->outboundEventLockCounter    = 0;
        pContext->pFirstDeadElement           = NULL;
        pContext->pElementUnderMouse          = NULL;
        pContext->pElementWithMouseCapture    = NULL;
        pContext->pElementWithKeyboardCapture = NULL;
        pContext->flags                       = 0;
        pContext->onGlobalCaptureMouse        = NULL;
        pContext->onGlobalReleaseMouse        = NULL;
        pContext->onGlobalCaptureKeyboard     = NULL;
        pContext->onGlobalReleaseKeyboard     = NULL;
        pContext->onLog                       = NULL;
    }

    return pContext;
}

void easygui_delete_context(easygui_context* pContext)
{
    // This is pure clean up. There is no event processing or whatnot here.

    if (pContext == NULL) {
        return;
    }


    // Make sure the mouse capture is released.
    if (pContext->pElementWithMouseCapture != NULL) {
        easygui_release_mouse(pContext);
    }

    // Make sure the keyboard capture is released.
    if (pContext->pElementWithKeyboardCapture != NULL) {
        easygui_release_keyboard(pContext);
    }


    if (easygui_is_inbound_events_locked(pContext))
    {
        // An inbound event is still being processed - we don't want to delete the context straight away because we can't
        // trust external event handlers to not try to access the context later on. To do this we just set the flag that
        // the context is deleted. It will then be deleted for real at the end of the inbound event handler.
        easygui_mark_context_as_dead(pContext);
    }
    else
    {
        // An inbound event is not being processed, so delete the context straight away.
        easygui_delete_context_for_real(pContext);
    }
}



/////////////////////////////////////////////////////////////////
// Events

easygui_bool easygui_post_inbound_event(easygui_event* pEvent)
{
    if (pEvent == NULL) {
        return EASYGUI_FALSE;
    }

    if (pEvent->pContext == NULL) {
        return EASYGUI_FALSE;
    }


    // Inbound events are not allowed to be called within an outbound event handler.
    assert(!easygui_is_outbound_events_locked(pEvent->pContext));

    
    easygui_bool result = EASYGUI_FALSE;
    easygui_lock_inbound_events(pEvent);
    {
        switch (pEvent->eventCode)
        {
            case easygui_event_mouse_enter:
            {
                result = easygui_handle_inbound_mouse_enter(pEvent);
            }

            case easygui_event_mouse_leave:
            {
                result = easygui_handle_inbound_mouse_leave(pEvent);
            }

            case easygui_event_mouse_move:
            {
                result = easygui_handle_inbound_mouse_move(pEvent);
            }

            case easygui_event_mouse_button_down:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_mouse_button_up:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_mouse_button_dblclick:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_mouse_wheel:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_key_down:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_key_up:
            {
                result = EASYGUI_TRUE;
            }

            case easygui_event_printable_key_down:
            {
                result = EASYGUI_TRUE;
            }

            default: break;
        }
    }
    easygui_unlock_inbound_events(pEvent);

    return result;
}


void easygui_register_global_on_capture_mouse(easygui_context* pContext, easygui_on_capture_mouse_proc onCaptureMouse)
{
    if (pContext != NULL) {
        pContext->onGlobalCaptureMouse = onCaptureMouse;
    }
}

void easygui_register_global_on_release_mouse(easygui_context* pContext, easygui_on_release_mouse_proc onReleaseMouse)
{
    if (pContext != NULL) {
        pContext->onGlobalReleaseMouse = onReleaseMouse;
    }
}

void easygui_register_global_on_capture_keyboard(easygui_context* pContext, easygui_on_capture_keyboard_proc onCaptureKeyboard)
{
    if (pContext != NULL) {
        pContext->onGlobalCaptureKeyboard = onCaptureKeyboard;
    }
}

void easygui_register_global_on_release_keyboard(easygui_context* pContext, easygui_on_capture_keyboard_proc onReleaseKeyboard)
{
    if (pContext != NULL) {
        pContext->onGlobalReleaseKeyboard = onReleaseKeyboard;
    }
}

void easygui_register_on_log(easygui_context* pContext, easygui_on_log onLog)
{
    if (pContext != NULL) {
        pContext->onLog = onLog;
    }
}



/////////////////////////////////////////////////////////////////
// Elements

easygui_element* easygui_create_element(easygui_context* pContext, easygui_element* pParent)
{
    if (pContext != NULL) {
        easygui_element* pElement = (easygui_element*)malloc(sizeof(easygui_element));
        if (pElement != NULL) {
            pElement->pContext     = pContext;
            pElement->pParent      = pParent;
            pElement->pFirstChild  = NULL;
            pElement->pLastChild   = NULL;
            pElement->pNextSibling = NULL;
            pElement->pPrevSibling = NULL;

            // Add to the the hierarchy.
            if (pElement->pParent != NULL) {
                if (pElement->pParent->pLastChild != NULL) {
                    pElement->pPrevSibling = pElement->pParent->pLastChild;
                    pElement->pPrevSibling->pNextSibling = pElement;
                }

                if (pElement->pParent->pFirstChild == NULL) {
                    pElement->pParent->pFirstChild = pElement;
                }

                pElement->pParent->pLastChild = pElement;
            }

            pElement->pNextDeadElement  = NULL;
            pElement->pUserData         = NULL;
            pElement->absolutePosX      = 0;
            pElement->absolutePosY      = 0;
            pElement->width             = 0;
            pElement->height            = 0;
            pElement->flags             = 0;
            pElement->onMouseMove       = NULL;
            pElement->onPaint           = NULL;
            pElement->onHitTest         = NULL;
            pElement->onCaptureMouse    = NULL;
            pElement->onReleaseMouse    = NULL;
            pElement->onCaptureKeyboard = NULL;
            pElement->onReleaseKeyboard = NULL;

            return pElement;
        }
    }

    return NULL;
}

void easygui_delete_element(easygui_element* pElement)
{
    // Orphan the element first.
    easygui_orphan_element(pElement);



    if (easygui_is_inbound_events_locked(pElement->pContext))
    {
        easygui_mark_element_as_dead(pElement);
    }
    else
    {
        easygui_delete_element_for_real(pElement);
    }
}


void* easygui_get_user_data(easygui_element* pElement)
{
    if (pElement != NULL) {
        return pElement->pUserData;
    }

    return NULL;
}

void easygui_set_user_data(easygui_element* pElement, void* pUserData)
{
    if (pElement != NULL) {
        pElement->pUserData = pUserData;
    }
}


void easygui_hide(easygui_element * pElement)
{
    if (pElement != NULL) {
        pElement->flags |= IS_ELEMENT_HIDDEN;
    }
}

void easygui_show(easygui_element * pElement)
{
    if (pElement != NULL) {
        pElement->flags &= ~IS_ELEMENT_HIDDEN;
    }
}

easygui_bool easygui_is_visible(const easygui_element * pElement)
{
    if (pElement != NULL) {
        return (pElement->flags & IS_ELEMENT_HIDDEN) == 0;
    }

    return EASYGUI_FALSE;
}

easygui_bool easygui_is_visible_recursive(const easygui_element * pElement)
{
    if (easygui_is_visible(pElement))
    {
        assert(pElement->pParent != NULL);

        if (pElement->pParent != NULL) {
            return easygui_is_visible(pElement->pParent);
        }
    }

    return EASYGUI_FALSE;
}


void easygui_disable_clipping(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags |= IS_ELEMENT_CLIPPING_DISABLED;
    }
}

void easygui_enable_clipping(easygui_element* pElement)
{
    if (pElement != NULL) {
        pElement->flags &= ~IS_ELEMENT_CLIPPING_DISABLED;
    }
}

easygui_bool easygui_is_clipping_enabled(const easygui_element* pElement)
{
    if (pElement != NULL) {
        return (pElement->flags & IS_ELEMENT_CLIPPING_DISABLED) == 0;
    }

    return EASYGUI_TRUE;
}



void easygui_capture_mouse(easygui_element* pElement)
{
    if (pElement == NULL) {
        return;
    }

    if (pElement->pContext == NULL) {
        return;
    }


    if (pElement->pContext->pElementWithMouseCapture != pElement)
    {
        // Release the previous capture first.
        if (pElement->pContext->pElementWithMouseCapture != NULL) {
            easygui_release_mouse(pElement->pContext);
        }

        assert(pElement->pContext->pElementWithMouseCapture == NULL);

        pElement->pContext->pElementWithMouseCapture = pElement;

        // Two events need to be posted - the global on_capture_mouse event and the local on_capture_mouse event.
        easygui_post_outbound_event_capture_mouse(pElement);
        easygui_post_outbound_event_capture_mouse_global(pElement);
    }
}

void easygui_release_mouse(easygui_context* pContext)
{
    if (pContext == NULL) {
        return;
    }


    // Events need to be posted before setting the internal pointer.
    easygui_post_outbound_event_release_mouse(pContext->pElementWithMouseCapture);
    easygui_post_outbound_event_release_mouse_global(pContext->pElementWithMouseCapture);


    pContext->pElementWithMouseCapture = NULL;
}


void easygui_capture_keyboard(easygui_element* pElement)
{
    if (pElement == NULL) {
        return;
    }

    if (pElement->pContext == NULL) {
        return;
    }


    if (pElement->pContext->pElementWithKeyboardCapture != pElement)
    {
        // Release the previous capture first.
        if (pElement->pContext->pElementWithKeyboardCapture != NULL) {
            easygui_release_keyboard(pElement->pContext);
        }

        assert(pElement->pContext->pElementWithKeyboardCapture == NULL);

        pElement->pContext->pElementWithKeyboardCapture = pElement;

        // Two events need to be posted - the global on_capture_mouse event and the local on_capture_mouse event.
        easygui_post_outbound_event_capture_keyboard(pElement);
        easygui_post_outbound_event_capture_keyboard_global(pElement);
    }
}

void easygui_release_keyboard(easygui_context* pContext)
{
    if (pContext == NULL) {
        return;
    }


    // Events need to be posted before setting the internal pointer.
    easygui_post_outbound_event_release_keyboard(pContext->pElementWithKeyboardCapture);
    easygui_post_outbound_event_release_keyboard_global(pContext->pElementWithKeyboardCapture);


    pContext->pElementWithKeyboardCapture = NULL;
}



//// Events ////

void easygui_register_on_mouse_enter(easygui_element* pElement, easygui_on_mouse_enter_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseEnter = callback;
    }
}

void easygui_register_on_mouse_leave(easygui_element* pElement, easygui_on_mouse_leave_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseLeave = callback;
    }
}

void easygui_register_on_mouse_move(easygui_element* pElement, easygui_on_mouse_move_proc callback)
{
    if (pElement != NULL) {
        pElement->onMouseMove = callback;
    }
}

void easygui_register_on_paint(easygui_element* pElement, easygui_on_paint_proc callback)
{
    if (pElement != NULL) {
        pElement->onPaint = callback;
    }
}

void easygui_register_on_hittest(easygui_element* pElement, easygui_on_hittest_proc callback)
{
    if (pElement != NULL) {
        pElement->onHitTest = callback;
    }
}

void easygui_register_on_capture_mouse(easygui_element* pElement, easygui_on_capture_mouse_proc callback)
{
    if (pElement != NULL) {
        pElement->onCaptureMouse = callback;
    }
}

void easygui_register_on_release_mouse(easygui_element* pElement, easygui_on_release_mouse_proc callback)
{
    if (pElement != NULL) {
        pElement->onReleaseMouse = callback;
    }
}

void easygui_register_on_capture_keyboard(easygui_element* pElement, easygui_on_capture_keyboard_proc callback)
{
    if (pElement != NULL) {
        pElement->onCaptureKeyboard = callback;
    }
}

void easygui_register_on_release_keyboard(easygui_element* pElement, easygui_on_release_keyboard_proc callback)
{
    if (pElement != NULL) {
        pElement->onReleaseKeyboard = callback;
    }
}



easygui_bool easygui_is_point_inside_element_bounds(const easygui_element* pElement, float absolutePosX, float absolutePosY)
{
    if (absolutePosX < pElement->absolutePosX ||
        absolutePosX < pElement->absolutePosY)
    {
        return EASYGUI_FALSE;
    }
    
    if (absolutePosX >= pElement->absolutePosX + pElement->width ||
        absolutePosY >= pElement->absolutePosY + pElement->height)
    {
        return EASYGUI_FALSE;
    }

    return EASYGUI_TRUE;
}

easygui_bool easygui_is_point_inside_element(easygui_element* pElement, float absolutePosX, float absolutePosY)
{
    if (easygui_is_point_inside_element_bounds(pElement, absolutePosX, absolutePosY))
    {
        // It is valid for onHitTest to be null, in which case we use the default hit test which assumes the element is just a rectangle
        // equal to the size of it's bounds. It's equivalent to onHitTest always returning true.

        if (pElement->onHitTest) {
            return pElement->onHitTest(pElement, absolutePosX - pElement->absolutePosX, absolutePosY - pElement->absolutePosY);
        }

        return EASYGUI_TRUE;
    }

    return EASYGUI_FALSE;
}



typedef struct
{
    easygui_element* pElementUnderPoint;
    float absolutePosX;
    float absolutePosY;
}easygui_find_element_under_point_data;

easygui_bool easygui_find_element_under_point_iterator(easygui_element* pElement, easygui_rect relativeVisibleRect, void* pUserData)
{
    assert(pElement != NULL);

    easygui_find_element_under_point_data* pData = pUserData;
    assert(pElement != NULL);

    float relativePosX = pData->absolutePosX;
    float relativePosY = pData->absolutePosY;
    easygui_make_point_relative_to_element(pElement, &relativePosX, &relativePosY);
    
    if (easygui_rect_contains_point(relativeVisibleRect, relativePosX, relativePosY))
    {
        if (pElement->onHitTest) {
            if (pElement->onHitTest(pElement, relativePosX, relativePosY)) {
                pData->pElementUnderPoint = pElement;
            }
        } else {
            pData->pElementUnderPoint = pElement;
        }
    }


    // Always return true to ensure the entire hierarchy is checked.
    return EASYGUI_TRUE;
}

easygui_element* easygui_find_element_under_point(easygui_element* pTopLevelElement, float absolutePosX, float absolutePosY)
{
    if (pTopLevelElement == NULL) {
        return NULL;
    }

    easygui_find_element_under_point_data data;
    data.pElementUnderPoint = NULL;
    data.absolutePosX = absolutePosX;
    data.absolutePosY = absolutePosY;
    easygui_iterate_visible_elements(pTopLevelElement, easygui_get_element_absolute_rect(pTopLevelElement), easygui_find_element_under_point_iterator, &data);

    return data.pElementUnderPoint;
}



//// Hierarchy ////

easygui_element* easygui_find_top_level_element(easygui_element* pElement)
{
    if (pElement == NULL) {
        return NULL;
    }

    if (pElement->pParent != NULL) {
        return easygui_find_top_level_element(pElement->pParent);
    }

    return pElement;
}



//// Layout ////

void easygui_set_element_relative_position(easygui_element* pElement, float relativePosX, float relativePosY)
{
    if (pElement != NULL) {
        pElement->absolutePosX = relativePosX;
        pElement->absolutePosY = relativePosY;

        if (pElement->pParent != NULL) {
            pElement->absolutePosX += pElement->pParent->absolutePosX;
            pElement->absolutePosY += pElement->pParent->absolutePosY;
        }
    }
}

float easygui_get_element_relative_position_x(const easygui_element* pElement)
{
    if (pElement != NULL) {
        if (pElement->pParent != NULL) {
            return pElement->absolutePosX - pElement->pParent->absolutePosX;
        } else {
            return pElement->absolutePosX;
        }
    }

    return 0;
}

float easygui_get_element_relative_position_y(const easygui_element* pElement)
{
    if (pElement != NULL) {
        if (pElement->pParent != NULL) {
            return pElement->absolutePosY - pElement->pParent->absolutePosY;
        } else {
            return pElement->absolutePosY;
        }
    }

    return 0;
}

void easygui_set_element_size(easygui_element* pElement, float width, float height)
{
    if (pElement != NULL) {
        pElement->width  = width;
        pElement->height = height;
    }
}


void easygui_get_element_size(const easygui_element* pElement, float* widthOut, float* heightOut)
{
    if (pElement != NULL) {
        if (widthOut != NULL) {
            *widthOut = pElement->width;
        }

        if (heightOut != NULL) {
            *heightOut = pElement->height;
        }
    }
}

float easygui_get_element_width(const easygui_element * pElement)
{
    if (pElement != NULL) {
        return pElement->width;
    }

    return 0;
}

float easygui_get_element_height(const easygui_element * pElement)
{
    if (pElement != NULL) {
        return pElement->height;
    }

    return 0;
}


easygui_rect easygui_get_element_absolute_rect(const easygui_element* pElement)
{
    easygui_rect rect;
    if (pElement == NULL)
    {
        rect.left   = pElement->absolutePosX;
        rect.top    = pElement->absolutePosY;
        rect.right  = rect.left + pElement->width;
        rect.bottom = rect.top + pElement->height;
    }
    else
    {
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = 0;
        rect.bottom = 0;
    }
    
    return rect;
}

easygui_rect easygui_get_element_relative_rect(const easygui_element* pElement)
{
    easygui_rect rect;
    if (pElement == NULL)
    {
        rect.left   = easygui_get_element_relative_position_x(pElement);
        rect.top    = easygui_get_element_relative_position_y(pElement);
        rect.right  = rect.left + pElement->width;
        rect.bottom = rect.top  + pElement->height;
    }
    else
    {
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = 0;
        rect.bottom = 0;
    }
    
    return rect;
}



//// Painting ////

void easygui_register_painting_callbacks(easygui_context* pContext, easygui_painting_callbacks callbacks)
{
    if (pContext == NULL) {
        return;
    }

    pContext->paintingCallbacks = callbacks;

    if (pContext->paintingCallbacks.drawBegin == NULL) {
        pContext->paintingCallbacks.drawBegin = easygui_draw_begin_null;
    }

    if (pContext->paintingCallbacks.drawEnd == NULL) {
        pContext->paintingCallbacks.drawEnd = easygui_draw_end_null;
    }

    if (pContext->paintingCallbacks.drawClip == NULL) {
        pContext->paintingCallbacks.drawClip = easygui_draw_clip_null;
    }

    if (pContext->paintingCallbacks.drawLine == NULL) {
        pContext->paintingCallbacks.drawLine = easygui_draw_line_null;
    }

    if (pContext->paintingCallbacks.drawRect == NULL) {
        pContext->paintingCallbacks.drawRect = easygui_draw_rect_null;
    }

    if (pContext->paintingCallbacks.drawText == NULL) {
        pContext->paintingCallbacks.drawText = easygui_draw_text_null;
    }
}


easygui_bool easygui_iterate_visible_elements(easygui_element* pParentElement, easygui_rect relativeRect, easygui_visible_iteration_proc callback, void* pUserData)
{
    if (pParentElement == NULL) {
        return EASYGUI_FALSE;
    }

    if (callback == NULL) {
        return EASYGUI_FALSE;
    }


    easygui_rect clampedRelativeRect = relativeRect;
    if (easygui_clamp_rect_to_element(pParentElement, &clampedRelativeRect))
    {
        // We'll only get here if some part of the rectangle was inside the element.
        if (!callback(pParentElement, clampedRelativeRect, pUserData)) {
            return EASYGUI_FALSE;
        }
    }

    for (easygui_element* pChild = pParentElement->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling)
    {
        float childRelativePosX = easygui_get_element_relative_position_x(pChild);
        float childRelativePosY = easygui_get_element_relative_position_y(pChild);

        easygui_rect childRect;
        if (easygui_is_clipping_enabled(pChild)) {
            childRect = clampedRelativeRect;
        } else {
            childRect = relativeRect;
        }


        childRect.left   -= childRelativePosX;
        childRect.top    -= childRelativePosY;
        childRect.right  -= childRelativePosX;
        childRect.bottom -= childRelativePosY;

        if (!easygui_iterate_visible_elements(pChild, childRect, callback, pUserData)) {
            return EASYGUI_FALSE;
        }
    }


    return EASYGUI_TRUE;
}


easygui_bool easygui_draw_iteration_callback(easygui_element* pElement, easygui_rect relativeRect, void* pUserData)
{
    assert(pElement != NULL);

    if (pElement->onPaint != NULL) {
        pElement->onPaint(pElement, relativeRect, pUserData);
    }

    return EASYGUI_TRUE;
}

void easygui_draw(easygui_element* pElement, easygui_rect relativeRect, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    easygui_context* pContext = pElement->pContext;
    if (pContext == NULL) {
        return;
    }

    assert(pContext->paintingCallbacks.drawBegin != NULL);
    assert(pContext->paintingCallbacks.drawEnd   != NULL);

    pContext->paintingCallbacks.drawBegin(pPaintData);
    {
        easygui_iterate_visible_elements(pElement, relativeRect, easygui_draw_iteration_callback, pPaintData);
    }
    pContext->paintingCallbacks.drawEnd(pPaintData);
}

void easygui_draw_rect(easygui_element* pElement, easygui_rect relativeRect, easygui_color color, void* pPaintData)
{
    if (pElement == NULL) {
        return;
    }

    assert(pElement->pContext != NULL);

    float offsetX = pElement->absolutePosX;
    float offsetY = pElement->absolutePosY;

    easygui_rect absoluteRect = relativeRect;
    absoluteRect.left   += offsetX;
    absoluteRect.top    += offsetY;
    absoluteRect.right  += offsetX;
    absoluteRect.bottom += offsetY;

    pElement->pContext->paintingCallbacks.drawRect(absoluteRect, color, pPaintData);
}



/////////////////////////////////////////////////////////////////
//
// HIGH-LEVEL API
//
/////////////////////////////////////////////////////////////////

easygui_bool easygui_post_inbound_event_mouse_move(easygui_element* pTopLevelElement, int mousePosX, int mousePosY)
{
    easygui_event e;
    e.pContext  = pTopLevelElement->pContext;
    e.pElement  = pTopLevelElement;
    e.eventCode = easygui_event_mouse_move;
    e.mouse_move.mousePosX = mousePosX;
    e.mouse_move.mousePosY = mousePosY;
    return easygui_post_inbound_event(&e);
}


//// Painting ////

void easygui_draw_border(easygui_element* pElement, float borderWidth, easygui_color color, void* pUserData)
{
    // TODO: In case alpha transparency is enabled, do not overlap the corners.

    easygui_rect borderLeft;
    borderLeft.left   = 0;
    borderLeft.right  = borderLeft.left + borderWidth;
    borderLeft.top    = 0;
    borderLeft.bottom = borderLeft.top + pElement->height;
    easygui_draw_rect(pElement, borderLeft, color, pUserData);

    easygui_rect borderTop;
    borderTop.left   = 0;
    borderTop.right  = borderTop.left + pElement->width;
    borderTop.top    = 0;
    borderTop.bottom = borderTop.top + borderWidth;
    easygui_draw_rect(pElement, borderTop, color, pUserData);

    easygui_rect borderRight;
    borderRight.left   = pElement->width - borderWidth;
    borderRight.right  = pElement->width;
    borderRight.top    = 0;
    borderRight.bottom = borderRight.top + pElement->height;
    easygui_draw_rect(pElement, borderRight, color, pUserData);

    easygui_rect borderBottom;
    borderBottom.left   = 0;
    borderBottom.right  = borderBottom.left + pElement->width;
    borderBottom.top    = pElement->height - borderWidth;
    borderBottom.bottom = pElement->height;
    easygui_draw_rect(pElement, borderBottom, color, pUserData);
}



/////////////////////////////////////////////////////////////////
//
// UTILITY API
//
/////////////////////////////////////////////////////////////////

easygui_color easygui_rgba(easygui_byte r, easygui_byte g, easygui_byte b, easygui_byte a)
{
    easygui_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return color;
}

easygui_color easygui_rgb(easygui_byte r, easygui_byte g, easygui_byte b)
{
    easygui_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;

    return color;
}

easygui_bool easygui_clamp_rect_to_element(const easygui_element* pElement, easygui_rect* pRelativeRect)
{
    if (pElement == NULL || pRelativeRect == NULL) {
        return EASYGUI_FALSE;
    }


    if (pRelativeRect->left < 0) {
        pRelativeRect->left = 0;
    }
    if (pRelativeRect->top < 0) {
        pRelativeRect->top = 0;
    }

    if (pRelativeRect->right > pElement->width) {
        pRelativeRect->right = pElement->width;
    }
    if (pRelativeRect->bottom > pElement->height) {
        pRelativeRect->bottom = pElement->height;
    }


    return (pRelativeRect->right - pRelativeRect->left > 0) && (pRelativeRect->bottom - pRelativeRect->top > 0);
}

void easygui_make_rect_relative_to_element(const easygui_element* pElement, easygui_rect* pRect)
{
    if (pElement == NULL || pRect == NULL) {
        return;
    }


    pRect->left   -= pElement->absolutePosX;
    pRect->top    -= pElement->absolutePosY;
    pRect->right  -= pElement->absolutePosX;
    pRect->bottom -= pElement->absolutePosY;
}

void easygui_make_point_relative_to_element(const easygui_element* pElement, float* positionX, float* positionY)
{
    if (pElement != NULL)
    {
        if (positionX != NULL) {
            *positionX -= pElement->absolutePosX;
        }

        if (positionY != NULL) {
            *positionY -= pElement->absolutePosY;
        }
    }
}

easygui_bool easygui_rect_contains_point(easygui_rect rect, float posX, float posY)
{
    if (posX < rect.left || posY < rect.top) {
        return EASYGUI_FALSE;
    }

    if (posX >= rect.right || posY >= rect.bottom) {
        return EASYGUI_FALSE;
    }

    return EASYGUI_TRUE;
}




/////////////////////////////////////////////////////////////////
//
// EASY_DRAW-SPECIFIC API
//
/////////////////////////////////////////////////////////////////
#ifndef EASYGUI_NO_EASY_DRAW

void easygui_draw_begin_easy_draw(void* pUserData);
void easygui_draw_end_easy_draw(void* pUserData);
void easygui_draw_rect_easy_draw(easygui_rect rect, easygui_color color, void* pUserData);

easygui_context* easygui_create_context_easy_draw()
{
    easygui_context* pContext = easygui_create_context();
    if (pContext != NULL) {
        easygui_register_easy_draw_callbacks(pContext);
    }

    return pContext;
}

void easygui_register_easy_draw_callbacks(easygui_context* pContext)
{
    easygui_painting_callbacks callbacks;
    callbacks.drawBegin = easygui_draw_begin_easy_draw;
    callbacks.drawEnd   = easygui_draw_end_easy_draw;
    callbacks.drawRect  = easygui_draw_rect_easy_draw;

    easygui_register_painting_callbacks(pContext, callbacks);
}


void easygui_draw_begin_easy_draw(void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_begin_draw(pSurface);
}

void easygui_draw_end_easy_draw(void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_end_draw(pSurface);
}

void easygui_draw_rect_easy_draw(easygui_rect rect, easygui_color color, void* pPaintData)
{
    easy2d_surface* pSurface = (easy2d_surface*)pPaintData;
    assert(pSurface != NULL);

    easy2d_draw_rect(pSurface, rect.left, rect.top, rect.right, rect.bottom, easy2d_rgba(color.r, color.g, color.b, color.a));
}

#endif

