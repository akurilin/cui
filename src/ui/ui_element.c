#include "ui/ui_element.h"

void ui_element_set_border(ui_element *element, const SDL_Color *border_color, float width)
{
    if (element == NULL)
    {
        return;
    }

    if (border_color == NULL)
    {
        ui_element_clear_border(element);
        return;
    }

    element->has_border = true;
    element->border_color = *border_color;
    element->border_width = width >= 1.0F ? width : 1.0F;
}

void ui_element_clear_border(ui_element *element)
{
    if (element == NULL)
    {
        return;
    }

    element->has_border = false;
    element->border_color = (SDL_Color){0, 0, 0, 0};
    element->border_width = 0.0F;
}

void ui_element_render_inner_border(SDL_Renderer *renderer, const SDL_FRect *rect, SDL_Color color,
                                    float width)
{
    if (renderer == NULL || rect == NULL || rect->w <= 0.0F || rect->h <= 0.0F || width <= 0.0F)
    {
        return;
    }

    const float max_width = (rect->w < rect->h ? rect->w : rect->h) * 0.5F;
    const float clamped_width = width > max_width ? max_width : width;
    if (clamped_width <= 0.0F)
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const SDL_FRect top = {rect->x, rect->y, rect->w, clamped_width};
    const SDL_FRect bottom = {rect->x, rect->y + rect->h - clamped_width, rect->w, clamped_width};
    const SDL_FRect left = {rect->x, rect->y + clamped_width, clamped_width,
                            rect->h - (2.0F * clamped_width)};
    const SDL_FRect right = {rect->x + rect->w - clamped_width, rect->y + clamped_width,
                             clamped_width, rect->h - (2.0F * clamped_width)};

    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bottom);
    if (left.h > 0.0F)
    {
        SDL_RenderFillRect(renderer, &left);
    }
    if (right.h > 0.0F)
    {
        SDL_RenderFillRect(renderer, &right);
    }
}
