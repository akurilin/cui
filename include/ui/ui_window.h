#ifndef UI_WINDOW_H
#define UI_WINDOW_H

#include "ui/ui_element.h"

#include <stddef.h>

/*
 * Root window element used as the page's top-level UI tree node.
 *
 * The window owns child elements and forwards measure/arrange/event/update/render
 * traversal to them.
 */
typedef struct ui_window
{
    ui_element base;
    ui_element **children;
    size_t child_count;
    size_t child_capacity;
    ui_element *focused_child;
    ui_element *captured_child;
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
 * - Destroying the window also destroys every child added through
 *   ui_window_add_child.
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

/*
 * Add one child element to the window root.
 *
 * On success, ownership transfers to the window. The child must be unparented.
 */
bool ui_window_add_child(ui_window *window, ui_element *child);

/*
 * Remove one child from the window root.
 *
 * When destroy_child is true, the removed child is destroyed.
 */
bool ui_window_remove_child(ui_window *window, ui_element *child, bool destroy_child);

/*
 * Remove all children from the window root.
 *
 * When destroy_children is true, each child is destroyed.
 */
void ui_window_clear_children(ui_window *window, bool destroy_children);

#endif
