#include "ui/ui_window.h"

#include <stdlib.h>

static bool handle_window_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
    return false;
}

static void update_window(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_window(const ui_element *element, SDL_Renderer *renderer)
{
    (void)element;
    (void)renderer;
}

static void destroy_window(ui_element *element) { free(element); }

static const ui_element_ops WINDOW_OPS = {
    .handle_event = handle_window_event,
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
    window->base.visible = false;
    window->base.enabled = false;
    ui_element_clear_border(&window->base);
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
