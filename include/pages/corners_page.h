#ifndef CORNERS_PAGE_H
#define CORNERS_PAGE_H

#include <SDL3/SDL.h>

#include "pages/app_page.h"
#include "system/ui_runtime.h"

/*
 * Opaque corners-page state object.
 *
 * The page owns a root window anchor and eight corner/edge test buttons used
 * to validate anchor behavior during viewport resize.
 */
typedef struct corners_page corners_page;

/*
 * Create and register the corners page in the provided UI context.
 *
 * Purpose:
 * - Build eight anchored buttons (corners and edge midpoints).
 * - Register page elements into `context` in render/event order.
 *
 * Behavior/contract:
 * - On success, returns a valid `corners_page *`.
 * - On unrecoverable internal failure (allocation/creation/registration),
 *   the page logs a critical error and aborts the process (fail-fast policy).
 *
 * Parameters:
 * - `window`: SDL window handle (unused by this page, but kept for a common
 *   page-creation signature).
 * - `context`: destination UI context that will own page elements after
 *   registration.
 * - `viewport_width`: logical viewport width for initial root anchor size.
 * - `viewport_height`: logical viewport height for initial root anchor size.
 *
 * Return value:
 * - Non-NULL page handle on success.
 *
 * Ownership/lifecycle:
 * - Caller owns the returned `corners_page *` and must release it with
 *   `corners_page_destroy`.
 */
corners_page *corners_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                                  int viewport_height);

/*
 * Reflow the corners page for a new viewport size.
 *
 * Purpose:
 * - Update root window anchor bounds so all anchored buttons track new edges.
 *
 * Parameters:
 * - `page`: page instance returned by `corners_page_create`.
 * - `viewport_width`: new viewport width in pixels.
 * - `viewport_height`: new viewport height in pixels.
 *
 * Return value:
 * - true on success.
 * - This function follows fail-fast semantics for invalid state.
 *
 * Ownership/lifecycle:
 * - Does not transfer ownership.
 */
bool corners_page_resize(corners_page *page, int viewport_width, int viewport_height);

/*
 * Advance corners-page per-frame state.
 *
 * Parameters:
 * - `page`: page instance returned by `corners_page_create`.
 *
 * Return value:
 * - true when update succeeds.
 * - This function follows fail-fast semantics for invalid state.
 */
bool corners_page_update(corners_page *page);

/*
 * Destroy a corners page and release its resources.
 *
 * Parameters:
 * - `page`: page instance to destroy; must be non-NULL.
 *
 * Ownership/lifecycle:
 * - After this call, `page` is invalid and must not be reused.
 */
void corners_page_destroy(corners_page *page);

/*
 * Corners page lifecycle callbacks exported for build-generated page discovery.
 *
 * Purpose:
 * - Expose corners page lifecycle callbacks through the generic app-page
 *   interface used by command-line page selection.
 *
 * Behavior/contract:
 * - Lifecycle callbacks forward to `corners_page_create`,
 *   `corners_page_resize`, `corners_page_update`, and `corners_page_destroy`.
 * - Page id is derived by CMake from `corners_page.c` filename.
 */
extern const app_page_ops corners_page_ops;

#endif
