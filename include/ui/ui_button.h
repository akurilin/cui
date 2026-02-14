#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "ui/ui_element.h"

/*
 * Callback invoked when a button click is committed.
 *
 * Click semantics: press inside + release inside the same button.
 */
typedef void (*button_click_handler)(void *context);

/*
 * Clickable rectangular control with pressed/unpressed visuals.
 */
typedef struct ui_button
{
    ui_element base;
    SDL_Color up_color;
    SDL_Color down_color;
    const char *label;
    bool is_pressed;
    button_click_handler on_click;
    void *on_click_context;
} ui_button;

/*
 * Create a button with self-contained click handling.
 *
 * Parameters:
 * - rect: clickable and render bounds in window coordinates
 * - up_color/down_color: fill colors for idle and pressed states
 * - label: optional borrowed text rendered in white at button center (NULL for no text)
 * - border_color: optional border color (NULL disables border)
 * - on_click/on_click_context: optional callback + user context
 *
 * Returns a heap-allocated button or NULL on allocation/validation failure.
 * Ownership transfers to caller (or ui_context after ui_context_add succeeds).
 */
ui_button *ui_button_create(const SDL_FRect *rect, SDL_Color up_color, SDL_Color down_color,
                            const char *label, const SDL_Color *border_color,
                            button_click_handler on_click, void *on_click_context);

#endif
