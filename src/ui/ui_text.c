#include "ui/ui_text.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;

static char *duplicate_text_content(const char *content)
{
    if (content == NULL)
    {
        return NULL;
    }

    const size_t length = strlen(content);
    char *copy = malloc(length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, content, length + 1U);
    return copy;
}

static bool handle_text_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
    return false;
}

static void update_text(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_text(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_text *text = (const ui_text *)element;

    if (text->content == NULL)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, text->color.r, text->color.g, text->color.b, text->color.a);
    SDL_RenderDebugText(renderer, text->base.rect.x, text->base.rect.y, text->content);
    if (text->base.has_border)
    {
        ui_element_render_inner_border(renderer, &text->base.rect, text->base.border_color,
                                       text->base.border_width);
    }
}

static void destroy_text(ui_element *element)
{
    ui_text *text = (ui_text *)element;
    free(text->content);
    free(text);
}

static const ui_element_ops TEXT_OPS = {
    .handle_event = handle_text_event,
    .update = update_text,
    .render = render_text,
    .destroy = destroy_text,
};

ui_text *ui_text_create(float x, float y, const char *content, SDL_Color color,
                        const SDL_Color *border_color)
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

    char *content_copy = duplicate_text_content(content);
    if (content_copy == NULL)
    {
        free(text);
        return NULL;
    }

    const float text_width = (float)strlen(content_copy) * DEBUG_GLYPH_WIDTH;
    text->base.rect = (SDL_FRect){x, y, text_width, DEBUG_GLYPH_HEIGHT};
    text->base.ops = &TEXT_OPS;
    text->base.visible = true;
    text->base.enabled = false;
    ui_element_set_border(&text->base, border_color, 1.0F);
    text->color = color;
    text->content = content_copy;

    return text;
}

bool ui_text_set_content(ui_text *text, const char *content)
{
    if (text == NULL || content == NULL)
    {
        return false;
    }

    char *content_copy = duplicate_text_content(content);
    if (content_copy == NULL)
    {
        return false;
    }

    free(text->content);
    text->content = content_copy;
    text->base.rect.w = (float)strlen(text->content) * DEBUG_GLYPH_WIDTH;
    text->base.rect.h = DEBUG_GLYPH_HEIGHT;
    return true;
}

const char *ui_text_get_content(const ui_text *text)
{
    if (text == NULL || text->content == NULL)
    {
        return "";
    }
    return text->content;
}
