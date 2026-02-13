#include "ui/ui_checkbox.h"

#include <stdlib.h>
#include <string.h>

// The indicator box is always 16x16 pixels.
static const float BOX_SIZE = 16.0F;

// Horizontal gap between the box and the label text.
static const float LABEL_GAP = 6.0F;

// SDL_RenderDebugText uses 8x8 pixel glyphs.
static const float DEBUG_CHAR_W = 8.0F;
static const float DEBUG_CHAR_H = 8.0F;

// Inset in pixels from the box edges for the check-mark lines.
static const float CHECK_INSET = 3.0F;

static void handle_checkbox_event(ui_element *element, const SDL_Event *event)
{
    ui_checkbox *checkbox = (ui_checkbox *)element;

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        if (SDL_PointInRectFloat(&cursor, &checkbox->base.rect))
        {
            checkbox->is_pressed = true;
        }
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
    {
        const bool was_pressed = checkbox->is_pressed;
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        const bool is_inside = SDL_PointInRectFloat(&cursor, &checkbox->base.rect);

        checkbox->is_pressed = false;
        if (was_pressed && is_inside)
        {
            checkbox->is_checked = !checkbox->is_checked;
            if (checkbox->on_change != NULL)
            {
                checkbox->on_change(checkbox->is_checked, checkbox->on_change_context);
            }
        }
    }
}

static void update_checkbox(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_checkbox(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_checkbox *checkbox = (const ui_checkbox *)element;
    const float box_x = checkbox->base.rect.x;
    const float box_y = checkbox->base.rect.y;

    // Draw the box outline.
    const SDL_FRect box_rect = {box_x, box_y, BOX_SIZE, BOX_SIZE};
    SDL_SetRenderDrawColor(renderer, checkbox->box_color.r, checkbox->box_color.g,
                           checkbox->box_color.b, checkbox->box_color.a);
    SDL_RenderRect(renderer, &box_rect);

    // Draw an X inside the box when checked.
    if (checkbox->is_checked)
    {
        SDL_SetRenderDrawColor(renderer, checkbox->check_color.r, checkbox->check_color.g,
                               checkbox->check_color.b, checkbox->check_color.a);
        SDL_RenderLine(renderer, box_x + CHECK_INSET, box_y + CHECK_INSET,
                       box_x + BOX_SIZE - CHECK_INSET, box_y + BOX_SIZE - CHECK_INSET);
        SDL_RenderLine(renderer, box_x + BOX_SIZE - CHECK_INSET, box_y + CHECK_INSET,
                       box_x + CHECK_INSET, box_y + BOX_SIZE - CHECK_INSET);
    }

    // Draw the label to the right of the box, vertically centered.
    const float label_x = box_x + BOX_SIZE + LABEL_GAP;
    const float label_y = box_y + ((BOX_SIZE - DEBUG_CHAR_H) / 2.0F);
    SDL_SetRenderDrawColor(renderer, checkbox->label_color.r, checkbox->label_color.g,
                           checkbox->label_color.b, checkbox->label_color.a);
    SDL_RenderDebugText(renderer, label_x, label_y, checkbox->label);
}

static void destroy_checkbox(ui_element *element) { free(element); }

static const ui_element_ops CHECKBOX_OPS = {
    .handle_event = handle_checkbox_event,
    .update = update_checkbox,
    .render = render_checkbox,
    .destroy = destroy_checkbox,
};

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
ui_checkbox *ui_checkbox_create(float x, float y, const char *label, SDL_Color box_color,
                                SDL_Color check_color, SDL_Color label_color,
                                bool initially_checked, checkbox_change_handler on_change,
                                void *on_change_context)
{
    if (label == NULL)
    {
        return NULL;
    }

    ui_checkbox *checkbox = malloc(sizeof(*checkbox));
    if (checkbox == NULL)
    {
        return NULL;
    }

    const float label_width = (float)strlen(label) * DEBUG_CHAR_W;
    const float total_width = BOX_SIZE + LABEL_GAP + label_width;

    checkbox->base.rect = (SDL_FRect){x, y, total_width, BOX_SIZE};
    checkbox->base.ops = &CHECKBOX_OPS;
    checkbox->base.visible = true;
    checkbox->base.enabled = true;
    checkbox->box_color = box_color;
    checkbox->check_color = check_color;
    checkbox->label_color = label_color;
    checkbox->is_checked = initially_checked;
    checkbox->is_pressed = false;
    checkbox->label = label;
    checkbox->on_change = on_change;
    checkbox->on_change_context = on_change_context;

    return checkbox;
}
