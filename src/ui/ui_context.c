#include "ui/ui_context.h"

#include <stdlib.h>

static bool is_valid_element(const ui_element *element)
{
    return element != NULL && element->ops != NULL;
}

static bool is_focusable(const ui_element *element)
{
    return is_valid_element(element) && element->enabled && element->ops->can_focus != NULL &&
           element->ops->can_focus(element);
}

static bool is_pointer_event(const SDL_Event *event)
{
    return event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP ||
           event->type == SDL_EVENT_MOUSE_MOTION || event->type == SDL_EVENT_MOUSE_WHEEL;
}

static bool is_keyboard_event(const SDL_Event *event)
{
    return event->type == SDL_EVENT_TEXT_INPUT || event->type == SDL_EVENT_KEY_DOWN ||
           event->type == SDL_EVENT_KEY_UP;
}

static bool get_pointer_position(const SDL_Event *event, SDL_FPoint *out_position)
{
    if (event == NULL || out_position == NULL)
    {
        return false;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        out_position->x = event->button.x;
        out_position->y = event->button.y;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION)
    {
        out_position->x = event->motion.x;
        out_position->y = event->motion.y;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_WHEEL)
    {
        out_position->x = event->wheel.mouse_x;
        out_position->y = event->wheel.mouse_y;
        return true;
    }

    return false;
}

static bool hit_test_element(const ui_element *element, const SDL_FPoint *point)
{
    if (!is_valid_element(element) || point == NULL || !element->visible)
    {
        return false;
    }

    if (element->ops->hit_test != NULL)
    {
        return element->ops->hit_test(element, point);
    }

    return ui_element_hit_test(element, point);
}

static void set_focused_element(ui_context *context, ui_element *next_focus)
{
    if (context == NULL || context->focused_element == next_focus)
    {
        return;
    }

    ui_element *previous_focus = context->focused_element;
    context->focused_element = NULL;

    if (is_valid_element(previous_focus) && previous_focus->ops->set_focus != NULL)
    {
        previous_focus->ops->set_focus(previous_focus, false);
    }

    if (is_focusable(next_focus))
    {
        context->focused_element = next_focus;
        if (next_focus->ops->set_focus != NULL)
        {
            next_focus->ops->set_focus(next_focus, true);
        }
    }
}

static ui_element *find_top_focusable_at(const ui_context *context, const SDL_FPoint *point)
{
    if (context == NULL || point == NULL)
    {
        return NULL;
    }

    for (size_t i = context->element_count; i > 0U; --i)
    {
        ui_element *element = context->elements[i - 1U];
        if (!is_focusable(element) || !hit_test_element(element, point))
        {
            continue;
        }
        return element;
    }

    return NULL;
}

static ui_element *dispatch_pointer_event(ui_context *context, const SDL_Event *event,
                                          const SDL_FPoint *point)
{
    if (context == NULL || event == NULL || point == NULL)
    {
        return NULL;
    }

    for (size_t i = context->element_count; i > 0U; --i)
    {
        ui_element *element = context->elements[i - 1U];
        if (!is_valid_element(element) || !element->enabled || element->ops->handle_event == NULL)
        {
            continue;
        }

        if (!hit_test_element(element, point))
        {
            continue;
        }

        if (element->ops->handle_event(element, event))
        {
            return element;
        }
    }

    return NULL;
}

static bool dispatch_to_element(ui_element *element, const SDL_Event *event)
{
    if (!is_valid_element(element) || !element->enabled || element->ops->handle_event == NULL)
    {
        return false;
    }

    return element->ops->handle_event(element, event);
}

bool ui_context_init(ui_context *context)
{
    if (context == NULL)
    {
        return false;
    }

    context->elements = NULL;
    context->element_count = 0;
    context->element_capacity = 0;
    context->focused_element = NULL;
    context->captured_element = NULL;
    return true;
}

void ui_context_destroy(ui_context *context)
{
    if (context == NULL)
    {
        return;
    }

    set_focused_element(context, NULL);
    context->captured_element = NULL;

    for (size_t i = 0; i < context->element_count; ++i)
    {
        ui_element *element = context->elements[i];
        if (is_valid_element(element) && element->ops->destroy != NULL)
        {
            element->ops->destroy(element);
        }
    }

    free((void *)context->elements);
    context->elements = NULL;
    context->element_count = 0;
    context->element_capacity = 0;
    context->focused_element = NULL;
    context->captured_element = NULL;
}

bool ui_context_add(ui_context *context, ui_element *element)
{
    if (context == NULL || !is_valid_element(element))
    {
        return false;
    }

    if (context->element_count == context->element_capacity)
    {
        size_t new_capacity = context->element_capacity == 0 ? 8U : context->element_capacity * 2U;
        ui_element **new_elements =
            (ui_element **)realloc((void *)context->elements, new_capacity * sizeof(ui_element *));
        if (new_elements == NULL)
        {
            return false;
        }
        context->elements = new_elements;
        context->element_capacity = new_capacity;
    }

    context->elements[context->element_count++] = element;
    return true;
}

bool ui_context_remove(ui_context *context, ui_element *element, bool destroy_element)
{
    if (context == NULL || !is_valid_element(element))
    {
        return false;
    }

    for (size_t i = 0; i < context->element_count; ++i)
    {
        if (context->elements[i] != element)
        {
            continue;
        }

        if (context->focused_element == element)
        {
            set_focused_element(context, NULL);
        }
        if (context->captured_element == element)
        {
            context->captured_element = NULL;
        }

        if (destroy_element && element->ops->destroy != NULL)
        {
            element->ops->destroy(element);
        }

        for (size_t j = i; j + 1U < context->element_count; ++j)
        {
            context->elements[j] = context->elements[j + 1U];
        }
        context->element_count--;
        return true;
    }

    return false;
}

void ui_context_handle_event(ui_context *context, const SDL_Event *event)
{
    if (context == NULL || event == NULL)
    {
        return;
    }

    if (is_keyboard_event(event))
    {
        (void)dispatch_to_element(context->focused_element, event);
        return;
    }

    if (is_pointer_event(event))
    {
        SDL_FPoint point = {0.0F, 0.0F};
        if (!get_pointer_position(event, &point))
        {
            return;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
        {
            set_focused_element(context, find_top_focusable_at(context, &point));
            ui_element *handled_element = dispatch_pointer_event(context, event, &point);
            context->captured_element = handled_element;
            return;
        }

        if (event->type == SDL_EVENT_MOUSE_MOTION)
        {
            if (dispatch_to_element(context->captured_element, event))
            {
                return;
            }
            (void)dispatch_pointer_event(context, event, &point);
            return;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
        {
            if (dispatch_to_element(context->captured_element, event))
            {
                context->captured_element = NULL;
                return;
            }
            context->captured_element = NULL;
            (void)dispatch_pointer_event(context, event, &point);
            return;
        }

        (void)dispatch_pointer_event(context, event, &point);
        return;
    }

    for (size_t i = context->element_count; i > 0U; --i)
    {
        ui_element *element = context->elements[i - 1U];
        if (dispatch_to_element(element, event))
        {
            return;
        }
    }
}

void ui_context_update(ui_context *context, float delta_seconds)
{
    if (context == NULL)
    {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i)
    {
        ui_element *element = context->elements[i];
        if (!is_valid_element(element) || !element->enabled || element->ops->update == NULL)
        {
            continue;
        }
        element->ops->update(element, delta_seconds);
    }
}

void ui_context_render(const ui_context *context, SDL_Renderer *renderer)
{
    if (context == NULL || renderer == NULL)
    {
        return;
    }

    for (size_t i = 0; i < context->element_count; ++i)
    {
        const ui_element *element = context->elements[i];
        if (!is_valid_element(element) || !element->visible || element->ops->render == NULL)
        {
            continue;
        }
        element->ops->render(element, renderer);
    }
}
