#include "ui/ui_layout_container.h"

#include <stdlib.h>

static const float DEFAULT_LAYOUT_PADDING = 8.0F;
static const float DEFAULT_LAYOUT_SPACING = 8.0F;
static const float PADDING_SIDES = 2.0F;

static bool is_valid_element(const ui_element *element)
{
    return element != NULL && element->ops != NULL;
}

static float clamp_non_negative(float value)
{
    if (value < 0.0F)
    {
        return 0.0F;
    }
    return value;
}

static void layout_children(ui_layout_container *container)
{
    const float padding = DEFAULT_LAYOUT_PADDING;
    const float spacing = DEFAULT_LAYOUT_SPACING;
    const float inner_w = clamp_non_negative(container->base.rect.w - (padding * 2.0F));
    const float inner_h = clamp_non_negative(container->base.rect.h - (padding * 2.0F));

    if (container->axis == UI_LAYOUT_AXIS_VERTICAL)
    {
        float cursor_y = padding;
        for (size_t i = 0; i < container->child_count; ++i)
        {
            ui_element *child = container->children[i];
            if (!is_valid_element(child))
            {
                continue;
            }

            const float child_height = clamp_non_negative(child->rect.h);
            child->rect.x = padding;
            child->rect.y = cursor_y;
            child->rect.w = inner_w;
            child->rect.h = child_height;
            cursor_y += child_height + spacing;
        }

        // Auto-size rect.h to the total content height so parent elements
        // (such as a scroll view) can read the container's rect to determine
        // how much content it holds.
        if (cursor_y > padding)
        {
            container->base.rect.h = cursor_y - spacing + padding;
        }
        else
        {
            container->base.rect.h = padding * PADDING_SIDES;
        }
        return;
    }

    float cursor_x = padding;
    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child))
        {
            continue;
        }

        const float child_width = clamp_non_negative(child->rect.w);
        child->rect.x = cursor_x;
        child->rect.y = padding;
        child->rect.w = child_width;
        child->rect.h = inner_h;
        cursor_x += child_width + spacing;
    }
}

static bool handle_layout_container_event(ui_element *element, const SDL_Event *event)
{
    ui_layout_container *container = (ui_layout_container *)element;
    layout_children(container);

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

static void update_layout_container(ui_element *element, float delta_seconds)
{
    ui_layout_container *container = (ui_layout_container *)element;
    layout_children(container);

    for (size_t i = 0; i < container->child_count; ++i)
    {
        ui_element *child = container->children[i];
        if (!is_valid_element(child) || !child->enabled || child->ops->update == NULL)
        {
            continue;
        }
        child->ops->update(child, delta_seconds);
    }

    // Single-pass layout uses each child's current size. Re-run layout after
    // child updates so size changes this frame are reflected before render.
    layout_children(container);
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
    .handle_event = handle_layout_container_event,
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

    return container;
}

bool ui_layout_container_add_child(ui_layout_container *container, ui_element *child)
{
    if (container == NULL || !is_valid_element(child))
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
