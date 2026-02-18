#ifndef TODO_PAGE_H
#define TODO_PAGE_H

#include <SDL3/SDL.h>

#include "pages/app_page.h"
#include "system/ui_runtime.h"

/*
 * Opaque todo-page state object.
 *
 * The page encapsulates all todo-list model state, callbacks, and UI element
 * references needed to build and maintain the todo mockup screen.
 */
typedef struct todo_page todo_page;

/*
 * Create and register the todo page in the provided UI context.
 *
 * Purpose:
 * - Build all todo-specific UI elements.
 * - Register those elements into `context` in render/event order.
 * - Initialize page-local task storage and seed initial demo tasks.
 *
 * Behavior/contract:
 * - On success, returns a valid `todo_page *` whose elements are owned by
 *   `context` until removed.
 * - On failure, all partially registered page elements are rolled back and
 *   destroyed, and this function returns NULL.
 *
 * Parameters:
 * - `window`: SDL window used by controls that require an owning window
 *   reference (for example, text input).
 * - `context`: destination UI context that will own page elements after
 *   successful registration.
 * - `viewport_width`: logical viewport width used by page widgets that require
 *   viewport anchoring (for example, FPS counter placement).
 * - `viewport_height`: logical viewport height used by page widgets that
 *   require viewport anchoring.
 *
 * Return value:
 * - Non-NULL page handle on success.
 * - NULL on allocation/creation/registration failure.
 *
 * Ownership/lifecycle:
 * - Caller owns the returned `todo_page *` and must release it with
 *   `todo_page_destroy`.
 * - The `ui_runtime` retains ownership of page UI elements while they remain
 *   registered.
 */
todo_page *todo_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                            int viewport_height);

/*
 * Reposition and resize all page elements for a new viewport size.
 *
 * Purpose:
 * - Reflow layout so content stretches horizontally, the task list fills
 *   available vertical space, and footer elements track the bottom edge.
 *
 * Parameters:
 * - `page`: page instance returned by `todo_page_create`.
 * - `viewport_width`: new viewport width in pixels.
 * - `viewport_height`: new viewport height in pixels.
 *
 * Return value:
 * - true on success.
 * - false if `page` is NULL or row rebuild fails.
 *
 * Ownership/lifecycle:
 * - Does not transfer ownership. Safe to call on every resize event.
 */
bool todo_page_resize(todo_page *page, int viewport_width, int viewport_height);

/*
 * Advance page-specific per-frame state.
 *
 * Purpose:
 * - Update page internals that are not handled by widget `update` hooks.
 *   Currently this refreshes the header clock only when the wall-clock second
 *   changes.
 *
 * Parameters:
 * - `page`: page instance returned by `todo_page_create`.
 *
 * Return value:
 * - true when update succeeds or no work is required.
 * - false if required UI state could not be updated.
 *
 * Ownership/lifecycle:
 * - Does not transfer ownership.
 * - Safe to call once per frame while the page is alive.
 */
bool todo_page_update(todo_page *page);

/*
 * Destroy a todo page and release its resources.
 *
 * Purpose:
 * - Unregister and destroy all page elements from their `ui_runtime`.
 * - Free todo model storage and callback context arrays.
 * - Free the page object itself.
 *
 * Parameters:
 * - `page`: page instance to destroy; NULL is allowed.
 *
 * Ownership/lifecycle:
 * - After this call, `page` is invalid and must not be reused.
 * - This function does not destroy the `ui_runtime` itself.
 */
void todo_page_destroy(todo_page *page);

/*
 * Todo page lifecycle callbacks exported for build-generated page discovery.
 *
 * Purpose:
 * - Expose TODO page lifecycle callbacks through the generic app-page
 *   interface used by command-line page selection.
 *
 * Behavior/contract:
 * - Lifecycle callbacks forward to `todo_page_create`, `todo_page_resize`,
 *   `todo_page_update`, and `todo_page_destroy`.
 * - Page id is derived by CMake from `todo_page.c` filename.
 *
 * Ownership/lifecycle:
 * - Descriptor storage is static and immutable.
 */
extern const app_page_ops todo_page_ops;

#endif
