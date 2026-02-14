#include "ui/ui_button.h"

#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;
static const SDL_Color BUTTON_TEXT_COLOR_WHITE = {255, 255, 255, 255};

// Hit-testing uses SDL3's SDL_PointInRectFloat (from SDL_rect.h) instead of a
// hand-rolled helper.  SDL handles the inclusive-bounds check for us and stays
// consistent with any future coordinate-space changes (e.g. logical presentation).
static void handle_button_event(ui_element *element, const SDL_Event *event)
{
    ui_button *button = (ui_button *)element;

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        if (SDL_PointInRectFloat(&cursor, &button->base.rect))
        {
            button->is_pressed = true;
        }
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
    {
        const bool was_pressed = button->is_pressed;
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        const bool is_inside = SDL_PointInRectFloat(&cursor, &button->base.rect);

        button->is_pressed = false;
        if (was_pressed && is_inside && button->on_click != NULL)
        {
            button->on_click(button->on_click_context);
        }
    }
}

static void update_button(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_button(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_button *button = (const ui_button *)element;
    const SDL_Color fill_color = button->is_pressed ? button->down_color : button->up_color;

    SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
    SDL_RenderFillRect(renderer, &button->base.rect);

    if (button->label != NULL && button->label[0] != '\0')
    {
        const float label_width = (float)strlen(button->label) * DEBUG_GLYPH_WIDTH;
        const float label_x = button->base.rect.x + ((button->base.rect.w - label_width) * 0.5F);
        const float label_y =
            button->base.rect.y + ((button->base.rect.h - DEBUG_GLYPH_HEIGHT) * 0.5F);

        SDL_SetRenderDrawColor(renderer, BUTTON_TEXT_COLOR_WHITE.r, BUTTON_TEXT_COLOR_WHITE.g,
                               BUTTON_TEXT_COLOR_WHITE.b, BUTTON_TEXT_COLOR_WHITE.a);
        SDL_RenderDebugText(renderer, label_x, label_y, button->label);
    }

    if (button->base.has_border)
    {
        ui_element_render_inner_border(renderer, &button->base.rect, button->base.border_color,
                                       button->base.border_width);
    }
}

static void destroy_button(ui_element *element) { free(element); }

static const ui_element_ops BUTTON_OPS = {
    .handle_event = handle_button_event,
    .update = update_button,
    .render = render_button,
    .destroy = destroy_button,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_button *ui_button_create(const SDL_FRect *rect, SDL_Color up_color, SDL_Color down_color,
                            const char *label, const SDL_Color *border_color,
                            button_click_handler on_click, void *on_click_context)
{
    if (rect == NULL)
    {
        return NULL;
    }

    ui_button *button = malloc(sizeof(*button));
    if (button == NULL)
    {
        return NULL;
    }

    button->base.rect = *rect;
    button->base.ops = &BUTTON_OPS;
    button->base.visible = true;
    button->base.enabled = true;
    ui_element_set_border(&button->base, border_color, 1.0F);
    button->up_color = up_color;
    button->down_color = down_color;
    button->label = label;
    button->is_pressed = false;
    button->on_click = on_click;
    button->on_click_context = on_click_context;

    return button;
}
