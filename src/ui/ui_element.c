#include "ui/ui_element.h"

#include <stddef.h>

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

static SDL_FRect compute_screen_rect(const ui_element *element, size_t depth)
{
    if (element == NULL)
    {
        return (SDL_FRect){0.0F, 0.0F, 0.0F, 0.0F};
    }

    static const size_t MAX_PARENT_CHAIN_DEPTH = 256U;
    if (depth > MAX_PARENT_CHAIN_DEPTH)
    {
        return (SDL_FRect){0.0F, 0.0F, element->rect.w, element->rect.h};
    }

    if (element->parent == NULL)
    {
        return element->rect;
    }

    const SDL_FRect parent_sr = compute_screen_rect(element->parent, depth + 1U);
    float abs_x;
    float abs_y;

    switch (element->align_h)
    {
    case UI_ALIGN_CENTER_H:
        abs_x = parent_sr.x + (parent_sr.w - element->rect.w) * 0.5F + element->rect.x;
        break;
    case UI_ALIGN_RIGHT:
        abs_x = parent_sr.x + parent_sr.w - element->rect.w - element->rect.x;
        break;
    default: /* UI_ALIGN_LEFT */
        abs_x = parent_sr.x + element->rect.x;
        break;
    }

    switch (element->align_v)
    {
    case UI_ALIGN_CENTER_V:
        abs_y = parent_sr.y + (parent_sr.h - element->rect.h) * 0.5F + element->rect.y;
        break;
    case UI_ALIGN_BOTTOM:
        abs_y = parent_sr.y + parent_sr.h - element->rect.h - element->rect.y;
        break;
    default: /* UI_ALIGN_TOP */
        abs_y = parent_sr.y + element->rect.y;
        break;
    }

    return (SDL_FRect){abs_x, abs_y, element->rect.w, element->rect.h};
}

SDL_FRect ui_element_screen_rect(const ui_element *element)
{
    return compute_screen_rect(element, 0U);
}

bool ui_element_hit_test(const ui_element *element, const SDL_FPoint *point)
{
    if (element == NULL || point == NULL)
    {
        return false;
    }

    const SDL_FRect sr = ui_element_screen_rect(element);
    return SDL_PointInRectFloat(point, &sr);
}
