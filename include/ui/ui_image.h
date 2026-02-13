#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include "ui/ui_element.h"

/*
 * Static image display element.
 *
 * Loads an image file (PNG, JPG, etc.) via SDL_image and renders it as
 * a texture stretched to fill the element's rect. If loading fails, a
 * built-in fallback asset (assets/missing-image.png) is loaded instead.
 * Non-interactive: the element is created with enabled=false so it
 * receives no events.
 *
 * The texture is owned by this element and destroyed when the element is
 * destroyed.
 */
typedef struct ui_image
{
    ui_element base;
    SDL_Texture *texture;
} ui_image;

/*
 * Create an image element that displays the file at file_path.
 *
 * The image is loaded via IMG_LoadTexture and rendered into the rectangle
 * defined by (x, y, w, h).  The texture stretches to fill the rect — no
 * aspect-ratio preservation is performed.
 *
 * Parameters:
 * - renderer:  active SDL renderer used to create the texture
 * - x, y:     top-left position in window coordinates
 * - w, h:     display width and height in window coordinates
 * - file_path: path to the image file (PNG, JPG, BMP, etc.)
 *
 * Returns a heap-allocated ui_image, or NULL if allocation fails, renderer
 * is NULL, or both the requested image and fallback image cannot be loaded.
 * Ownership transfers to caller (or ui_context after ui_context_add
 * succeeds).
 */
ui_image *ui_image_create(SDL_Renderer *renderer, float x, float y, float w, float h,
                          const char *file_path);

#endif
