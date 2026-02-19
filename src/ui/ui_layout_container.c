#include "ui/ui_layout_container.h"

#include <stdlib.h>

static const float DEFAULT_LAYOUT_PADDING = 8.0F;
static const float DEFAULT_LAYOUT_SPACING = 8.0F;
static const float PADDING_SIDES = 2.0F;

static bool is_valid_element(const ui_element *element)
{
    return element != NULL && element->ops != NULL;
}

static bool is_focusable_element(const ui_element *element)
{
    return is_valid_element(element) && element->enabled && element->ops->can_focus != NULL &&
           element->ops->can_focus(element);
}

static bool is_pointer_press_event(const SDL_Event *event)
{
    return event != NULL && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
           event->button.button == SDL_BUTTON_LEFT;
}

static bool get_pointer_position(const SDL_Event *event, SDL_FPoint *out)
{
    if (event == NULL || out == NULL)
    {
        return false;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        out->x = event->button.x;
        out->y = event->button.y;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION)
    {
        out->x = event->motion.x;
        out->y = event->motion.y;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_WHEEL)
    {
        out->x = event->wheel.mouse_x;
        out->y = event->wheel.mouse_y;
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

static void set_focused_child(ui_layout_container *container, ui_element *child)
{
    if (container == NULL || container->focused_child == child)
    {
        return;
    }

    if (is_valid_element(container->focused_child) &&
        container->focused_child->ops->set_focus != NULL)
    {
        container->focused_child->ops->set_focus(container->focused_child, false);
    }

    container->focused_child = NULL;

    if (is_focusable_element(child))
    {
        container->focused_child = child;
        if (child->ops->set_focus != NULL)
        {
            child->ops->set_focus(child, true);
        }
    }
}

static ui_element *find_top_focusable_child_at(const ui_layout_container *container,
                                               const SDL_FPoint *point)
{
    if (container == NULL || point == NULL)
    {
        return NULL;
    }

    for (size_t i = container->child_count; i > 0U; --i)
    {
        ui_element *child = container->children[i - 1U];
        if (!is_focusable_element(child))
        {
            continue;
        }

        if (hit_test_element(child, point))
        {
            return child;
        }
    }

    return NULL;
}

static bool would_create_parent_cycle(const ui_element *child, const ui_element *new_parent)
{
    if (child == NULL || new_parent == NULL)
    {
        return false;
    }

    for (const ui_element *cursor = new_parent; cursor != NULL; cursor = cursor->parent)
    {
        if (cursor == child)
        {
            return true;
        }
    }

    return false;
}

static float clamp_non_negative(float value)
{
    if (value < 0.0F)
    {
        return 0.0F;
    }
    return value;
}

static float measure_vertical_children(ui_layout_container *container, float inner_w)
{
    const float padding = DEFAULT_LAYOUT_PADDING;
    const float spacing = DEFAULT_LAYOUT_SPACING;
    float cursor_y = padding;

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        const SDL_FRect child_available = {0.0F, 0.0F, inner_w, child->rect.h};
        ui_element_measure(child, &child_available);
        cursor_y += clamp_non_negative(child->rect.h) + spacing;
    }

    if (cursor_y <= padding)
    {
        return padding * PADDING_SIDES;
    }

    return cursor_y - spacing + padding;
}

static void arrange_vertical_children(ui_layout_container *container, float inner_w)
{
    const float padding = DEFAULT_LAYOUT_PADDING;
    const float spacing = DEFAULT_LAYOUT_SPACING;
    float cursor_y = padding;

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        const float child_height = clamp_non_negative(child->rect.h);
        const SDL_FRect child_final = {padding, cursor_y, inner_w, child_height};
        ui_element_arrange(child, &child_final);
        cursor_y += child_height + spacing;
    }
}

static void measure_horizontal_children(ui_layout_container *container, float inner_w,
                                        float inner_h)
{
    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        SDL_FRect child_available = {0.0F, 0.0F, inner_w, inner_h};
        if (child->align_h == UI_ALIGN_RIGHT)
        {
            child_available.w = child->rect.w;
        }
        ui_element_measure(child, &child_available);
    }
}

static void arrange_horizontal_children(ui_layout_container *container, float inner_h)
{
    const float padding = DEFAULT_LAYOUT_PADDING;
    const float spacing = DEFAULT_LAYOUT_SPACING;
    float cursor_x = padding;

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        const float child_width = clamp_non_negative(child->rect.w);
        const float inset_x = child->rect.x;

        if (child->align_h == UI_ALIGN_RIGHT)
        {
            // Preserve right-anchor inset. screen_rect resolves x from parent
            // width when align_h is UI_ALIGN_RIGHT.
            const SDL_FRect child_final = {inset_x, padding, child_width, inner_h};
            ui_element_arrange(child, &child_final);
            continue;
        }

        const SDL_FRect child_final = {cursor_x, padding, child_width, inner_h};
        ui_element_arrange(child, &child_final);
        cursor_x += child_width + spacing;
    }
}

static void measure_layout_container(ui_element *element, const SDL_FRect *available_rect)
{
    ui_layout_container *container = (ui_layout_container *)element;
    const float padding = DEFAULT_LAYOUT_PADDING;

    if (available_rect != NULL)
    {
        container->base.rect.w = clamp_non_negative(available_rect->w);
        if (available_rect->h > 0.0F)
        {
            container->base.rect.h = clamp_non_negative(available_rect->h);
        }
    }

    const float inner_w = clamp_non_negative(container->base.rect.w - (padding * 2.0F));
    const float inner_h = clamp_non_negative(container->base.rect.h - (padding * 2.0F));

    if (container->axis == UI_LAYOUT_AXIS_VERTICAL)
    {
        container->base.rect.h = measure_vertical_children(container, inner_w);
        return;
    }

    measure_horizontal_children(container, inner_w, inner_h);
}

static void arrange_layout_container(ui_element *element, const SDL_FRect *final_rect)
{
    ui_layout_container *container = (ui_layout_container *)element;
    const float padding = DEFAULT_LAYOUT_PADDING;

    if (final_rect != NULL)
    {
        container->base.rect = *final_rect;
    }

    const float inner_w = clamp_non_negative(container->base.rect.w - (padding * 2.0F));
    const float inner_h = clamp_non_negative(container->base.rect.h - (padding * 2.0F));
    if (container->axis == UI_LAYOUT_AXIS_VERTICAL)
    {
        arrange_vertical_children(container, inner_w);
        return;
    }

    arrange_horizontal_children(container, inner_h);
}

static bool handle_layout_container_event(ui_element *element, const SDL_Event *event)
{
    ui_layout_container *container = (ui_layout_container *)element;
    if (container == NULL)
    {
        return false;
    }

    if (is_pointer_press_event(event))
    {
        SDL_FPoint point = {0.0F, 0.0F};
        if (get_pointer_position(event, &point))
        {
            set_focused_child(container, find_top_focusable_child_at(container, &point));
        }
    }

    for (size_t i = container->child_count; i > 0U; --i)
    {
        ui_element *child = container->children[i - 1U];
        if (!is_valid_element(child) || !child->enabled || child->ops->handle_event == NULL)
        {
            continue;
        }
        if (child->ops->handle_event(child, event))
        {
            return true;
        }
    }

    return false;
}

static bool can_focus_layout_container(const ui_element *element)
{
    const ui_layout_container *container = (const ui_layout_container *)element;
    if (container == NULL)
    {
        return false;
    }

    for (size_t i = 0; i < container->child_count; ++i)
    {
        if (is_focusable_element(container->children[i]))
        {
            return true;
        }
    }

    return false;
}

static void set_layout_container_focus(ui_element *element, bool focused)
{
    ui_layout_container *container = (ui_layout_container *)element;
    if (container == NULL || focused)
    {
        return;
    }

    set_focused_child(container, NULL);
}

static void update_layout_container(ui_element *element, float delta_seconds)
{
    ui_layout_container *container = (ui_layout_container *)element;
    if (container == NULL)
    {
        return;
    }

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child) || !child->enabled || child->ops->update == NULL)
        {
            continue;
        }
        child->ops->update(child, delta_seconds);
    }
}

static void render_layout_container(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_layout_container *container = (const ui_layout_container *)element;

    for (size_t i = 0; i < container->child_count; ++i)
    {
        const ui_element *child = container->children[i];
        if (!is_valid_element(child) || !child->visible || child->ops->render == NULL)
        {
            continue;
        }
        child->ops->render(child, renderer);
    }

    if (container->base.has_border)
    {
        const SDL_FRect sr = ui_element_screen_rect(element);
        ui_element_render_inner_border(renderer, &sr, container->base.border_color,
                                       container->base.border_width);
    }
}

static void destroy_layout_container(ui_element *element)
{
    ui_layout_container *container = (ui_layout_container *)element;

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (is_valid_element(child) && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }
    }

    free((void *)container->children);
    free(container);
}

static const ui_element_ops LAYOUT_CONTAINER_OPS = {
    .measure = measure_layout_container,
    .arrange = arrange_layout_container,
    .handle_event = handle_layout_container_event,
    .can_focus = can_focus_layout_container,
    .set_focus = set_layout_container_focus,
    .update = update_layout_container,
    .render = render_layout_container,
    .destroy = destroy_layout_container,
};

ui_layout_container *ui_layout_container_create(const SDL_FRect *rect, ui_layout_axis axis,
                                                const SDL_Color *border_color)
{
    if (rect == NULL)
    {
        return NULL;
    }

    ui_layout_container *container = malloc(sizeof(*container));
    if (container == NULL)
    {
        return NULL;
    }

    container->base.rect = *rect;
    container->base.ops = &LAYOUT_CONTAINER_OPS;
    container->base.visible = true;
    container->base.enabled = true;
    container->base.parent = NULL;
    container->base.align_h = UI_ALIGN_LEFT;
    container->base.align_v = UI_ALIGN_TOP;
    ui_element_set_border(&container->base, border_color, 1.0F);
    container->axis = axis;
    container->children = NULL;
    container->child_count = 0;
    container->child_capacity = 0;
    container->focused_child = NULL;

    return container;
}

bool ui_layout_container_add_child(ui_layout_container *container, ui_element *child)
{
    if (container == NULL || !is_valid_element(child))
    {
        return false;
    }

    if (child->parent != NULL)
    {
        return false;
    }

    if (would_create_parent_cycle(child, &container->base))
    {
        return false;
    }

    if (container->child_count == container->child_capacity)
    {
        size_t new_capacity = container->child_capacity == 0 ? 8U : container->child_capacity * 2U;
        ui_element **new_children = (ui_element **)realloc((void *)container->children,
                                                           new_capacity * sizeof(ui_element *));
        if (new_children == NULL)
        {
            return false;
        }
        container->children = new_children;
        container->child_capacity = new_capacity;
    }

    child->parent = &container->base;
    container->children[container->child_count++] = child;
    return true;
}

bool ui_layout_container_remove_child(ui_layout_container *container, ui_element *child,
                                      bool destroy_child)
{
    if (container == NULL || !is_valid_element(child))
    {
        return false;
    }

    for (size_t i = 0; i < container->child_count; ++i)
    {
        if (container->children[i] != child)
        {
            continue;
        }

        if (destroy_child && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }
        else
        {
            child->parent = NULL;
        }

        if (container->focused_child == child)
        {
            container->focused_child = NULL;
        }

        for (size_t j = i; j + 1U < container->child_count; ++j)
        {
            container->children[j] = container->children[j + 1U];
        }
        container->child_count--;
        return true;
    }

    return false;
}

void ui_layout_container_clear_children(ui_layout_container *container, bool destroy_children)
{
    if (container == NULL)
    {
        return;
    }

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }
        if (destroy_children && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }
        else
        {
            child->parent = NULL;
        }
    }

    container->child_count = 0;
}
