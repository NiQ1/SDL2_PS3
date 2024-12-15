/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_RENDER_PS3

#include "../SDL_sysrender.h"
#include "../../video/SDL_sysvideo.h"
#include "../../video/ps3/SDL_PS3_mm.h"
#include "../../video/ps3/SDL_PS3video.h"

#include "../software/SDL_draw.h"
#include "../software/SDL_blendfillrect.h"
#include "../software/SDL_blendline.h"
#include "../software/SDL_blendpoint.h"
#include "../software/SDL_drawline.h"
#include "../software/SDL_drawpoint.h"

#include <cell/gcm.h>
#include <unistd.h>
#include <assert.h>
#include <sys/timer.h>

/* SDL surface based renderer implementation */

static SDL_Renderer *PS3_CreateRenderer(SDL_Window * window, Uint32 flags);
static void PS3_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event);
static SDL_bool PS3_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode);
static int PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * rect, const void *pixels,
                            int pitch);
static int PS3_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect,
                     const Uint8 *Yplane, int Ypitch,
                     const Uint8 *Uplane, int Upitch,
                     const Uint8 *Vplane, int Vpitch);
static int PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                          const SDL_Rect * rect, void **pixels, int *pitch);
static void PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int PS3_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture);
static int PS3_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd);
static int PS3_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd);
static void PS3_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode);
static int PS3_UpdateViewport(SDL_Renderer * renderer);
static int PS3_RenderClear(SDL_Renderer * renderer);
static int PS3_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count);
static int PS3_RenderDrawPoints(SDL_Renderer * renderer,
                               const SDL_Point * points, int count);
static int PS3_RenderDrawLines(SDL_Renderer * renderer,
                              const SDL_Point * points, int count);
static int PS3_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count);
static int PS3_RenderFillRects(SDL_Renderer * renderer,
                              const SDL_Rect * rects, int count);
static int PS3_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect);
static int PS3_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                         const SDL_Rect * srcrect, const SDL_Rect * dstrect);
static int PS3_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                const SDL_Rect * srcquad, const SDL_FRect * dstrect,
                const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
static int PS3_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize);
static int PS3_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                               Uint32 format, void * pixels, int pitch);
static void PS3_RenderPresent(SDL_Renderer * renderer);
static void PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static void PS3_DestroyRenderer(SDL_Renderer * renderer);


SDL_RenderDriver PS3_RenderDriver = {
    PS3_CreateRenderer,
    {
     "PS3",
     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC,
     1,
     { SDL_PIXELFORMAT_ARGB8888 },
     0,
     0}
};

typedef struct
{
    bool first_fb; // Is this the first flip ?
    int current_screen;
    SDL_Surface *screens[3];
    void *textures[3];
    CellGcmContextData *context; // Context to keep track of the RSX buffer.
} PS3_RenderData;

typedef struct
{
    SDL_Rect    srcRect;
    SDL_Rect   dstRect;
} PS3_CopyData;


static void waitFlip()
{
    while (cellGcmGetFlipStatus() != 0)
      sys_timer_usleep(200);
    cellGcmResetFlipStatus();
}

static SDL_Surface *
PS3_ActivateRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->driverdata;

    return data->screens[data->current_screen];
}

SDL_Renderer *
PS3_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Renderer *renderer;
    PS3_RenderData *data;
    SDL_VideoDisplay *display = SDL_GetDisplayForWindow(window);
    SDL_DisplayMode *displayMode = &display->current_mode;
    int i, n;
    int bpp;
    int pitch;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!SDL_PixelFormatEnumToMasks(displayMode->format, &bpp,
                                    &Rmask, &Gmask, &Bmask, &Amask)) {
        SDL_SetError("Unknown display format");
        return NULL;
    }

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }

    data = (PS3_RenderData *) SDL_calloc(1, sizeof(*data));
    if (!data) {
        PS3_DestroyRenderer(renderer);
        SDL_OutOfMemory();
        return NULL;
    }
    
    SDL_zerop(data);
    
    deprintf (1, "\tMem allocated\n");
    
    // Get a copy of the command buffer
    data->context = ((SDL_DeviceData*) display->device->driverdata)->_CommandBuffer;
    data->current_screen = 0;
    data->first_fb = true;
    
    pitch = displayMode->w * SDL_BYTESPERPIXEL(displayMode->format);
    
    n = 2;
    deprintf (1, "\tCreate the %d screen(s):\n", n);
    for (i = 0; i < n; ++i) {
        deprintf (1,  "\t\tAllocate RSX memory for pixels\n");
        /* Allocate RSX memory for pixels */
        data->textures[i] = rsxMemalign(64, displayMode->h * pitch);
        if (!data->textures[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }

        memset(data->textures[i], 0, displayMode->h * pitch);

        deprintf (1,  "\t\tSDL_CreateRGBSurfaceFrom( w: %d, h: %d)\n", displayMode->w, displayMode->h);
        data->screens[i] =
            SDL_CreateRGBSurfaceFrom(data->textures[i], displayMode->w, displayMode->h, bpp, pitch, Rmask, Gmask,
                                 Bmask, Amask);
        if (!data->screens[i]) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            return NULL;
        }

        u32 offset = 0;
        deprintf (1,  "\t\tPrepare RSX offsets (%p, %p) \n", data->screens[i]->pixels, &offset);
        if (cellGcmAddressToOffset(data->screens[i]->pixels, &offset) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }
        
        deprintf (1,  "\t\tSetup the display buffers\n");
        // Setup the display buffers
        if (cellGcmSetDisplayBuffer(i, offset, data->screens[i]->pitch, data->screens[i]->w, data->screens[i]->h) != 0) {
            deprintf (1, "ERROR\n");
            PS3_DestroyRenderer(renderer);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    deprintf (1,  "\tFinished\n");

    renderer->WindowEvent = PS3_WindowEvent;
    renderer->SupportsBlendMode = PS3_SupportsBlendMode;
    renderer->CreateTexture = PS3_CreateTexture;
    renderer->UpdateTexture = PS3_UpdateTexture;
    renderer->UpdateTextureYUV = PS3_UpdateTextureYUV;
    renderer->LockTexture = PS3_LockTexture;
    renderer->UnlockTexture = PS3_UnlockTexture;
    renderer->SetTextureScaleMode = PS3_SetTextureScaleMode;
    renderer->SetRenderTarget = PS3_SetRenderTarget;
    renderer->QueueSetViewport = PS3_QueueSetViewport;
    renderer->QueueSetDrawColor = PS3_QueueSetDrawColor;
    renderer->QueueDrawPoints = PS3_QueueDrawPoints;
    renderer->QueueDrawLines = PS3_QueueDrawPoints;  // lines and points queue the same way.
    renderer->QueueFillRects = PS3_QueueFillRects;
    renderer->QueueCopy = PS3_QueueCopy;
    renderer->QueueCopyEx = PS3_QueueCopyEx;
    renderer->RunCommandQueue = PS3_RunCommandQueue;
    renderer->RenderReadPixels = PS3_RenderReadPixels;
    renderer->RenderPresent = PS3_RenderPresent;
    renderer->DestroyTexture = PS3_DestroyTexture;
    renderer->DestroyRenderer = PS3_DestroyRenderer;
    renderer->info = PS3_RenderDriver.info;
    renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    renderer->driverdata = data;
    renderer->window = window;

    PS3_UpdateViewport(renderer);
    PS3_ActivateRenderer(renderer);
    
    return renderer;
}

static void
PS3_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
}

static SDL_bool
PS3_SupportsBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
    return SDL_FALSE;
}

static int
PS3_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    int bpp;
    int pitch;
    void *pixels;
    Uint32 Rmask, Gmask, Bmask, Amask;

    if (!SDL_PixelFormatEnumToMasks
        (texture->format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
        SDL_SetError("Unknown texture format");
        return -1;
    }

    // Allocate GFX memory for textures
    pitch = texture->w * SDL_BYTESPERPIXEL(texture->format);
    pixels = rsxMemalign(64, texture->h * pitch);

    texture->driverdata =
        SDL_CreateRGBSurfaceFrom(pixels, texture->w, texture->h, bpp, pitch,
                            Rmask, Gmask, Bmask, Amask);

    SDL_SetSurfaceColorMod(texture->driverdata, texture->r, texture->g,
                           texture->b);
    SDL_SetSurfaceAlphaMod(texture->driverdata, texture->a);
    SDL_SetSurfaceBlendMode(texture->driverdata, texture->blendMode);

    if (!texture->driverdata) {
        return -1;
    }
    return 0;
}

static int
PS3_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * rect, const void *pixels, int pitch)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;
    Uint8 *src, *dst;
    int row;
    size_t length;

    src = (Uint8 *) pixels;
    dst = (Uint8 *) surface->pixels +
                        rect->y * surface->pitch +
                        rect->x * surface->format->BytesPerPixel;
    length = rect->w * surface->format->BytesPerPixel;
    for (row = 0; row < rect->h; ++row) {
        SDL_memcpy(dst, src, length);
        src += pitch;
        dst += surface->pitch;
    }
    return 0;
}

static int 
PS3_UpdateTextureYUV(SDL_Renderer * renderer, SDL_Texture * texture,
                     const SDL_Rect * rect,
                     const Uint8 *Yplane, int Ypitch,
                     const Uint8 *Uplane, int Upitch,
                     const Uint8 *Vplane, int Vpitch)
{
    return 0;
}

static int
PS3_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
               const SDL_Rect * rect, void **pixels, int *pitch)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

    *pixels =
        (void *) ((Uint8 *) surface->pixels + rect->y * surface->pitch +
                  rect->x * surface->format->BytesPerPixel);
    *pitch = surface->pitch;
    return 0;
}

static void
PS3_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
}

static int 
PS3_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{
    return 0;
}

static int
PS3_QueueSetViewport(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;
}

static int
PS3_QueueSetDrawColor(SDL_Renderer * renderer, SDL_RenderCommand *cmd)
{
    return 0;
}

static void
PS3_SetTextureScaleMode(SDL_Renderer * renderer, SDL_Texture * texture, SDL_ScaleMode scaleMode)
{
    
}

static int
PS3_UpdateViewport(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->driverdata;
    SDL_Surface *surface = data->screens[0];

    if (!renderer->viewport.w && !renderer->viewport.h) {
        /* There may be no window, so update the viewport directly */
        renderer->viewport.w = surface->w;
        renderer->viewport.h = surface->h;
    }

    /* Center drawable region on screen */
    if (renderer->window && surface->w > renderer->window->w) {
        renderer->viewport.x += (surface->w - renderer->window->w)/2;
    }
    if (renderer->window && surface->h > renderer->window->h) {
        renderer->viewport.y += (surface->h - renderer->window->h)/2;
    }
    
    SDL_SetClipRect(data->screens[0], &renderer->viewport);
    SDL_SetClipRect(data->screens[1], &renderer->viewport);
    return 0;
}

static int
PS3_RenderClear(SDL_Renderer * renderer)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    Uint32 color;
    SDL_Rect clip_rect;

    if (!surface) {
        return -1;
    }

    color = SDL_MapRGBA(surface->format,
                        renderer->r, renderer->g, renderer->b, renderer->a);

    /* By definition the clear ignores the clip rect */
    clip_rect = surface->clip_rect;
    SDL_SetClipRect(surface, NULL);
    SDL_FillRect(surface, NULL, color);
    SDL_SetClipRect(surface, &clip_rect);
    return 0;
}

static int
PS3_QueueDrawPoints(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FPoint * points, int count)
{
    const size_t vertlen = sizeof(SDL_Point) * count;
    SDL_Point *verts = (SDL_Point *) SDL_AllocateRenderVertices(renderer, vertlen, 0, &cmd->data.draw.first);
    if (!verts) {
        return -1;
    }
    cmd->data.draw.count = count;
    for (int i = 0; i < count; ++i)
    {
        verts[i].x = points[i].x;
        verts[i].y = points[i].y;
    }
    return 0;
}

static int
PS3_RenderDrawPoints(SDL_Renderer * renderer, const SDL_Point * points,
                    int count)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    SDL_Point *temp = NULL;
    int status;

    if (!surface) {
        return -1;
    }

    if (renderer->viewport.x || renderer->viewport.y) {
        int i;
        int x = renderer->viewport.x;
        int y = renderer->viewport.y;

        temp = SDL_stack_alloc(SDL_Point, count);
        for (i = 0; i < count; ++i) {
            temp[i].x = x + points[i].x;
            temp[i].y = y + points[i].x;
        }
        points = temp;
    }

    /* Draw the points! */
    if (renderer->blendMode == SDL_BLENDMODE_NONE) {
        Uint32 color = SDL_MapRGBA(surface->format,
                                   renderer->r, renderer->g, renderer->b,
                                   renderer->a);

        status = SDL_DrawPoints(surface, points, count, color);
    } else {
        status = SDL_BlendPoints(surface, points, count,
                                renderer->blendMode,
                                renderer->r, renderer->g, renderer->b,
                                renderer->a);
    }

    if (temp) {
        SDL_stack_free(temp);
    }
    return status;
}

static int
PS3_RenderDrawLines(SDL_Renderer * renderer, const SDL_Point * points,
                   int count)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    SDL_Point *temp = NULL;
    int status;

    if (!surface) {
        return -1;
    }

    if (renderer->viewport.x || renderer->viewport.y) {
        int i;
        int x = renderer->viewport.x;
        int y = renderer->viewport.y;

        temp = SDL_stack_alloc(SDL_Point, count);
        for (i = 0; i < count; ++i) {
            temp[i].x = x + points[i].x;
            temp[i].y = y + points[i].y;
        }
        points = temp;
    }

    /* Draw the lines! */
    if (renderer->blendMode == SDL_BLENDMODE_NONE) {
        Uint32 color = SDL_MapRGBA(surface->format,
                                   renderer->r, renderer->g, renderer->b,
                                   renderer->a);

        status = SDL_DrawLines(surface, points, count, color);
    } else {
        status = SDL_BlendLines(surface, points, count,
                                renderer->blendMode,
                                renderer->r, renderer->g, renderer->b,
                                renderer->a);
    }

    if (temp) {
        SDL_stack_free(temp);
    }
    return status;
}

static int 
PS3_QueueFillRects(SDL_Renderer * renderer, SDL_RenderCommand *cmd, const SDL_FRect * rects, int count)
{
    const size_t outLen = count * sizeof (SDL_Rect);
    SDL_Rect *outRects = (SDL_Rect *) SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);
    
    if (!outRects) {
        return -1;
    }
    cmd->data.draw.count = count;
    for (int i = 0; i < count; ++i)
    {
        outRects[i].x = rects[i].x;
        outRects[i].y = rects[i].y;
        outRects[i].w = rects[i].w;
        outRects[i].h = rects[i].h;
    }
    
    SDL_memcpy(outRects, rects, outLen);

    return 0;
}

static int
PS3_RenderFillRects(SDL_Renderer * renderer, const SDL_Rect * rects, int count)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    SDL_Rect *temp = NULL;
    int status;

    if (!surface) {
        return -1;
    }

    if (renderer->viewport.x || renderer->viewport.y) {
        int i;
        int x = renderer->viewport.x;
        int y = renderer->viewport.y;

        temp = SDL_stack_alloc(SDL_Rect, count);
        for (i = 0; i < count; ++i) {
            temp[i].x = x + rects[i].x;
            temp[i].y = y + rects[i].y;
            temp[i].w = rects[i].w;
            temp[i].h = rects[i].h;
        }
        rects = temp;
    }

    if (renderer->blendMode == SDL_BLENDMODE_NONE) {
        Uint32 color = SDL_MapRGBA(surface->format,
                                   renderer->r, renderer->g, renderer->b,
                                   renderer->a);
        status = SDL_FillRects(surface, rects, count, color);
    } else {
        status = SDL_BlendFillRects(surface, rects, count,
                                    renderer->blendMode,
                                    renderer->r, renderer->g, renderer->b,
                                    renderer->a);
    }

    if (temp) {
        SDL_stack_free(temp);
    }
    return status;
}

static int 
PS3_QueueCopy(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    const size_t outLen = sizeof (PS3_CopyData);
    PS3_CopyData *outData = (PS3_CopyData *) SDL_AllocateRenderVertices(renderer, outLen, 0, &cmd->data.draw.first);
    
    if (!outData) {
        return -1;
    }
    cmd->data.draw.count = 1;
    
    SDL_memcpy(&outData->srcRect, srcrect, sizeof(SDL_Rect));
    
    outData->dstRect.x = dstrect->x;
    outData->dstRect.y = dstrect->y;
    outData->dstRect.w = dstrect->w;
    outData->dstRect.h = dstrect->h;

    return 0;
}

static int
PS3_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
              const SDL_Rect * srcrect, const SDL_Rect * dstrect)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->driverdata;
    SDL_Surface *dst = PS3_ActivateRenderer(renderer);
    SDL_Surface *src = (SDL_Surface *) texture->driverdata;
    SDL_Rect final_rect = *dstrect;
    u32 src_offset, dst_offset;

    if (!dst) {
        return -1;
    }

    if (renderer->viewport.x || renderer->viewport.y) {
        final_rect.x += renderer->viewport.x;
        final_rect.y += renderer->viewport.y;
    }

    cellGcmAddressToOffset(dst->pixels, &dst_offset);
    cellGcmAddressToOffset(src->pixels, &src_offset);

    CellGcmTransferScale scale;
    scale.conversion = CELL_GCM_TRANSFER_CONVERSION_TRUNCATE;
    scale.format = CELL_GCM_TRANSFER_SCALE_FORMAT_A8R8G8B8;
    scale.operation = CELL_GCM_TRANSFER_OPERATION_SRCCOPY;
    scale.clipX = final_rect.x;
    scale.clipY = final_rect.y;
    scale.clipW = final_rect.w;
    scale.clipH = final_rect.h;
    scale.outX = final_rect.x;
    scale.outY = final_rect.y;
    scale.outW = final_rect.w;
    scale.outH = final_rect.h;
    scale.ratioX = (srcrect->w << 20) / final_rect.w;
    scale.ratioY = (srcrect->h << 20) / final_rect.h;
    scale.inX = srcrect->x;
    scale.inY = srcrect->y;
    scale.inW = srcrect->w;
    scale.inH = srcrect->h;
    scale.offset = src_offset;
    scale.pitch = src->pitch;
    scale.origin = CELL_GCM_TRANSFER_ORIGIN_CORNER;
    scale.interp = CELL_GCM_TRANSFER_INTERPOLATOR_ZOH;

    CellGcmTransferSurface surface;
    surface.format = CELL_GCM_TRANSFER_SURFACE_FORMAT_A8R8G8B8;
    surface.pitch = dst->pitch;
    surface.offset = dst_offset;

    // Hardware accelerated blit with scaling
    cellGcmSetTransferScaleMode(data->context, CELL_GCM_TRANSFER_LOCAL_TO_LOCAL, CELL_GCM_TRANSFER_SURFACE);
    cellGcmSetTransferScaleSurface(data->context, &scale, &surface);

    // TODO: Blending / clipping

    return 0;
}

static int
PS3_QueueCopyEx(SDL_Renderer * renderer, SDL_RenderCommand *cmd, SDL_Texture * texture,
                const SDL_Rect * srcquad, const SDL_FRect * dstrect,
                const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
    return 0;
}

static int
PS3_RunCommandQueue(SDL_Renderer * renderer, SDL_RenderCommand *cmd, void *vertices, size_t vertsize)
{
    while (cmd) {
        switch (cmd->command) {
            case SDL_RENDERCMD_SETDRAWCOLOR: {
                break;
            }

            case SDL_RENDERCMD_SETVIEWPORT: {
                break;
            }

            case SDL_RENDERCMD_SETCLIPRECT: {
                break;
            }

            case SDL_RENDERCMD_CLEAR: {
                PS3_RenderClear(renderer);
                break;
            }

            case SDL_RENDERCMD_DRAW_POINTS: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_Point *points = (SDL_Point *) (((Uint8 *) vertices) + first);
                PS3_RenderDrawPoints(renderer, points, count);
                break;
            }

            case SDL_RENDERCMD_DRAW_LINES: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_Point *points = (SDL_Point *) (((Uint8 *) vertices) + first);

                PS3_RenderDrawLines(renderer, points, count);
                break;
            }

            case SDL_RENDERCMD_FILL_RECTS: {
                const size_t count = cmd->data.draw.count;
                const size_t first = cmd->data.draw.first;
                const SDL_Rect *rects = (SDL_Rect *) (((Uint8 *) vertices) + first);

                PS3_RenderFillRects(renderer, rects, count);
                break;
            }

            case SDL_RENDERCMD_COPY: {
                const size_t first = cmd->data.draw.first;
                const PS3_CopyData *copyData = (PS3_CopyData *) (((Uint8 *) vertices) + first);

                PS3_RenderCopy(renderer, cmd->data.draw.texture, &copyData->srcRect, &copyData->dstRect);
                break;
            }

            case SDL_RENDERCMD_COPY_EX: {
                break;
            }

            case SDL_RENDERCMD_NO_OP:
                break;
        }

        cmd = cmd->next;
    }

    return 0;
}

static int
PS3_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                    Uint32 format, void * pixels, int pitch)
{
    SDL_Surface *surface = PS3_ActivateRenderer(renderer);
    Uint32 src_format;
    void *src_pixels;
    SDL_Rect final_rect;

    if (!surface) {
        return -1;
    }

    if (renderer->viewport.x || renderer->viewport.y) {
        final_rect.x = renderer->viewport.x + rect->x;
        final_rect.y = renderer->viewport.y + rect->y;
        final_rect.w = rect->w;
        final_rect.h = rect->h;
        rect = &final_rect;
    }

    if (rect->x < 0 || rect->x+rect->w > surface->w ||
        rect->y < 0 || rect->y+rect->h > surface->h) {
        SDL_SetError("Tried to read outside of surface bounds");
        return -1;
    }

    src_format = surface->format->format;
    src_pixels = (void*)((Uint8 *) surface->pixels +
                    rect->y * surface->pitch +
                    rect->x * surface->format->BytesPerPixel);

    return SDL_ConvertPixels(rect->w, rect->h,
                             src_format, src_pixels, surface->pitch,
                             format, pixels, pitch);
}

static void
PS3_RenderPresent(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->driverdata;

    if (data->first_fb)
    {
        cellGcmResetFlipStatus();
    }

    cellGcmSetFlip(data->context, data->current_screen);
    rsxFlushBuffer(data->context);

    cellGcmSetWaitFlip(data->context);

    waitFlip();

    data->first_fb = false;

    // Update the flipping chain, if any
    data->current_screen = (data->current_screen + 1) % 2;
}

static void
PS3_DestroyTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    SDL_Surface *surface = (SDL_Surface *) texture->driverdata;

    if (!surface)
    {
        return;
    }

    // TODO: Wait for the DMA transfer to complete
    rsxFree(surface->pixels);
    SDL_FreeSurface(surface);
}

static void
PS3_DestroyRenderer(SDL_Renderer * renderer)
{
    PS3_RenderData *data = (PS3_RenderData *) renderer->driverdata;
    int i;

    deprintf (1, "SDL_PS3_DestroyRenderer()\n");

    if (data) {
        for (i = 0; i < SDL_arraysize(data->screens); ++i) {
            if (data->screens[i]) {
               SDL_FreeSurface(data->screens[i]);
            }
            if (data->textures[i]) {
                rsxFree(data->textures[i]);
            }
        }
        SDL_free(data);
    }
    SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_PS3 */

/* vi: set ts=4 sw=4 expandtab: */
