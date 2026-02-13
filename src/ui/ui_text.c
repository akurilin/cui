#include "ui/ui_text.h"

#include <stdlib.h>

static void handle_text_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
}

static void update_text(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_text(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_text *text = (const ui_text *)element;

    SDL_SetRenderDrawColor(renderer, text->color.r, text->color.g, text->color.b, text->color.a);
    SDL_RenderDebugText(renderer, text->base.rect.x, text->base.rect.y, text->content);
}

static void destroy_text(ui_element *element) { free(element); }

static const ui_element_ops TEXT_OPS = {
    .handle_event = handle_text_event,
    .update = update_text,
    .render = render_text,
    .destroy = destroy_text,
};

ui_text *create_text(float x, float y, const char *content, SDL_Color color)
{
    if (content == NULL)
    {
        return NULL;
    }

    ui_text *text = malloc(sizeof(*text));
    if (text == NULL)
    {
        return NULL;
    }

    text->base.rect = (SDL_FRect){x, y, 0.0F, 0.0F};
    text->base.ops = &TEXT_OPS;
    text->base.visible = true;
    text->base.enabled = false;
    text->color = color;
    text->content = content;

    return text;
}
