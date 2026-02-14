#ifndef UI_CONTEXT_H
#define UI_CONTEXT_H

#include "ui/ui_element.h"

#include <stddef.h>

/*
 * Owns an ordered list of UI elements and drives their lifecycle.
 *
 * Why this exists: centralizing event dispatch, update, and render keeps main
 * loops simple and ensures controls are processed consistently.
 */
typedef struct ui_context
{
    ui_element **elements;
    size_t element_count;
    size_t element_capacity;
} ui_context;

/*
 * Initialize an empty context.
 * Returns false if context is NULL.
 */
bool ui_context_init(ui_context *context);

/*
 * Destroy all registered elements (via their destroy op) and free context storage.
 * Safe to call with NULL.
 */
void ui_context_destroy(ui_context *context);

/*
 * Add one element to the context in render/event order.
 *
 * Ownership note: after successful add, the context owns the element and is
 * responsible for destroying it in ui_context_destroy.
 */
bool ui_context_add(ui_context *context, ui_element *element);

/*
 * Remove one element from the context.
 *
 * Behavior:
 * - Matches by pointer identity.
 * - Preserves relative order of remaining elements.
 * - Optionally destroys the removed element.
 *
 * Parameters:
 * - context: root UI context
 * - element: element pointer previously added to this context
 * - destroy_element: when true, call element->ops->destroy before removal returns
 *
 * Returns:
 * - true when an element was found and removed
 * - false when context/element is invalid or element is not present
 */
bool ui_context_remove(ui_context *context, ui_element *element, bool destroy_element);

/*
 * Dispatch a single SDL event to enabled elements from front to back
 * (reverse insertion order).
 *
 * Dispatch stops at the first element whose handle_event returns true.
 */
void ui_context_handle_event(ui_context *context, const SDL_Event *event);

/*
 * Call update on each enabled element.
 * delta_seconds should be frame time in seconds.
 */
void ui_context_update(ui_context *context, float delta_seconds);

/*
 * Call render on each visible element in insertion order.
 */
void ui_context_render(const ui_context *context, SDL_Renderer *renderer);

#endif
