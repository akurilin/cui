#include "ui/ui_fps_counter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;

static void format_fps_label(ui_fps_counter *counter)
{
    snprintf(counter->label, sizeof(counter->label), "FPS: %.1f", counter->displayed_fps);
}

static void update_counter_layout(ui_fps_counter *counter)
{
    if (counter == NULL)
    {
        return;
    }

    if (counter->base.parent == NULL)
    {
        const float label_width = (float)strlen(counter->label) * DEBUG_GLYPH_WIDTH;
        counter->base.rect.x = (float)counter->viewport_width - counter->padding - label_width;
        counter->base.rect.y =
            (float)counter->viewport_height - counter->padding - DEBUG_GLYPH_HEIGHT;
    }
    else
    {
        counter->base.rect.x = counter->padding;
        counter->base.rect.y = counter->padding;
    }
}

static void measure_fps_counter(ui_element *element, const SDL_FRect *available_rect)
{
    (void)available_rect;

    ui_fps_counter *counter = (ui_fps_counter *)element;
    if (counter == NULL)
    {
        return;
    }

    counter->base.rect.w = (float)strlen(counter->label) * DEBUG_GLYPH_WIDTH;
    counter->base.rect.h = DEBUG_GLYPH_HEIGHT;
    update_counter_layout(counter);
}

static void arrange_fps_counter(ui_element *element, const SDL_FRect *final_rect)
{
    ui_fps_counter *counter = (ui_fps_counter *)element;
    if (counter == NULL)
    {
        return;
    }

    if (final_rect != NULL)
    {
        counter->base.rect = *final_rect;
    }
}

static bool handle_fps_counter_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
    return false;
}

static void update_fps_counter(ui_element *element, float delta_seconds)
{
    ui_fps_counter *counter = (ui_fps_counter *)element;

    counter->elapsed_seconds += delta_seconds;
    counter->frame_count += 1;

    if (counter->elapsed_seconds >= counter->update_interval_seconds &&
        counter->elapsed_seconds > 0.0F)
    {
        counter->displayed_fps = (float)counter->frame_count / counter->elapsed_seconds;
        counter->elapsed_seconds = 0.0F;
        counter->frame_count = 0;
    }

    format_fps_label(counter);
    measure_fps_counter(element, &counter->base.rect);
}

static void render_fps_counter(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_fps_counter *counter = (const ui_fps_counter *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);

    SDL_SetRenderDrawColor(renderer, counter->color.r, counter->color.g, counter->color.b,
                           counter->color.a);
    SDL_RenderDebugText(renderer, sr.x, sr.y, counter->label);
    if (counter->base.has_border)
    {
        ui_element_render_inner_border(renderer, &sr, counter->base.border_color,
                                       counter->base.border_width);
    }
}

static void destroy_fps_counter(ui_element *element) { free(element); }

static const ui_element_ops FPS_COUNTER_OPS = {
    .measure = measure_fps_counter,
    .arrange = arrange_fps_counter,
    .handle_event = handle_fps_counter_event,
    .update = update_fps_counter,
    .render = render_fps_counter,
    .destroy = destroy_fps_counter,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_fps_counter *ui_fps_counter_create(int viewport_width, int viewport_height, float padding,
                                      SDL_Color color, const SDL_Color *border_color)
{
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        return NULL;
    }

    ui_fps_counter *counter = malloc(sizeof(*counter));
    if (counter == NULL)
    {
        return NULL;
    }

    counter->base.rect = (SDL_FRect){0.0F, 0.0F, 0.0F, DEBUG_GLYPH_HEIGHT};
    counter->base.ops = &FPS_COUNTER_OPS;
    counter->base.visible = true;
    counter->base.enabled = true;
    counter->base.parent = NULL;
    counter->base.align_h = UI_ALIGN_RIGHT;
    counter->base.align_v = UI_ALIGN_BOTTOM;
    ui_element_set_border(&counter->base, border_color, 1.0F);
    counter->color = color;
    counter->viewport_width = viewport_width;
    counter->viewport_height = viewport_height;
    counter->update_interval_seconds = 0.25F;
    counter->elapsed_seconds = 0.0F;
    counter->frame_count = 0;
    counter->displayed_fps = 0.0F;
    counter->padding = padding;
    format_fps_label(counter);
    measure_fps_counter(&counter->base, &counter->base.rect);

    return counter;
}
