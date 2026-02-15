#include "ui/ui_hrule.h"

#include <stdlib.h>

static bool handle_hrule_event(ui_element *element, const SDL_Event *event)
{
    (void)element;
    (void)event;
    return false;
}

static void update_hrule(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_hrule(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_hrule *rule = (const ui_hrule *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);

    const float inset = rule->inset_fraction * sr.w;
    const SDL_FRect line_rect = {
        sr.x + inset,
        sr.y + (sr.h - 1.0F) / 2.0F,
        sr.w - 2.0F * inset,
        1.0F,
    };

    SDL_SetRenderDrawColor(renderer, rule->color.r, rule->color.g, rule->color.b, rule->color.a);
    SDL_RenderFillRect(renderer, &line_rect);
}

static void destroy_hrule(ui_element *element) { free(element); }

static const ui_element_ops HRULE_OPS = {
    .handle_event = handle_hrule_event,
    .update = update_hrule,
    .render = render_hrule,
    .destroy = destroy_hrule,
};

ui_hrule *ui_hrule_create(float thickness, SDL_Color color, float inset_fraction)
{
    ui_hrule *rule = malloc(sizeof(*rule));
    if (rule == NULL)
    {
        return NULL;
    }

    rule->base.rect = (SDL_FRect){0.0F, 0.0F, 0.0F, thickness};
    rule->base.ops = &HRULE_OPS;
    rule->base.visible = true;
    rule->base.enabled = true;
    rule->base.parent = NULL;
    rule->base.align_h = UI_ALIGN_LEFT;
    rule->base.align_v = UI_ALIGN_TOP;
    ui_element_clear_border(&rule->base);
    rule->color = color;
    rule->inset_fraction = inset_fraction;

    return rule;
}
