#ifndef UI_HRULE_H
#define UI_HRULE_H

#include "ui/ui_element.h"

/*
 * Thin horizontal divider element.
 *
 * Renders a centered horizontal line within its rect, inset from the edges
 * by a configurable fraction.  Purely visual — no event handling or update
 * logic.
 *
 * Width is assigned externally by the parent layout container (top-down);
 * the hrule only owns its height (the visual thickness).
 */
typedef struct ui_hrule
{
    ui_element base;
    SDL_Color color;
    float inset_fraction;
} ui_hrule;

/*
 * Create a horizontal rule element.
 *
 * Purpose:
 * - Provide a lightweight visual separator for use inside layout containers.
 *
 * Behavior:
 * - rect.h is set to thickness; rect.w is expected to be assigned by the
 *   parent layout container each frame.
 * - Rendering draws a filled rect that is (1 - 2*inset_fraction) * rect.w
 *   wide, centered horizontally within the element's rect.
 *
 * Parameters:
 * - thickness: vertical height of the rule in pixels (becomes rect.h)
 * - color: fill color of the line
 * - inset_fraction: fraction of rect.w to leave as padding on each side
 *   (e.g. 0.05 for 5% inset on each side)
 *
 * Returns:
 * - Heap-allocated ui_hrule on success
 * - NULL on allocation failure
 *
 * Ownership/Lifecycle:
 * - Caller owns the returned pointer until transferred to a layout container
 *   or ui_runtime.
 */
ui_hrule *ui_hrule_create(float thickness, SDL_Color color, float inset_fraction);

#endif
