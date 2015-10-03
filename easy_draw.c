// Public Domain. See "unlicense" statement at the end of this file.

#include "easy_draw.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

easy2d_context* easy2d_create_context(easy2d_drawing_callbacks drawingCallbacks, size_t contextExtraBytes, size_t surfaceExtraBytes)
{
    easy2d_context* pContext = (easy2d_context*)malloc(sizeof(easy2d_context) - sizeof(pContext->pExtraData) + contextExtraBytes);
    if (pContext != NULL) {
        pContext->drawingCallbacks  = drawingCallbacks;
        pContext->surfaceExtraBytes = surfaceExtraBytes;
        pContext->contextExtraBytes = contextExtraBytes;
        memset(pContext->pExtraData, 0, contextExtraBytes);

        // The create_context callback is optional. If it is set to NULL, we just finish up here and return successfully. Otherwise
        // we want to call the create_context callback and check it's return value. If it's return value if false it means there
        // was an error and we need to return null.
        if (pContext->drawingCallbacks.on_create_context != NULL)
        {
            if (!pContext->drawingCallbacks.on_create_context(pContext))
            {
                // An error was thrown from the callback.
                free(pContext);
                pContext = NULL;
            }
        }
    }

    return pContext;
}

void easy2d_delete_context(easy2d_context* pContext)
{
    if (pContext != NULL) {
        if (pContext->drawingCallbacks.on_delete_context != NULL) {
            pContext->drawingCallbacks.on_delete_context(pContext);
        }

        free(pContext);
    }
}

void* easy2d_get_context_extra_data(easy2d_context* pContext)
{
    return pContext->pExtraData;
}


easy2d_surface* easy2d_create_surface(easy2d_context* pContext, float width, float height)
{
    if (pContext != NULL) {
        easy2d_surface* pSurface = (easy2d_surface*)malloc(sizeof(easy2d_surface) - sizeof(pContext->pExtraData) + pContext->surfaceExtraBytes);
        if (pSurface != NULL) {
            pSurface->pContext = pContext;
            pSurface->width    = width;
            pSurface->height   = height;
            memset(pSurface->pExtraData, 0, pContext->surfaceExtraBytes);

            // The create_surface callback is optional. If it is set to NULL, we just finish up here and return successfully. Otherwise
            // we want to call the create_surface callback and check it's return value. If it's return value if false it means there
            // was an error and we need to return null.
            if (pContext->drawingCallbacks.on_create_surface != NULL)
            {
                if (!pContext->drawingCallbacks.on_create_surface(pSurface, width, height))
                {
                    free(pSurface);
                    pSurface = NULL;
                }
            }
        }

        return pSurface;
    }

    return NULL;
}

void easy2d_delete_surface(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.on_delete_surface != NULL) {
            pSurface->pContext->drawingCallbacks.on_delete_surface(pSurface);
        }

        free(pSurface);
    }
}

float easy2d_get_surface_width(const easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->width;
    }

    return 0;
}

float easy2d_get_surface_height(const easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->height;
    }

    return 0;
}

void* easy2d_get_surface_extra_data(easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        return pSurface->pExtraData;
    }

    return NULL;
}


void easy2d_begin_draw(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.begin_draw != NULL) {
            pSurface->pContext->drawingCallbacks.begin_draw(pSurface);
        }
    }
}

void easy2d_end_draw(easy2d_surface* pSurface)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.end_draw != NULL) {
            pSurface->pContext->drawingCallbacks.end_draw(pSurface);
        }
    }
}

void easy2d_draw_rect(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    if (pSurface != NULL)
    {
        assert(pSurface->pContext != NULL);

        if (pSurface->pContext->drawingCallbacks.draw_rect != NULL) {
            pSurface->pContext->drawingCallbacks.draw_rect(pSurface, left, top, right, bottom, color);
        }
    }
}




/////////////////////////////////////////////////////////////////
//
// UTILITY API
//
/////////////////////////////////////////////////////////////////

easy2d_color easy2d_rgba(easy2d_byte r, easy2d_byte g, easy2d_byte b, easy2d_byte a)
{
    easy2d_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;

    return color;
}

easy2d_color easy2d_rgb(easy2d_byte r, easy2d_byte g, easy2d_byte b)
{
    easy2d_color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = 255;

    return color;
}




/////////////////////////////////////////////////////////////////
//
// WINDOWS GDI 2D API
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_GDI

typedef struct
{
    /// The device context that owns every surface HBITMAP object. All drawing is done through this DC.
    HDC hDC;

}gdi_context_data;

typedef struct
{
    /// The bitmap to render to. This is created with GDI's CreateDIBSection() API, using the DC above.
    HBITMAP hBitmap;

    /// A pointer to the raw bitmap data. This is allocated CreateDIBSection().
    void* pBitmapData;

}gdi_surface_data;

easy2d_bool easy2d_on_create_context_gdi(easy2d_context* pContext);
void easy2d_on_delete_context_gdi(easy2d_context* pContext);
easy2d_bool easy2d_on_create_surface_gdi(easy2d_surface* pSurface, float width, float height);
void easy2d_on_delete_surface_gdi(easy2d_surface* pSurface);
void easy2d_begin_draw_gdi(easy2d_surface* pSurface);
void easy2d_end_draw_gdi(easy2d_surface* pSurface);
void easy2d_draw_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);


easy2d_context* easy2d_create_context_gdi()
{
    easy2d_drawing_callbacks callbacks;
    callbacks.on_create_context = easy2d_on_create_context_gdi;
    callbacks.on_delete_context = easy2d_on_delete_context_gdi;
    callbacks.on_create_surface = easy2d_on_create_surface_gdi;
    callbacks.on_delete_surface = easy2d_on_delete_surface_gdi;
    callbacks.begin_draw     = easy2d_begin_draw_gdi;
    callbacks.end_draw       = easy2d_end_draw_gdi;
    callbacks.draw_rect      = easy2d_draw_rect_gdi;

    return easy2d_create_context(callbacks, sizeof(gdi_context_data), sizeof(gdi_surface_data));
}

HDC easy2d_get_HDC(easy2d_context* pContext)
{
    if (pContext != NULL) {
        gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
        if (pGDIData != NULL) {
            return pGDIData->hDC;
        }
    }

    return NULL;
}

HBITMAP easy2d_get_HBITMAP(easy2d_surface* pSurface)
{
    if (pSurface != NULL) {
        gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
        if (pGDIData != NULL) {
            return pGDIData->hBitmap;
        }
    }

    return NULL;
}


easy2d_bool easy2d_on_create_context_gdi(easy2d_context* pContext)
{
    assert(pContext != NULL);

    // We need to create the DC that all of our rendering commands will be drawn to.
    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData == NULL) {
        return EASY2D_FALSE;
    }

    pGDIData->hDC = CreateCompatibleDC(GetDC(GetDesktopWindow()));
    if (pGDIData->hDC == NULL) {
        return EASY2D_FALSE;
    }


    // No pen by default.
    SelectObject(pGDIData->hDC, GetStockObject(NULL_PEN));

    // The normal brush by default.
    SelectObject(pGDIData->hDC, GetStockObject(DC_BRUSH));

    // Don't draw the default text background.
    SetBkMode(pGDIData->hDC, TRANSPARENT);


    return EASY2D_TRUE;
}

void easy2d_on_delete_context_gdi(easy2d_context* pContext)
{
    assert(pContext != NULL);

    gdi_context_data* pGDIData = easy2d_get_context_extra_data(pContext);
    if (pGDIData != NULL)
    {
        DeleteDC(pGDIData->hDC);
        pGDIData->hDC = NULL;
    }
}

easy2d_bool easy2d_on_create_surface_gdi(easy2d_surface* pSurface, float width, float height)
{
    assert(pSurface != NULL);

    gdi_context_data* pGDIContextData = easy2d_get_context_extra_data(pSurface->pContext);
    if (pGDIContextData == NULL) {
        return EASY2D_FALSE;
    }

    gdi_surface_data* pGDISurfaceData = easy2d_get_surface_extra_data(pSurface);
    if (pGDISurfaceData == NULL) {
        return EASY2D_FALSE;
    }

    HDC hDC = pGDIContextData->hDC;
    if (hDC == NULL) {
        return EASY2D_FALSE;
    }


    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = (LONG)width;
    bmi.bmiHeader.biHeight      = (LONG)height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    pGDISurfaceData->hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, &pGDISurfaceData->pBitmapData, NULL, 0);
    if (pGDISurfaceData->hBitmap == NULL) {
        return EASY2D_FALSE;
    }

    return EASY2D_TRUE;
}

void easy2d_on_delete_surface_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        DeleteObject(pGDIData->hBitmap);
        pGDIData->hBitmap = NULL;
    }
}


void easy2d_begin_draw_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL) {
        SelectObject(easy2d_get_HDC(pSurface->pContext), pGDIData->hBitmap);
    }
}

void easy2d_end_draw_gdi(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);
    (void)pSurface;
}

void easy2d_draw_rect_gdi(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    assert(pSurface != NULL);

    gdi_surface_data* pGDIData = easy2d_get_surface_extra_data(pSurface);
    if (pGDIData != NULL)
    {
        HDC hDC = easy2d_get_HDC(pSurface->pContext);

        // Set the color.
        SetDCBrushColor(hDC, RGB(color.r, color.g, color.b));

        // Now draw the rectangle. The documentation for this says that the width and height is 1 pixel less when the pen is null. Therefore we will
        // increase the width and height by 1 since we have got the pen set to null.
        Rectangle(hDC, (int)left, (int)top, (int)right + 1, (int)bottom + 1);
    }
}

#endif  // GDI





/////////////////////////////////////////////////////////////////
//
// CAIRO 2D API
//
/////////////////////////////////////////////////////////////////
#ifndef EASY2D_NO_CAIRO

typedef struct
{
    cairo_surface_t* pCairoSurface;
    cairo_t* pCairoContext;

}cairo_surface_data;

easy2d_bool easy2d_on_create_context_cairo(easy2d_context* pContext);
void easy2d_on_delete_context_cairo(easy2d_context* pContext);
easy2d_bool easy2d_on_create_surface_cairo(easy2d_surface* pSurface, float width, float height);
void easy2d_on_delete_surface_cairo(easy2d_surface* pSurface);
void easy2d_begin_draw_cairo(easy2d_surface* pSurface);
void easy2d_end_draw_cairo(easy2d_surface* pSurface);
void easy2d_draw_rect_cairo(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color);


easy2d_context* easy2d_create_context_cairo()
{
    easy2d_drawing_callbacks callbacks;
    callbacks.on_create_context = easy2d_on_create_context_cairo;
    callbacks.on_delete_context = easy2d_on_delete_context_cairo;
    callbacks.on_create_surface = easy2d_on_create_surface_cairo;
    callbacks.on_delete_surface = easy2d_on_delete_surface_cairo;
    callbacks.begin_draw        = easy2d_begin_draw_cairo;
    callbacks.end_draw          = easy2d_end_draw_cairo;
    callbacks.draw_rect         = easy2d_draw_rect_cairo;

    return easy2d_create_context(callbacks, 0, sizeof(cairo_surface_data));
}

cairo_surface_t* easy2d_get_cairo_surface_t(easy2d_surface* pSurface)
{
    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL) {
        return pCairoData->pCairoSurface;
    }

    return NULL;
}

cairo_t* easy2d_get_cairo_t(easy2d_surface* pSurface)
{
    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL) {
        return pCairoData->pCairoContext;
    }

    return NULL;
}


easy2d_bool easy2d_on_create_context_cairo(easy2d_context* pContext)
{
    assert(pContext != NULL);
    (void)pContext;

    return EASY2D_TRUE;
}

void easy2d_on_delete_context_cairo(easy2d_context* pContext)
{
    assert(pContext != NULL);
    (void)pContext;
}

easy2d_bool easy2d_on_create_surface_cairo(easy2d_surface* pSurface, float width, float height)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData == NULL) {
        return EASY2D_FALSE;
    }

    pCairoData->pCairoSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)width, (int)height);
    if (pCairoData->pCairoSurface == NULL) {
        return EASY2D_FALSE;
    }

    pCairoData->pCairoContext = cairo_create(pCairoData->pCairoSurface);
    if (pCairoData->pCairoContext == NULL) {
        cairo_surface_destroy(pCairoData->pCairoSurface);
        return EASY2D_FALSE;
    }


    return EASY2D_TRUE;
}

void easy2d_on_delete_surface_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
        cairo_destroy(pCairoData->pCairoContext);
        cairo_surface_destroy(pCairoData->pCairoSurface);
    }
}


void easy2d_begin_draw_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
    }
}

void easy2d_end_draw_cairo(easy2d_surface* pSurface)
{
    assert(pSurface != NULL);
    (void)pSurface;
}

void easy2d_draw_rect_cairo(easy2d_surface* pSurface, float left, float top, float right, float bottom, easy2d_color color)
{
    assert(pSurface != NULL);

    cairo_surface_data* pCairoData = easy2d_get_surface_extra_data(pSurface);
    if (pCairoData != NULL)
    {
        cairo_set_source_rgba(pCairoData->pCairoContext, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
        cairo_rectangle(pCairoData->pCairoContext, left, top, (right - left), (bottom - top));
        cairo_fill(pCairoData->pCairoContext);
    }
}


#endif  // Cairo














