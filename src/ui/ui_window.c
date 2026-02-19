#include "ui/ui_window.h"

#include <stdlib.h>

static bool is_valid_element(const ui_element *element)
{
    return element != NULL && element->ops != NULL;
}

static bool is_focusable_element(const ui_element *element)
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

static bool hit_test_child(const ui_element *child, const SDL_FPoint *point)
{
    if (!is_valid_element(child) || point == NULL || !child->visible)
    {
        return false;
    }

    if (child->ops->hit_test != NULL)
    {
        return child->ops->hit_test(child, point);
    }

    return ui_element_hit_test(child, point);
}

static bool dispatch_to_child(ui_element *child, const SDL_Event *event)
{
    if (!is_valid_element(child) || !child->enabled || child->ops->handle_event == NULL)
    {
        return false;
    }

    return child->ops->handle_event(child, event);
}

static void set_focused_child(ui_window *window, ui_element *next_focus)
{
    if (window == NULL || window->focused_child == next_focus)
    {
        return;
    }

    ui_element *previous = window->focused_child;
    window->focused_child = NULL;

    if (is_valid_element(previous) && previous->ops->set_focus != NULL)
    {
        previous->ops->set_focus(previous, false);
    }

    if (is_focusable_element(next_focus))
    {
        window->focused_child = next_focus;
        if (next_focus->ops->set_focus != NULL)
        {
            next_focus->ops->set_focus(next_focus, true);
        }
    }
}

static ui_element *find_top_focusable_child_at(const ui_window *window, const SDL_FPoint *point)
{
    if (window == NULL || point == NULL)
    {
        return NULL;
    }

    for (size_t i = window->child_count; i > 0U; --i)
    {
        ui_element *child = window->children[i - 1U];
        if (!is_focusable_element(child) || !hit_test_child(child, point))
        {
            continue;
        }
        return child;
    }

    return NULL;
}

static ui_element *dispatch_pointer_to_top_child(ui_window *window, const SDL_Event *event,
                                                 const SDL_FPoint *point)
{
    if (window == NULL || event == NULL || point == NULL)
    {
        return NULL;
    }

    for (size_t i = window->child_count; i > 0U; --i)
    {
        ui_element *child = window->children[i - 1U];
        if (!is_valid_element(child) || !child->enabled || child->ops->handle_event == NULL)
        {
            continue;
        }

        if (!hit_test_child(child, point))
        {
            continue;
        }

        if (child->ops->handle_event(child, event))
        {
            return child;
        }
    }

    return NULL;
}

static void run_window_layout_pass(ui_window *window)
{
    if (window == NULL)
    {
        return;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        ui_element *child = window->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        const SDL_FRect child_available = child->rect;
        ui_element_measure(child, &child_available);
        SDL_FRect child_final = child->rect;
        if (child->ops != NULL && child->ops->measure != NULL)
        {
            child_final.w = child->rect.w;
            child_final.h = child->rect.h;
        }
        ui_element_arrange(child, &child_final);
    }
}

static void measure_window(ui_element *element, const SDL_FRect *available_rect)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL)
    {
        return;
    }

    if (available_rect != NULL)
    {
        if (available_rect->w > 0.0F)
        {
            window->base.rect.w = available_rect->w;
        }
        if (available_rect->h > 0.0F)
        {
            window->base.rect.h = available_rect->h;
        }
    }
}

static void arrange_window(ui_element *element, const SDL_FRect *final_rect)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL)
    {
        return;
    }

    if (final_rect != NULL)
    {
        window->base.rect = *final_rect;
    }

    run_window_layout_pass(window);
}

static bool handle_window_event(ui_element *element, const SDL_Event *event)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL || event == NULL)
    {
        return false;
    }

    if (is_keyboard_event(event))
    {
        return dispatch_to_child(window->focused_child, event);
    }

    if (is_pointer_event(event))
    {
        SDL_FPoint point = {0.0F, 0.0F};
        if (!get_pointer_position(event, &point))
        {
            return false;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
        {
            set_focused_child(window, find_top_focusable_child_at(window, &point));
            ui_element *handled = dispatch_pointer_to_top_child(window, event, &point);
            window->captured_child = handled;
            return handled != NULL;
        }

        if (event->type == SDL_EVENT_MOUSE_MOTION)
        {
            if (dispatch_to_child(window->captured_child, event))
            {
                return true;
            }
            return dispatch_pointer_to_top_child(window, event, &point) != NULL;
        }

        if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
        {
            if (dispatch_to_child(window->captured_child, event))
            {
                window->captured_child = NULL;
                return true;
            }
            window->captured_child = NULL;
            return dispatch_pointer_to_top_child(window, event, &point) != NULL;
        }

        return dispatch_pointer_to_top_child(window, event, &point) != NULL;
    }

    for (size_t i = window->child_count; i > 0U; --i)
    {
        ui_element *child = window->children[i - 1U];
        if (dispatch_to_child(child, event))
        {
            return true;
        }
    }

    return false;
}

static bool can_focus_window(const ui_element *element)
{
    const ui_window *window = (const ui_window *)element;
    if (window == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        if (is_focusable_element(window->children[i]))
        {
            return true;
        }
    }

    return false;
}

static void set_window_focus(ui_element *element, bool focused)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL || focused)
    {
        return;
    }

    set_focused_child(window, NULL);
}

static void update_window(ui_element *element, float delta_seconds)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL)
    {
        return;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        ui_element *child = window->children[i];
        if (!is_valid_element(child) || !child->enabled || child->ops->update == NULL)
        {
            continue;
        }
        child->ops->update(child, delta_seconds);
    }
}

static void render_window(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_window *window = (const ui_window *)element;
    if (window == NULL)
    {
        return;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        const ui_element *child = window->children[i];
        if (!is_valid_element(child) || !child->visible || child->ops->render == NULL)
        {
            continue;
        }
        child->ops->render(child, renderer);
    }
}

static void destroy_window(ui_element *element)
{
    ui_window *window = (ui_window *)element;
    if (window == NULL)
    {
        return;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        ui_element *child = window->children[i];
        if (is_valid_element(child) && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }
    }

    free((void *)window->children);
    free(window);
}

static const ui_element_ops WINDOW_OPS = {
    .measure = measure_window,
    .arrange = arrange_window,
    .handle_event = handle_window_event,
    .can_focus = can_focus_window,
    .set_focus = set_window_focus,
    .update = update_window,
    .render = render_window,
    .destroy = destroy_window,
};

ui_window *ui_window_create(const SDL_FRect *rect)
{
    if (rect == NULL || rect->w <= 0.0F || rect->h <= 0.0F)
    {
        return NULL;
    }

    ui_window *window = malloc(sizeof(*window));
    if (window == NULL)
    {
        return NULL;
    }

    window->base.rect = *rect;
    window->base.parent = NULL;
    window->base.align_h = UI_ALIGN_LEFT;
    window->base.align_v = UI_ALIGN_TOP;
    window->base.ops = &WINDOW_OPS;
    window->base.visible = true;
    window->base.enabled = true;
    ui_element_clear_border(&window->base);
    window->children = NULL;
    window->child_count = 0U;
    window->child_capacity = 0U;
    window->focused_child = NULL;
    window->captured_child = NULL;
    return window;
}

bool ui_window_set_size(ui_window *window, float width, float height)
{
    if (window == NULL || width <= 0.0F || height <= 0.0F)
    {
        return false;
    }

    window->base.rect.w = width;
    window->base.rect.h = height;
    return true;
}

bool ui_window_add_child(ui_window *window, ui_element *child)
{
    if (window == NULL || !is_valid_element(child) || child->parent != NULL)
    {
        return false;
    }

    if (window->child_count == window->child_capacity)
    {
        size_t new_capacity = window->child_capacity == 0U ? 8U : window->child_capacity * 2U;
        ui_element **new_children =
            (ui_element **)realloc((void *)window->children, new_capacity * sizeof(ui_element *));
        if (new_children == NULL)
        {
            return false;
        }

        window->children = new_children;
        window->child_capacity = new_capacity;
    }

    child->parent = &window->base;
    window->children[window->child_count++] = child;
    return true;
}

bool ui_window_remove_child(ui_window *window, ui_element *child, bool destroy_child)
{
    if (window == NULL || !is_valid_element(child))
    {
        return false;
    }

    for (size_t i = 0; i < window->child_count; ++i)
    {
        if (window->children[i] != child)
        {
            continue;
        }

        if (window->focused_child == child)
        {
            set_focused_child(window, NULL);
        }
        if (window->captured_child == child)
        {
            window->captured_child = NULL;
        }

        child->parent = NULL;
        if (destroy_child && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }

        for (size_t j = i; j + 1U < window->child_count; ++j)
        {
            window->children[j] = window->children[j + 1U];
        }
        window->child_count--;
        return true;
    }

    return false;
}

void ui_window_clear_children(ui_window *window, bool destroy_children)
{
    if (window == NULL)
    {
        return;
    }

    while (window->child_count > 0U)
    {
        ui_element *child = window->children[window->child_count - 1U];
        (void)ui_window_remove_child(window, child, destroy_children);
    }
}
