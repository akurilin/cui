#ifndef APP_PAGE_H
#define APP_PAGE_H

#include <SDL3/SDL.h>

#include "system/ui_runtime.h"

#include <stdbool.h>
#include <stddef.h>

/*
 * Runtime lifecycle callbacks for a selectable app page.
 *
 * Purpose:
 * - Provide a common polymorphic interface so `main.c` can create, update,
 *   resize, and destroy different pages without knowing each page type.
 *
 * Behavior/contract:
 * - Implementations must return a non-NULL page instance from `create` on
 *   success.
 * - Implementations are expected to fail fast for unrecoverable internal
 *   failures instead of returning recoverable errors.
 * - `resize` and `update` return true on success.
 * - `destroy` requires a non-NULL page instance and releases all owned
 *   resources.
 */
typedef struct app_page_ops
{
    /*
     * Create one page instance and register its UI elements into `context`.
     *
     * Parameters:
     * - `window`: owning SDL window.
     * - `context`: destination UI runtime that receives page elements.
     * - `viewport_width`: initial logical viewport width.
     * - `viewport_height`: initial logical viewport height.
     *
     * Return value:
     * - Opaque page instance on success.
     */
    void *(*create)(SDL_Window *window, ui_runtime *context, int viewport_width,
                    int viewport_height);

    /*
     * Reflow the page after viewport size changes.
     *
     * Parameters:
     * - `page_instance`: page created by `create`.
     * - `viewport_width`: updated logical viewport width.
     * - `viewport_height`: updated logical viewport height.
     *
     * Return value:
     * - true on success.
     */
    bool (*resize)(void *page_instance, int viewport_width, int viewport_height);

    /*
     * Advance page-specific per-frame state.
     *
     * Parameters:
     * - `page_instance`: page created by `create`.
     *
     * Return value:
     * - true on success.
     */
    bool (*update)(void *page_instance);

    /*
     * Destroy page instance and release all owned resources.
     *
     * Parameters:
     * - `page_instance`: non-NULL page instance created by `create`.
     */
    void (*destroy)(void *page_instance);
} app_page_ops;

/*
 * One discovered app page entry.
 *
 * Behavior/contract:
 * - `id` is derived from source filename prefix: `src/pages/<id>_page.c`.
 * - `ops` points to the page's lifecycle callbacks.
 */
typedef struct app_page_entry
{
    const char *id;
    const app_page_ops *ops;
} app_page_entry;

/*
 * Build-generated table of all pages discovered from `src/pages/<id>_page.c`.
 *
 * Ownership/lifecycle:
 * - Storage is static for process lifetime.
 * - Entries are immutable.
 */
extern const app_page_entry app_pages[];
extern const size_t app_page_count;

#endif
