#include "ui/ui_fps_counter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;

static void update_fps_label(ui_fps_counter *counter) {
    snprintf(counter->label, sizeof(counter->label), "FPS: %.1f", counter->displayed_fps);

    const float label_width = (float)strlen(counter->label) * DEBUG_GLYPH_WIDTH;
    counter->base.rect.x = (float)counter->viewport_width - counter->padding - label_width;
    counter->base.rect.y =
        (float)counter->viewport_height - counter->padding - DEBUG_GLYPH_HEIGHT;
}

static void handle_fps_counter_event(ui_element *element, const SDL_Event *event) {
    (void)element;
    (void)event;
}

static void update_fps_counter(ui_element *element, float delta_seconds) {
    ui_fps_counter *counter = (ui_fps_counter *)element;

    counter->elapsed_seconds += delta_seconds;
    counter->frame_count += 1;

    if (counter->elapsed_seconds >= counter->update_interval_seconds &&
        counter->elapsed_seconds > 0.0F) {
        counter->displayed_fps = (float)counter->frame_count / counter->elapsed_seconds;
        counter->elapsed_seconds = 0.0F;
        counter->frame_count = 0;
    }

    update_fps_label(counter);
}

static void render_fps_counter(const ui_element *element, SDL_Renderer *renderer) {
    const ui_fps_counter *counter = (const ui_fps_counter *)element;

    SDL_SetRenderDrawColor(renderer, counter->color.r, counter->color.g, counter->color.b,
                           counter->color.a);
    SDL_RenderDebugText(renderer, counter->base.rect.x, counter->base.rect.y, counter->label);
}

static void destroy_fps_counter(ui_element *element) { free(element); }

static const ui_element_ops FPS_COUNTER_OPS = {
    .handle_event = handle_fps_counter_event,
    .update = update_fps_counter,
    .render = render_fps_counter,
    .destroy = destroy_fps_counter,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_fps_counter *create_fps_counter(int viewport_width, int viewport_height, float padding,
                                   SDL_Color color) {
    if (viewport_width <= 0 || viewport_height <= 0) {
        return NULL;
    }

    ui_fps_counter *counter = malloc(sizeof(*counter));
    if (counter == NULL) {
        return NULL;
    }

    counter->base.rect = (SDL_FRect){0.0F, 0.0F, 0.0F, DEBUG_GLYPH_HEIGHT};
    counter->base.ops = &FPS_COUNTER_OPS;
    counter->base.visible = true;
    counter->base.enabled = true;
    counter->color = color;
    counter->viewport_width = viewport_width;
    counter->viewport_height = viewport_height;
    counter->update_interval_seconds = 0.25F;
    counter->elapsed_seconds = 0.0F;
    counter->frame_count = 0;
    counter->displayed_fps = 0.0F;
    counter->padding = padding;
    snprintf(counter->label, sizeof(counter->label), "FPS: %.1f", counter->displayed_fps);
    update_fps_label(counter);

    return counter;
}
