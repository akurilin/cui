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
 * Horizontal alignment anchor within a parent element.
 *
 * Controls which horizontal reference point of the parent rect.x offsets from:
 * - LEFT:     rect.x is offset rightward from parent's left edge (default).
 * - CENTER_H: rect.x is offset from horizontally-centered position.
 * - RIGHT:    rect.x is inset leftward from parent's right edge.
 */
typedef enum ui_align_h
{
    UI_ALIGN_LEFT,
    UI_ALIGN_CENTER_H,
    UI_ALIGN_RIGHT,
} ui_align_h;

/*
 * Vertical alignment anchor within a parent element.
 *
 * Controls which vertical reference point of the parent rect.y offsets from:
 * - TOP:      rect.y is offset downward from parent's top edge (default).
 * - CENTER_V: rect.y is offset from vertically-centered position.
 * - BOTTOM:   rect.y is inset upward from parent's bottom edge.
 */
typedef enum ui_align_v
{
    UI_ALIGN_TOP,
    UI_ALIGN_CENTER_V,
    UI_ALIGN_BOTTOM,
} ui_align_v;

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
     * Test whether a point is inside this element's interactive region.
     * When NULL, ui_runtime falls back to ui_element_hit_test (rect bounds).
     */
    bool (*hit_test)(const ui_element *element, const SDL_FPoint *point);

    /*
     * Return true when this element can receive keyboard focus.
     * When NULL, the element is treated as not focusable.
     */
    bool (*can_focus)(const ui_element *element);

    /*
     * Notify the element that focus changed.
     * Called by ui_runtime only for focusable elements.
     */
    void (*set_focus)(ui_element *element, bool focused);

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
 * - rect: position/size relative to the parent element (or window when
 *   parent is NULL). The meaning of rect.x/rect.y depends on align_h/align_v.
 * - parent: owning element for relative positioning (NULL = top-level).
 * - align_h/align_v: anchor point on the parent that rect offsets from.
 * - ops: behavior implementation (must be non-NULL for valid elements)
 * - visible: participates in render pass when true
 * - enabled: participates in event/update passes when true
 */
struct ui_element
{
    SDL_FRect rect;
    ui_element *parent;
    ui_align_h align_h;
    ui_align_v align_v;
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

/*
 * Compute the absolute (window-space) rectangle for an element.
 *
 * Walks the parent chain, applying each ancestor's alignment and offset to
 * convert the element's parent-relative rect into absolute window coordinates.
 * When parent is NULL the rect is returned as-is (already absolute).
 *
 * Returns: SDL_FRect with absolute x, y and the element's own w, h.
 */
SDL_FRect ui_element_screen_rect(const ui_element *element);

/*
 * Default point-in-rect hit test used by ui_runtime for pointer routing.
 *
 * Uses ui_element_screen_rect() so hit testing works correctly for elements
 * with parent-relative coordinates.
 *
 * Returns false when element/point is NULL or when the point is outside the
 * element bounds.
 */
bool ui_element_hit_test(const ui_element *element, const SDL_FPoint *point);

#endif
