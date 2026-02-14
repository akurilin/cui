#ifndef UI_SCROLL_VIEW_H
#define UI_SCROLL_VIEW_H

#include "ui/ui_element.h"

/*
 * Scrollable viewport that wraps a single child element.
 *
 * The scroll view's rect defines the visible viewport on screen. The child
 * element may be taller than the viewport; when it is, the user can scroll
 * vertically with the mouse wheel.
 *
 * Coordinate model:
 * - The scroll view positions the child so that child.rect.y is offset
 *   upward by scroll_offset_y relative to the viewport top.
 * - Child elements are positioned in screen coordinates, so hit-testing
 *   works naturally without coordinate translation.
 * - Rendering is clipped to the viewport rect.
 *
 * Mouse events are only forwarded to the child when the cursor is inside
 * the viewport. Non-positional events (keyboard, text input) are always
 * forwarded.
 */
typedef struct ui_scroll_view
{
    ui_element base;
    ui_element *child;
    float scroll_offset_y;
    float scroll_step;
} ui_scroll_view;

/*
 * Create a scroll view wrapping a single child element.
 *
 * Parameters:
 * - rect: viewport bounds in window coordinates. This is the visible area.
 * - child: the element to scroll. Its rect.h is read as the content height.
 *   Ownership transfers to the scroll view on success.
 * - scroll_step: pixels scrolled per mouse wheel tick.
 * - border_color: optional border around the viewport (NULL disables).
 *
 * Returns a heap-allocated scroll view or NULL on failure.
 * Ownership transfers to caller (or to ui_context after ui_context_add).
 */
ui_scroll_view *ui_scroll_view_create(const SDL_FRect *rect, ui_element *child, float scroll_step,
                                      const SDL_Color *border_color);

#endif
