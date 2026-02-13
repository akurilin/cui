#include "ui/ui_pane.h"

#include <stdlib.h>

static void handle_pane_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
}

static void update_pane(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_pane(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_pane *pane = (const ui_pane *)element;

    SDL_SetRenderDrawColor(renderer, pane->fill_color.r, pane->fill_color.g, pane->fill_color.b,
                           pane->fill_color.a);
    SDL_RenderFillRect(renderer, &pane->base.rect);

    SDL_SetRenderDrawColor(renderer, pane->border_color.r, pane->border_color.g,
                           pane->border_color.b, pane->border_color.a);
    SDL_RenderRect(renderer, &pane->base.rect);
}

static void destroy_pane(ui_element *element) { free(element); }

static const ui_element_ops PANE_OPS = {
    .handle_event = handle_pane_event,
    .update = update_pane,
    .render = render_pane,
    .destroy = destroy_pane,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_pane *create_pane(const SDL_FRect *rect, SDL_Color fill_color, SDL_Color border_color)
{
    if (rect == NULL)
    {
        return NULL;
    }

    ui_pane *pane = malloc(sizeof(*pane));
    if (pane == NULL)
    {
        return NULL;
    }

    pane->base.rect = *rect;
    pane->base.ops = &PANE_OPS;
    pane->base.visible = true;
    pane->base.enabled = true;
    pane->fill_color = fill_color;
    pane->border_color = border_color;

    return pane;
}
