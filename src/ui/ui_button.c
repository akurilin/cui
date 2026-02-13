#include "ui/ui_button.h"

#include <stdlib.h>

static void handle_button_event(ui_element *element, const SDL_Event *event)
{
    ui_button *button = (ui_button *)element;

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        if (is_point_in_rect(event->button.x, event->button.y, &button->base.rect))
        {
            button->is_pressed = true;
        }
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
    {
        const bool was_pressed = button->is_pressed;
        const bool is_inside =
            is_point_in_rect(event->button.x, event->button.y, &button->base.rect);

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

    SDL_SetRenderDrawColor(renderer, button->border_color.r, button->border_color.g,
                           button->border_color.b, button->border_color.a);
    SDL_RenderRect(renderer, &button->base.rect);
}

static void destroy_button(ui_element *element) { free(element); }

static const ui_element_ops BUTTON_OPS = {
    .handle_event = handle_button_event,
    .update = update_button,
    .render = render_button,
    .destroy = destroy_button,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_button *create_button(const SDL_FRect *rect, SDL_Color up_color, SDL_Color down_color,
                         SDL_Color border_color, button_click_handler on_click,
                         void *on_click_context)
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
    button->up_color = up_color;
    button->down_color = down_color;
    button->border_color = border_color;
    button->is_pressed = false;
    button->on_click = on_click;
    button->on_click_context = on_click_context;

    return button;
}
