#ifndef UI_CHECKBOX_H
#define UI_CHECKBOX_H

#include "ui/ui_element.h"

/*
 * Callback invoked when the checkbox state changes.
 *
 * Parameters:
 * - checked: the new toggle state (true = checked, false = unchecked)
 * - context: caller-supplied opaque data pointer
 */
typedef void (*checkbox_change_handler)(bool checked, void *context);

/*
 * Toggle control with a square indicator box and a text label.
 *
 * Clicking anywhere inside the element (box or label area) toggles the
 * checked state and fires the optional on_change callback.  Click semantics
 * match ui_button: press inside + release inside.
 */
typedef struct ui_checkbox
{
    ui_element base;
    SDL_Color box_color;
    SDL_Color check_color;
    SDL_Color label_color;
    bool is_checked;
    bool is_pressed;
    const char *label;
    checkbox_change_handler on_change;
    void *on_change_context;
} ui_checkbox;

/*
 * Create a checkbox at the given position.
 *
 * The indicator box is a fixed 16x16 square drawn at (x, y).  The label is
 * rendered to the right of the box using SDL_RenderDebugText.  The element's
 * hit-test rect spans the full width of box + gap + label text.
 *
 * Parameters:
 * - x, y: top-left corner in window coordinates
 * - label: borrowed string displayed next to the box (must outlive the element)
 * - box_color: outline color for the square
 * - check_color: color of the X drawn when checked
 * - label_color: text color
 * - initially_checked: starting toggle state
 * - on_change/on_change_context: optional callback + user data
 * - border_color: optional border color around full checkbox bounds (NULL disables)
 *
 * Returns a heap-allocated checkbox or NULL on failure.
 * Ownership transfers to caller (or ui_context after ui_context_add succeeds).
 */
ui_checkbox *ui_checkbox_create(float x, float y, const char *label, SDL_Color box_color,
                                SDL_Color check_color, SDL_Color label_color,
                                bool initially_checked, checkbox_change_handler on_change,
                                void *on_change_context, const SDL_Color *border_color);

#endif
