#include "ui/ui_slider.h"

#include <stdlib.h>

static const float TRACK_HEIGHT = 4.0F;
static const float DEFAULT_THUMB_WIDTH = 12.0F;

static float clamp_slider_value(const ui_slider *slider, float value)
{
    if (value < slider->min_value)
    {
        return slider->min_value;
    }
    if (value > slider->max_value)
    {
        return slider->max_value;
    }
    return value;
}

static float calculate_thumb_x(const ui_slider *slider)
{
    const float range = slider->max_value - slider->min_value;
    const float usable_width = slider->base.rect.w - slider->thumb_width;
    const float t = (slider->value - slider->min_value) / range;
    return slider->base.rect.x + (t * usable_width);
}

static SDL_FRect get_thumb_rect(const ui_slider *slider)
{
    return (SDL_FRect){calculate_thumb_x(slider), slider->base.rect.y, slider->thumb_width,
                       slider->base.rect.h};
}

static void set_slider_value_from_cursor(ui_slider *slider, float cursor_x)
{
    const float min_thumb_x = slider->base.rect.x;
    const float max_thumb_x = slider->base.rect.x + slider->base.rect.w - slider->thumb_width;
    float clamped_thumb_x = cursor_x - (slider->thumb_width * 0.5F);
    if (clamped_thumb_x < min_thumb_x)
    {
        clamped_thumb_x = min_thumb_x;
    }
    if (clamped_thumb_x > max_thumb_x)
    {
        clamped_thumb_x = max_thumb_x;
    }

    const float usable_width = slider->base.rect.w - slider->thumb_width;
    const float t =
        usable_width > 0.0F ? (clamped_thumb_x - slider->base.rect.x) / usable_width : 0.0F;
    const float new_value = slider->min_value + (t * (slider->max_value - slider->min_value));

    const float old_value = slider->value;
    slider->value = clamp_slider_value(slider, new_value);
    if (slider->on_change != NULL && slider->value != old_value)
    {
        slider->on_change(slider->value, slider->on_change_context);
    }
}

static void handle_slider_event(ui_element *element, const SDL_Event *event)
{
    ui_slider *slider = (ui_slider *)element;

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        if (SDL_PointInRectFloat(&cursor, &slider->base.rect))
        {
            slider->is_dragging = true;
            set_slider_value_from_cursor(slider, event->button.x);
        }
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION && slider->is_dragging)
    {
        set_slider_value_from_cursor(slider, event->motion.x);
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
    {
        if (slider->is_dragging)
        {
            set_slider_value_from_cursor(slider, event->button.x);
            slider->is_dragging = false;
        }
    }
}

static void update_slider(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_slider(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_slider *slider = (const ui_slider *)element;
    const float track_y = slider->base.rect.y + ((slider->base.rect.h - TRACK_HEIGHT) * 0.5F);
    const SDL_FRect track_rect = {slider->base.rect.x, track_y, slider->base.rect.w, TRACK_HEIGHT};
    const SDL_FRect thumb_rect = get_thumb_rect(slider);
    const SDL_Color current_thumb_color =
        slider->is_dragging ? slider->active_thumb_color : slider->thumb_color;

    SDL_SetRenderDrawColor(renderer, slider->track_color.r, slider->track_color.g,
                           slider->track_color.b, slider->track_color.a);
    SDL_RenderFillRect(renderer, &track_rect);

    SDL_SetRenderDrawColor(renderer, current_thumb_color.r, current_thumb_color.g,
                           current_thumb_color.b, current_thumb_color.a);
    SDL_RenderFillRect(renderer, &thumb_rect);

    if (slider->base.has_border)
    {
        ui_element_render_inner_border(renderer, &slider->base.rect, slider->base.border_color,
                                       slider->base.border_width);
    }
}

static void destroy_slider(ui_element *element) { free(element); }

static const ui_element_ops SLIDER_OPS = {
    .handle_event = handle_slider_event,
    .update = update_slider,
    .render = render_slider,
    .destroy = destroy_slider,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_slider *ui_slider_create(const SDL_FRect *rect, float min_value, float max_value,
                            float initial_value, SDL_Color track_color, SDL_Color thumb_color,
                            SDL_Color active_thumb_color, const SDL_Color *border_color,
                            slider_change_handler on_change, void *on_change_context)
{
    if (rect == NULL || rect->w <= 0.0F || rect->h <= 0.0F || min_value >= max_value)
    {
        return NULL;
    }

    ui_slider *slider = malloc(sizeof(*slider));
    if (slider == NULL)
    {
        return NULL;
    }

    slider->base.rect = *rect;
    slider->base.ops = &SLIDER_OPS;
    slider->base.visible = true;
    slider->base.enabled = true;
    ui_element_set_border(&slider->base, border_color, 1.0F);
    slider->min_value = min_value;
    slider->max_value = max_value;
    slider->value = initial_value;
    slider->thumb_width = DEFAULT_THUMB_WIDTH;
    if (slider->thumb_width > slider->base.rect.w)
    {
        slider->thumb_width = slider->base.rect.w;
    }
    slider->track_color = track_color;
    slider->thumb_color = thumb_color;
    slider->active_thumb_color = active_thumb_color;
    slider->is_dragging = false;
    slider->on_change = on_change;
    slider->on_change_context = on_change_context;
    slider->value = clamp_slider_value(slider, slider->value);

    return slider;
}
