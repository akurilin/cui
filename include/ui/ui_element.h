#ifndef UI_ELEMENT_H
#define UI_ELEMENT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Forward declaration for the shared UI base type.
 *
 * Every concrete UI control embeds this as its first field so it can be stored
 * and processed through the common UI context APIs.
 */
typedef struct ui_element ui_element;

/*
 * Virtual function table for UI elements.
 *
 * Why this exists: C has no inheritance, so each concrete control provides this
 * operations table to opt into common event/update/render/destroy flows.
 */
typedef struct ui_element_ops {
    /*
     * Process one SDL event for this element.
     * Called only when the element is enabled.
     */
    void (*handle_event)(ui_element *element, const SDL_Event *event);

    /*
     * Advance element state by delta_seconds.
     * Called once per frame when enabled.
     */
    void (*update)(ui_element *element, float delta_seconds);

    /*
     * Draw this element.
     * Called once per frame when visible.
     */
    void (*render)(const ui_element *element, SDL_Renderer *renderer);

    /*
     * Release resources owned by the element.
     * Implementations generally free the concrete struct itself.
     */
    void (*destroy)(ui_element *element);
} ui_element_ops;

/*
 * Shared fields for all UI elements.
 *
 * - rect: element position/size in window coordinates
 * - ops: behavior implementation (must be non-NULL for valid elements)
 * - visible: participates in render pass when true
 * - enabled: participates in event/update passes when true
 */
struct ui_element {
    SDL_FRect rect;
    const ui_element_ops *ops;
    bool visible;
    bool enabled;
};

/*
 * Return true when the point is inside rect (inclusive bounds).
 *
 * Why this helper exists: hit-testing is reused by multiple controls
 * (for example, buttons) and should stay consistent across widgets.
 */
bool is_point_in_rect(float cursor_x, float cursor_y, const SDL_FRect *rect);

#endif
