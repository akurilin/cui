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
typedef struct ui_element_ops
{
    /*
     * Process one SDL event for this element.
     * Called only when the element is enabled.
     *
     * Return:
     * - true when the element consumed/handled the event and propagation
     *   should stop for elements behind it.
     * - false when the event was not handled.
     */
    bool (*handle_event)(ui_element *element, const SDL_Event *event);

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
struct ui_element
{
    SDL_FRect rect;
    const ui_element_ops *ops;
    bool visible;
    bool enabled;
    bool has_border;
    SDL_Color border_color;
    float border_width;
};

/*
 * Configure an optional border for an element.
 *
 * Behavior:
 * - border_color == NULL clears border rendering.
 * - border_color != NULL enables border rendering using the provided color.
 * - width is clamped to at least 1.0f when border is enabled.
 */
void ui_element_set_border(ui_element *element, const SDL_Color *border_color, float width);

/*
 * Disable border rendering for an element.
 */
void ui_element_clear_border(ui_element *element);

/*
 * Draw an internal border fully inside rect.
 *
 * Border geometry is drawn as 4 filled strips and never extends outside rect.
 * If width exceeds half of rect width/height, width is clamped to fit.
 */
void ui_element_render_inner_border(SDL_Renderer *renderer, const SDL_FRect *rect, SDL_Color color,
                                    float width);

#endif
