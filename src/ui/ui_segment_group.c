#include "ui/ui_segment_group.h"

#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;

static size_t clamp_segment_index(const ui_segment_group *group, size_t index)
{
    if (index >= group->segment_count)
    {
        return group->segment_count - 1U;
    }
    return index;
}

static size_t segment_index_from_x(const ui_segment_group *group, float x, const SDL_FRect *sr)
{
    if (x <= sr->x)
    {
        return 0U;
    }
    if (x >= sr->x + sr->w)
    {
        return group->segment_count - 1U;
    }

    const float relative_x = x - sr->x;
    const float normalized = relative_x / sr->w;
    const float scaled = normalized * (float)group->segment_count;
    return clamp_segment_index(group, (size_t)scaled);
}

static void set_selected_index_internal(ui_segment_group *group, size_t selected_index, bool notify)
{
    const size_t clamped = clamp_segment_index(group, selected_index);
    const bool changed = group->selected_index != clamped;
    group->selected_index = clamped;

    if (changed && notify && group->on_change != NULL)
    {
        group->on_change(group->selected_index, group->labels[group->selected_index],
                         group->on_change_context);
    }
}

static bool handle_segment_group_event(ui_element *element, const SDL_Event *event)
{
    ui_segment_group *group = (ui_segment_group *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        const SDL_FPoint cursor = {event->button.x, event->button.y};
        if (!SDL_PointInRectFloat(&cursor, &sr))
        {
            return false;
        }

        group->pressed_index = segment_index_from_x(group, event->button.x, &sr);
        group->has_pressed_segment = true;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT)
    {
        const bool was_pressed = group->has_pressed_segment;
        if (!was_pressed)
        {
            return false;
        }

        const SDL_FPoint cursor = {event->button.x, event->button.y};
        const bool is_inside = SDL_PointInRectFloat(&cursor, &sr);
        if (is_inside)
        {
            const size_t selected = segment_index_from_x(group, event->button.x, &sr);
            set_selected_index_internal(group, selected, true);
        }

        group->has_pressed_segment = false;
        return true;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION && group->has_pressed_segment)
    {
        const SDL_FPoint cursor = {event->motion.x, event->motion.y};
        if (SDL_PointInRectFloat(&cursor, &sr))
        {
            group->pressed_index = segment_index_from_x(group, event->motion.x, &sr);
        }
        return true;
    }

    return false;
}

static void update_segment_group(ui_element *element, float delta_seconds)
{
    (void)element;
    (void)delta_seconds;
}

static void render_segment_group(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_segment_group *group = (const ui_segment_group *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);
    const float segment_width = sr.w / (float)group->segment_count;

    for (size_t i = 0; i < group->segment_count; ++i)
    {
        const float segment_x = sr.x + (segment_width * (float)i);
        float width = segment_width;
        if (i == group->segment_count - 1U)
        {
            width = (sr.x + sr.w) - segment_x;
        }

        const SDL_FRect segment_rect = {segment_x, sr.y, width, sr.h};
        const bool is_selected = i == group->selected_index;
        const bool is_pressed = group->has_pressed_segment && i == group->pressed_index;
        const SDL_Color fill_color =
            is_pressed ? group->pressed_color
                       : (is_selected ? group->selected_color : group->base_color);
        const SDL_Color label_color = is_selected ? group->selected_text_color : group->text_color;

        SDL_SetRenderDrawColor(renderer, fill_color.r, fill_color.g, fill_color.b, fill_color.a);
        SDL_RenderFillRect(renderer, &segment_rect);

        const char *label = group->labels[i];
        if (label != NULL && label[0] != '\0')
        {
            const float label_width = (float)strlen(label) * DEBUG_GLYPH_WIDTH;
            const float label_x = segment_rect.x + ((segment_rect.w - label_width) * 0.5F);
            const float label_y = segment_rect.y + ((segment_rect.h - DEBUG_GLYPH_HEIGHT) * 0.5F);

            SDL_SetRenderDrawColor(renderer, label_color.r, label_color.g, label_color.b,
                                   label_color.a);
            SDL_RenderDebugText(renderer, label_x, label_y, label);
        }
    }

    const SDL_Color separator_color =
        group->base.has_border ? group->base.border_color : group->text_color;
    SDL_SetRenderDrawColor(renderer, separator_color.r, separator_color.g, separator_color.b,
                           separator_color.a);
    for (size_t i = 1; i < group->segment_count; ++i)
    {
        const float separator_x = sr.x + (segment_width * (float)i);
        SDL_RenderLine(renderer, separator_x, sr.y, separator_x, sr.y + sr.h);
    }

    if (group->base.has_border)
    {
        ui_element_render_inner_border(renderer, &sr, group->base.border_color,
                                       group->base.border_width);
    }
}

static void destroy_segment_group(ui_element *element) { free(element); }

static const ui_element_ops SEGMENT_GROUP_OPS = {
    .handle_event = handle_segment_group_event,
    .update = update_segment_group,
    .render = render_segment_group,
    .destroy = destroy_segment_group,
};

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
ui_segment_group *
ui_segment_group_create(const SDL_FRect *rect, const char **labels, size_t segment_count,
                        size_t initial_selected_index, SDL_Color base_color,
                        SDL_Color selected_color, SDL_Color pressed_color, SDL_Color text_color,
                        SDL_Color selected_text_color, const SDL_Color *border_color,
                        segment_group_change_handler on_change, void *on_change_context)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (rect == NULL || rect->w <= 0.0F || rect->h <= 0.0F || labels == NULL ||
        segment_count == 0U || initial_selected_index >= segment_count)
    {
        return NULL;
    }

    for (size_t i = 0; i < segment_count; ++i)
    {
        if (labels[i] == NULL)
        {
            return NULL;
        }
    }

    ui_segment_group *group = malloc(sizeof(*group));
    if (group == NULL)
    {
        return NULL;
    }

    group->base.rect = *rect;
    group->base.ops = &SEGMENT_GROUP_OPS;
    group->base.visible = true;
    group->base.enabled = true;
    group->base.parent = NULL;
    group->base.align_h = UI_ALIGN_LEFT;
    group->base.align_v = UI_ALIGN_TOP;
    ui_element_set_border(&group->base, border_color, 1.0F);
    group->labels = labels;
    group->segment_count = segment_count;
    group->selected_index = initial_selected_index;
    group->has_pressed_segment = false;
    group->pressed_index = initial_selected_index;
    group->base_color = base_color;
    group->selected_color = selected_color;
    group->pressed_color = pressed_color;
    group->text_color = text_color;
    group->selected_text_color = selected_text_color;
    group->on_change = on_change;
    group->on_change_context = on_change_context;

    return group;
}

size_t ui_segment_group_get_selected_index(const ui_segment_group *group)
{
    if (group == NULL)
    {
        return 0U;
    }
    return group->selected_index;
}

const char *ui_segment_group_get_selected_label(const ui_segment_group *group)
{
    if (group == NULL || group->labels == NULL || group->selected_index >= group->segment_count)
    {
        return "";
    }
    return group->labels[group->selected_index];
}

bool ui_segment_group_set_selected_index(ui_segment_group *group, size_t selected_index,
                                         bool notify)
{
    if (group == NULL || selected_index >= group->segment_count)
    {
        return false;
    }

    set_selected_index_internal(group, selected_index, notify);
    return true;
}
