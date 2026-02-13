#ifndef UI_SLIDER_H
#define UI_SLIDER_H

#include "ui/ui_element.h"

/*
 * Callback invoked when the slider value changes due to pointer interaction.
 *
 * Parameters:
 * - value: the newly selected slider value in [min_value, max_value]
 * - context: caller-supplied opaque pointer
 */
typedef void (*slider_change_handler)(float value, void *context);

/*
 * Minimal horizontal slider control.
 *
 * The slider uses the element rect as its full interactive area. A horizontal
 * track and draggable thumb are rendered inside that area.
 */
typedef struct ui_slider
{
    ui_element base;
    float min_value;
    float max_value;
    float value;
    float thumb_width;
    SDL_Color track_color;
    SDL_Color thumb_color;
    SDL_Color active_thumb_color;
    bool is_dragging;
    slider_change_handler on_change;
    void *on_change_context;
} ui_slider;

/*
 * Create a horizontal slider.
 *
 * Behavior:
 * - Left mouse press inside slider starts drag and updates value immediately.
 * - While dragging, horizontal mouse motion updates value continuously.
 * - Left mouse release ends dragging.
 *
 * Parameters:
 * - rect: slider bounds in window coordinates
 * - min_value/max_value: inclusive numeric range (must satisfy min_value < max_value)
 * - initial_value: initial slider value (clamped to range)
 * - track_color: color used to draw the horizontal track
 * - thumb_color: color used to draw the thumb when idle
 * - active_thumb_color: color used to draw the thumb while dragging
 * - border_color: optional border around full slider bounds (NULL disables border)
 * - on_change/on_change_context: optional callback and user context
 *
 * Returns:
 * - Heap-allocated slider on success
 * - NULL on invalid arguments or allocation failure
 *
 * Ownership:
 * - Caller owns the returned pointer until transferring ownership to ui_context
 *   via ui_context_add, after which ui_context_destroy releases it.
 */
ui_slider *ui_slider_create(const SDL_FRect *rect, float min_value, float max_value,
                            float initial_value, SDL_Color track_color, SDL_Color thumb_color,
                            SDL_Color active_thumb_color, const SDL_Color *border_color,
                            slider_change_handler on_change, void *on_change_context);

#endif
