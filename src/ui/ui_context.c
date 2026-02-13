#include "ui/ui_context.h"

#include <stdlib.h>

static bool is_valid_element(const ui_element *element) {
    return element != NULL && element->ops != NULL;
}

bool ui_context_init(ui_context *context) {
    if (context == NULL) {
        return false;
    }

    context->elements = NULL;
    context->element_count = 0;
    context->element_capacity = 0;
    return true;
}

void ui_context_destroy(ui_context *context) {
    if (context == NULL) {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i) {
        ui_element *element = context->elements[i];
        if (is_valid_element(element) && element->ops->destroy != NULL) {
            element->ops->destroy(element);
        }
    }

    free((void *)context->elements);
    context->elements = NULL;
    context->element_count = 0;
    context->element_capacity = 0;
}

bool ui_context_add(ui_context *context, ui_element *element) {
    if (context == NULL || !is_valid_element(element)) {
        return false;
    }

    if (context->element_count == context->element_capacity) {
        size_t new_capacity = context->element_capacity == 0 ? 8U : context->element_capacity * 2U;
        ui_element **new_elements =
            (ui_element **)realloc((void *)context->elements, new_capacity * sizeof(*new_elements));
        if (new_elements == NULL) {
            return false;
        }
        context->elements = new_elements;
        context->element_capacity = new_capacity;
    }

    context->elements[context->element_count++] = element;
    return true;
}

void ui_context_handle_event(ui_context *context, const SDL_Event *event) {
    if (context == NULL || event == NULL) {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i) {
        ui_element *element = context->elements[i];
        if (!is_valid_element(element) || !element->enabled || element->ops->handle_event == NULL) {
            continue;
        }
        element->ops->handle_event(element, event);
    }
}

void ui_context_update(ui_context *context, float delta_seconds) {
    if (context == NULL) {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i) {
        ui_element *element = context->elements[i];
        if (!is_valid_element(element) || !element->enabled || element->ops->update == NULL) {
            continue;
        }
        element->ops->update(element, delta_seconds);
    }
}

void ui_context_render(const ui_context *context, SDL_Renderer *renderer) {
    if (context == NULL || renderer == NULL) {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i) {
        const ui_element *element = context->elements[i];
        if (!is_valid_element(element) || !element->visible || element->ops->render == NULL) {
            continue;
        }
        element->ops->render(element, renderer);
    }
}
