#include "ui/ui_scroll_view.h"

#include <stdlib.h>

static bool is_mouse_event(const SDL_Event *event)
{
    return event->type == SDL_EVENT_MOUSE_BUTTON_DOWN || event->type == SDL_EVENT_MOUSE_BUTTON_UP ||
           event->type == SDL_EVENT_MOUSE_MOTION || event->type == SDL_EVENT_MOUSE_WHEEL;
}

static bool get_mouse_position(const SDL_Event *event, SDL_FPoint *out)
{
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

static float compute_max_scroll(const ui_scroll_view *scroll)
{
    const float content_height = scroll->child->rect.h;
    const float viewport_height = scroll->base.rect.h;
    if (content_height <= viewport_height)
    {
        return 0.0F;
    }
    return content_height - viewport_height;
}

static float clamp_scroll(float offset, float max_offset)
{
    if (offset < 0.0F)
    {
        return 0.0F;
    }
    if (offset > max_offset)
    {
        return max_offset;
    }
    return offset;
}

static void position_child(ui_scroll_view *scroll)
{
    scroll->child->rect.x = 0.0F;
    scroll->child->rect.y = -scroll->scroll_offset_y;
    scroll->child->rect.w = scroll->base.rect.w;
}

static bool can_focus_scroll_view(const ui_element *element)
{
    const ui_scroll_view *scroll = (const ui_scroll_view *)element;
    if (scroll == NULL || scroll->child == NULL || scroll->child->ops == NULL ||
        !scroll->child->enabled || scroll->child->ops->can_focus == NULL)
    {
        return false;
    }

    return scroll->child->ops->can_focus(scroll->child);
}

static void set_scroll_view_focus(ui_element *element, bool focused)
{
    ui_scroll_view *scroll = (ui_scroll_view *)element;
    if (scroll == NULL || scroll->child == NULL || scroll->child->ops == NULL ||
        scroll->child->ops->set_focus == NULL)
    {
        return;
    }

    scroll->child->ops->set_focus(scroll->child, focused);
}

static bool handle_scroll_view_event(ui_element *element, const SDL_Event *event)
{
    ui_scroll_view *scroll = (ui_scroll_view *)element;

    if (scroll->child == NULL || scroll->child->ops == NULL)
    {
        return false;
    }

    position_child(scroll);

    // Gate mouse events to the viewport area.
    if (is_mouse_event(event))
    {
        const SDL_FRect sr = ui_element_screen_rect(element);
        SDL_FPoint cursor = {0.0F, 0.0F};
        if (!get_mouse_position(event, &cursor) || !SDL_PointInRectFloat(&cursor, &sr))
        {
            return false;
        }

        // Handle mouse wheel for scrolling.
        if (event->type == SDL_EVENT_MOUSE_WHEEL)
        {
            const float max_offset = compute_max_scroll(scroll);
            if (max_offset <= 0.0F)
            {
                return false;
            }
            scroll->scroll_offset_y -= event->wheel.y * scroll->scroll_step;
            scroll->scroll_offset_y = clamp_scroll(scroll->scroll_offset_y, max_offset);
            position_child(scroll);
            return true;
        }

        // Forward other mouse events to the child.
        if (scroll->child->enabled && scroll->child->ops->handle_event != NULL)
        {
            return scroll->child->ops->handle_event(scroll->child, event);
        }
        return false;
    }

    // Non-mouse events (keyboard, text input) are always forwarded.
    if (scroll->child->enabled && scroll->child->ops->handle_event != NULL)
    {
        return scroll->child->ops->handle_event(scroll->child, event);
    }
    return false;
}

static void update_scroll_view(ui_element *element, float delta_seconds)
{
    ui_scroll_view *scroll = (ui_scroll_view *)element;

    if (scroll->child == NULL || scroll->child->ops == NULL)
    {
        return;
    }

    position_child(scroll);

    if (scroll->child->enabled && scroll->child->ops->update != NULL)
    {
        scroll->child->ops->update(scroll->child, delta_seconds);
    }

    // Re-clamp after child update in case content height changed.
    const float max_offset = compute_max_scroll(scroll);
    scroll->scroll_offset_y = clamp_scroll(scroll->scroll_offset_y, max_offset);
    position_child(scroll);
}

static void render_scroll_view(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_scroll_view *scroll = (const ui_scroll_view *)element;

    if (scroll->child == NULL || scroll->child->ops == NULL)
    {
        return;
    }

    // Save the current clip rect, apply viewport clipping, render child, restore.
    const SDL_FRect sr = ui_element_screen_rect(element);
    SDL_Rect saved_clip;
    const bool had_clip = SDL_GetRenderClipRect(renderer, &saved_clip);
    const SDL_Rect viewport_clip = {(int)sr.x, (int)sr.y, (int)sr.w, (int)sr.h};
    SDL_SetRenderClipRect(renderer, &viewport_clip);

    if (scroll->child->visible && scroll->child->ops->render != NULL)
    {
        scroll->child->ops->render(scroll->child, renderer);
    }

    // Restore previous clip state.
    if (had_clip && (saved_clip.w > 0 || saved_clip.h > 0))
    {
        SDL_SetRenderClipRect(renderer, &saved_clip);
    }
    else
    {
        SDL_SetRenderClipRect(renderer, NULL);
    }

    if (scroll->base.has_border)
    {
        ui_element_render_inner_border(renderer, &sr, scroll->base.border_color,
                                       scroll->base.border_width);
    }
}

static void destroy_scroll_view(ui_element *element)
{
    ui_scroll_view *scroll = (ui_scroll_view *)element;

    if (scroll->child != NULL && scroll->child->ops != NULL && scroll->child->ops->destroy != NULL)
    {
        scroll->child->ops->destroy(scroll->child);
    }
    free(element);
}

static const ui_element_ops SCROLL_VIEW_OPS = {
    .handle_event = handle_scroll_view_event,
    .can_focus = can_focus_scroll_view,
    .set_focus = set_scroll_view_focus,
    .update = update_scroll_view,
    .render = render_scroll_view,
    .destroy = destroy_scroll_view,
};

ui_scroll_view *ui_scroll_view_create(const SDL_FRect *rect, ui_element *child, float scroll_step,
                                      const SDL_Color *border_color)
{
    if (rect == NULL || child == NULL || child->ops == NULL || rect->w <= 0.0F || rect->h <= 0.0F)
    {
        return NULL;
    }

    if (child->parent != NULL)
    {
        return NULL;
    }

    ui_scroll_view *scroll = malloc(sizeof(*scroll));
    if (scroll == NULL)
    {
        return NULL;
    }

    scroll->base.rect = *rect;
    scroll->base.ops = &SCROLL_VIEW_OPS;
    scroll->base.visible = true;
    scroll->base.enabled = true;
    scroll->base.parent = NULL;
    scroll->base.align_h = UI_ALIGN_LEFT;
    scroll->base.align_v = UI_ALIGN_TOP;
    ui_element_set_border(&scroll->base, border_color, 1.0F);
    scroll->child = child;
    scroll->scroll_offset_y = 0.0F;
    static const float DEFAULT_SCROLL_STEP = 20.0F;
    scroll->scroll_step = scroll_step > 0.0F ? scroll_step : DEFAULT_SCROLL_STEP;

    // Set parent and position the child relative to the scroll view.
    child->parent = &scroll->base;
    child->rect.x = 0.0F;
    child->rect.y = 0.0F;
    child->rect.w = rect->w;

    return scroll;
}
