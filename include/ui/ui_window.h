#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "ui/ui_element.h"

/*
 * Root window element used as a parent anchor for top-level UI controls.
 *
 * This element intentionally renders nothing and handles no input. It exists
 * to provide a stable parent rect so child elements can use relative anchoring
 * (for example, bottom-right HUD elements).
 */
typedef struct ui_window
{
    ui_element base;
} ui_window;

/*
 * Create a window-root element with the provided bounds.
 *
 * Parameters:
 * - rect: root bounds in window coordinates (must be non-NULL with positive
 *   width and height).
 *
 * Returns:
 * - Heap-allocated root element on success.
 * - NULL on invalid arguments or allocation failure.
 *
 * Ownership/Lifecycle:
 * - Caller owns the returned pointer until transferred to ui_runtime.
 * - Destroying the element releases only the element itself.
 */
ui_window *ui_window_create(const SDL_FRect *rect);

/*
 * Update the window-root bounds.
 *
 * Parameters:
 * - window: target window-root element.
 * - width: new root width in pixels (must be > 0).
 * - height: new root height in pixels (must be > 0).
 *
 * Return value:
 * - true when updated.
 * - false when arguments are invalid.
 */
bool ui_window_set_size(ui_window *window, float width, float height);

#endif
