#ifndef SHOWCASE_PAGE_H
#define SHOWCASE_PAGE_H

#include <SDL3/SDL.h>

#include "pages/app_page.h"
#include "system/ui_runtime.h"

/*
 * Opaque showcase-page state object.
 *
 * The page demonstrates all core UI widgets on a single scrollable screen.
 */
typedef struct showcase_page showcase_page;

/*
 * Create and register the showcase page in the provided UI context.
 *
 * Purpose:
 * - Build a single page containing every current UI widget type.
 * - Register page-owned top-level elements into `context`.
 *
 * Behavior/contract:
 * - Returns a valid `showcase_page *` on success.
 * - Returns NULL on allocation, creation, or registration failure.
 *
 * Parameters:
 * - `window`: SDL window used by controls that require a window handle.
 * - `context`: destination UI runtime that will own registered elements.
 * - `viewport_width`: initial logical viewport width in pixels.
 * - `viewport_height`: initial logical viewport height in pixels.
 *
 * Ownership/lifecycle:
 * - Caller owns the returned page object and must destroy it with
 *   `showcase_page_destroy`.
 */
showcase_page *showcase_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                                    int viewport_height);

/*
 * Reflow the showcase page for a new viewport size.
 *
 * Purpose:
 * - Resize anchored/root elements and the main scroll viewport.
 *
 * Parameters:
 * - `page`: page instance returned by `showcase_page_create`.
 * - `viewport_width`: new logical viewport width in pixels.
 * - `viewport_height`: new logical viewport height in pixels.
 *
 * Return value:
 * - true on success.
 * - false on invalid arguments.
 */
bool showcase_page_resize(showcase_page *page, int viewport_width, int viewport_height);

/*
 * Advance showcase-page per-frame state.
 *
 * Parameters:
 * - `page`: page instance returned by `showcase_page_create`.
 *
 * Return value:
 * - true when page is valid.
 * - false when `page` is NULL.
 */
bool showcase_page_update(showcase_page *page);

/*
 * Destroy a showcase page and release all owned resources.
 *
 * Parameters:
 * - `page`: page instance to destroy; NULL is allowed.
 */
void showcase_page_destroy(showcase_page *page);

/*
 * Showcase page lifecycle callbacks exported for build-generated page discovery.
 */
extern const app_page_ops showcase_page_ops;

#endif
