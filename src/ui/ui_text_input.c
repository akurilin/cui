#include "ui/ui_text_input.h"

#include "util/string_util.h"

#include <stdlib.h>
#include <string.h>

static const float DEBUG_GLYPH_WIDTH = 8.0F;
static const float DEBUG_GLYPH_HEIGHT = 8.0F;
static const float TEXT_PADDING = 4.0F;
static const float CARET_BLINK_PERIOD = 1.0F;
static const float CARET_BLINK_HALF = 0.5F;
static const float CARET_WIDTH = 2.0F;
static const float HALF = 0.5F;
static const float PADDING_SIDES = 2.0F;

static size_t compute_max_visible_chars(float width)
{
    if (width <= 0.0F)
    {
        return 0U;
    }

    const float usable_width = width - (PADDING_SIDES * TEXT_PADDING);
    if (usable_width <= 0.0F)
    {
        return 0U;
    }

    size_t max_visible = (size_t)(usable_width / DEBUG_GLYPH_WIDTH);
    if (max_visible > UI_TEXT_INPUT_BUFFER_SIZE - 1U)
    {
        max_visible = UI_TEXT_INPUT_BUFFER_SIZE - 1U;
    }
    return max_visible;
}

static void update_visible_capacity(ui_text_input *input)
{
    if (input == NULL)
    {
        return;
    }

    input->max_length = compute_max_visible_chars(input->base.rect.w);
    if (input->length > input->max_length)
    {
        input->length = input->max_length;
        input->buffer[input->length] = '\0';
    }
}

static void set_focus(ui_text_input *input, bool focused)
{
    if (input == NULL || input->is_focused == focused)
    {
        return;
    }

    input->is_focused = focused;
    input->caret_blink_timer = 0.0F;
    input->base.border_color =
        focused ? input->focused_border_color : input->unfocused_border_color;

    if (focused)
    {
        SDL_StartTextInput(input->window);
    }
    else
    {
        SDL_StopTextInput(input->window);
    }
}

static bool can_focus_text_input(const ui_element *element) { return element != NULL; }

static void set_text_input_focus(ui_element *element, bool focused)
{
    set_focus((ui_text_input *)element, focused);
}

static bool handle_text_input_event(ui_element *element, const SDL_Event *event)
{
    ui_text_input *input = (ui_text_input *)element;

    if (!input->is_focused)
    {
        return false;
    }

    if (event->type == SDL_EVENT_TEXT_INPUT)
    {
        const char *text = event->text.text;
        size_t text_len = strlen(text);
        const size_t available = input->max_length - input->length;
        if (text_len > available)
        {
            text_len = available;
        }
        if (text_len > 0)
        {
            memcpy(input->buffer + input->length, text, text_len);
            input->length += text_len;
            input->buffer[input->length] = '\0';
            input->caret_blink_timer = 0.0F;
        }
        return true;
    }

    if (event->type == SDL_EVENT_KEY_DOWN)
    {
        if (event->key.key == SDLK_BACKSPACE)
        {
            if (input->length > 0)
            {
                input->length--;
                input->buffer[input->length] = '\0';
                input->caret_blink_timer = 0.0F;
            }
            return true;
        }

        if (event->key.key == SDLK_RETURN || event->key.key == SDLK_KP_ENTER)
        {
            if (input->on_submit != NULL)
            {
                input->on_submit(input->buffer, input->on_submit_context);
            }
            return true;
        }
    }

    return false;
}

static void update_text_input(ui_element *element, float delta_seconds)
{
    ui_text_input *input = (ui_text_input *)element;

    if (input->is_focused)
    {
        input->caret_blink_timer += delta_seconds;
        if (input->caret_blink_timer >= CARET_BLINK_PERIOD)
        {
            input->caret_blink_timer -= CARET_BLINK_PERIOD;
        }
    }
}

static void measure_text_input(ui_element *element, const SDL_FRect *available_rect)
{
    (void)available_rect;

    ui_text_input *input = (ui_text_input *)element;
    if (input == NULL)
    {
        return;
    }

    if (input->base.rect.h < DEBUG_GLYPH_HEIGHT + (PADDING_SIDES * TEXT_PADDING))
    {
        input->base.rect.h = DEBUG_GLYPH_HEIGHT + (PADDING_SIDES * TEXT_PADDING);
    }

    update_visible_capacity(input);
}

static void arrange_text_input(ui_element *element, const SDL_FRect *final_rect)
{
    ui_text_input *input = (ui_text_input *)element;
    if (input == NULL || final_rect == NULL)
    {
        return;
    }

    input->base.rect = *final_rect;
    update_visible_capacity(input);
}

static void render_text_input(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_text_input *input = (const ui_text_input *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);

    // Background fill.
    SDL_SetRenderDrawColor(renderer, input->background_color.r, input->background_color.g,
                           input->background_color.b, input->background_color.a);
    SDL_RenderFillRect(renderer, &sr);

    const float text_x = sr.x + TEXT_PADDING;
    const float text_y = sr.y + ((sr.h - DEBUG_GLYPH_HEIGHT) * HALF);

    // Text content.
    if (input->length > 0)
    {
        SDL_SetRenderDrawColor(renderer, input->text_color.r, input->text_color.g,
                               input->text_color.b, input->text_color.a);
        SDL_RenderDebugText(renderer, text_x, text_y, input->buffer);
    }
    else if (!input->is_focused && input->placeholder != NULL && input->placeholder[0] != '\0')
    {
        SDL_SetRenderDrawColor(renderer, input->placeholder_color.r, input->placeholder_color.g,
                               input->placeholder_color.b, input->placeholder_color.a);
        SDL_RenderDebugText(renderer, text_x, text_y, input->placeholder);
    }

    // Blinking caret when focused (visible during the first half of the blink cycle).
    if (input->is_focused && input->caret_blink_timer < CARET_BLINK_HALF)
    {
        const float caret_x = text_x + ((float)input->length * DEBUG_GLYPH_WIDTH);
        const SDL_FRect caret_rect = {caret_x, text_y, CARET_WIDTH, DEBUG_GLYPH_HEIGHT};

        SDL_SetRenderDrawColor(renderer, input->text_color.r, input->text_color.g,
                               input->text_color.b, input->text_color.a);
        SDL_RenderFillRect(renderer, &caret_rect);
    }

    // Border (always enabled for text inputs).
    if (input->base.has_border)
    {
        ui_element_render_inner_border(renderer, &sr, input->base.border_color,
                                       input->base.border_width);
    }
}

static void destroy_text_input(ui_element *element)
{
    ui_text_input *input = (ui_text_input *)element;

    if (input->is_focused)
    {
        SDL_StopTextInput(input->window);
    }
    free(input->placeholder);
    free(element);
}

static const ui_element_ops TEXT_INPUT_OPS = {
    .measure = measure_text_input,
    .arrange = arrange_text_input,
    .handle_event = handle_text_input_event,
    .can_focus = can_focus_text_input,
    .set_focus = set_text_input_focus,
    .update = update_text_input,
    .render = render_text_input,
    .destroy = destroy_text_input,
};

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
ui_text_input *ui_text_input_create(const SDL_FRect *rect, SDL_Color text_color,
                                    SDL_Color background_color, SDL_Color border_color,
                                    SDL_Color focused_border_color, const char *placeholder,
                                    SDL_Color placeholder_color, SDL_Window *window,
                                    text_input_submit_handler on_submit, void *on_submit_context)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (rect == NULL || rect->w <= 0.0F || rect->h <= 0.0F || window == NULL)
    {
        return NULL;
    }

    ui_text_input *input = malloc(sizeof(*input));
    if (input == NULL)
    {
        return NULL;
    }

    input->base.rect = *rect;
    input->base.ops = &TEXT_INPUT_OPS;
    input->base.visible = true;
    input->base.enabled = true;
    input->base.parent = NULL;
    input->base.align_h = UI_ALIGN_LEFT;
    input->base.align_v = UI_ALIGN_TOP;
    ui_element_set_border(&input->base, &border_color, 1.0F);

    input->buffer[0] = '\0';
    input->placeholder = NULL;
    input->length = 0;

    update_visible_capacity(input);

    input->is_focused = false;
    input->text_color = text_color;
    input->placeholder_color = placeholder_color;
    input->background_color = background_color;
    input->focused_border_color = focused_border_color;
    input->unfocused_border_color = border_color;
    input->caret_blink_timer = 0.0F;
    input->window = window;
    input->on_submit = on_submit;
    input->on_submit_context = on_submit_context;

    if (!ui_text_input_set_placeholder(input, placeholder))
    {
        free(input);
        return NULL;
    }

    return input;
}

const char *ui_text_input_get_value(const ui_text_input *input)
{
    if (input == NULL)
    {
        return "";
    }
    return input->buffer;
}

bool ui_text_input_set_value(ui_text_input *input, const char *value)
{
    if (input == NULL || value == NULL)
    {
        return false;
    }

    size_t value_length = strlen(value);
    if (value_length > input->max_length)
    {
        value_length = input->max_length;
    }

    memcpy(input->buffer, value, value_length);
    input->buffer[value_length] = '\0';
    input->length = value_length;
    input->caret_blink_timer = 0.0F;
    return true;
}

bool ui_text_input_set_placeholder(ui_text_input *input, const char *placeholder)
{
    if (input == NULL)
    {
        return false;
    }

    if (placeholder == NULL)
    {
        free(input->placeholder);
        input->placeholder = NULL;
        return true;
    }

    char *placeholder_copy = duplicate_string(placeholder);
    if (placeholder_copy == NULL)
    {
        return false;
    }

    free(input->placeholder);
    input->placeholder = placeholder_copy;
    return true;
}

void ui_text_input_clear(ui_text_input *input)
{
    if (input == NULL)
    {
        return;
    }
    input->buffer[0] = '\0';
    input->length = 0;
    input->caret_blink_timer = 0.0F;
}

bool ui_text_input_is_focused(const ui_text_input *input)
{
    if (input == NULL)
    {
        return false;
    }
    return input->is_focused;
}
