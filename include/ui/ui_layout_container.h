#ifndef UI_LAYOUT_CONTAINER_H
#define UI_LAYOUT_CONTAINER_H

#include "ui/ui_element.h"

#include <stddef.h>

/*
 * Primary direction used to place container children.
 */
typedef enum ui_layout_axis
{
    UI_LAYOUT_AXIS_VERTICAL,
    UI_LAYOUT_AXIS_HORIZONTAL,
} ui_layout_axis;

/*
 * Stack-like container element that auto-positions child elements.
 *
 * Single-pass design note:
 * - This container intentionally uses a simple single-pass layout algorithm.
 * - It does not run a separate "measure" pass where children report desired
 *   size to the parent.
 * - Child main-axis size comes from each child's current rect size.
 * - Child cross-axis size is stretched to fill the container's inner size.
 *
 * This is intentional for v1 to keep the API and implementation small in this
 * learning-oriented project.
 */
typedef struct ui_layout_container
{
    ui_element base;
    ui_layout_axis axis;
    ui_element **children;
    size_t child_count;
    size_t child_capacity;
} ui_layout_container;

/*
 * Create a stack layout container.
 *
 * Purpose:
 * - Provide a retained-mode parent element that owns child elements and
 *   automatically positions them.
 *
 * Behavior:
 * - Children are laid out in insertion order.
 * - Layout uses fixed internal defaults (padding=8px, spacing=8px).
 * - Vertical axis: children keep rect.h and are stretched to inner width.
 * - Horizontal axis: children keep rect.w and are stretched to inner height.
 * - Children may overflow the container bounds; clipping/scrolling is not
 *   performed in this version.
 *
 * Parameters:
 * - rect: container bounds in window coordinates (must be non-NULL)
 * - axis: layout direction for child placement
 * - border_color: optional border color around container bounds (NULL disables)
 *
 * Returns:
 * - Heap-allocated container on success
 * - NULL on invalid arguments or allocation failure
 *
 * Ownership/Lifecycle:
 * - Caller owns the returned pointer until transferred to ui_context via
 *   ui_context_add.
 * - The container owns every child added through ui_layout_container_add_child.
 * - Destroying the container destroys all registered children.
 */
ui_layout_container *ui_layout_container_create(const SDL_FRect *rect, ui_layout_axis axis,
                                                const SDL_Color *border_color);

/*
 * Add one child element to a layout container.
 *
 * Purpose:
 * - Register a child for automatic layout and event/update/render forwarding.
 *
 * Behavior/Contract:
 * - Children are processed in insertion order.
 * - On success, ownership transfers to the container.
 * - On failure, ownership remains with the caller.
 * - child must be non-NULL and must provide a non-NULL ops table.
 * - child must not already have a parent (reparenting is rejected).
 * - The call is rejected if attaching child would create a parent-cycle.
 *
 * Parameters:
 * - container: destination layout container (must be non-NULL)
 * - child: child element to register (must be non-NULL and valid)
 *
 * Returns:
 * - true on success
 * - false on validation or allocation failure
 */
bool ui_layout_container_add_child(ui_layout_container *container, ui_element *child);

/*
 * Remove one child from a layout container.
 *
 * Behavior:
 * - Matches by pointer identity.
 * - Preserves relative order of remaining children.
 * - Optionally destroys the removed child.
 *
 * Parameters:
 * - container: source layout container
 * - child: child pointer previously added to this container
 * - destroy_child: when true, call child->ops->destroy before returning
 *
 * Returns:
 * - true when a child was found and removed
 * - false on invalid args or if child is not present
 */
bool ui_layout_container_remove_child(ui_layout_container *container, ui_element *child,
                                      bool destroy_child);

/*
 * Remove all children from a layout container.
 *
 * Parameters:
 * - container: source layout container
 * - destroy_children: when true, destroy each removed child
 */
void ui_layout_container_clear_children(ui_layout_container *container, bool destroy_children);

#endif
