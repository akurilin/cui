#ifndef UI_TEXT_INPUT_H
#define UI_TEXT_INPUT_H

#include "ui/ui_element.h"

/*
 * Maximum buffer size for text input content (including null terminator).
 * The actual usable character count may be smaller depending on the element
 * width, since characters beyond the visible area are not accepted.
 */
#define UI_TEXT_INPUT_BUFFER_SIZE 256

/*
 * Callback invoked when focused input receives Enter/Return.
 *
 * Parameters:
 * - value: current null-terminated input text
 * - context: caller-supplied opaque pointer
 */
typedef void (*text_input_submit_handler)(const char *value, void *context);

/*
 * Single-line text input field with click-to-focus keyboard input.
 *
 * Focus model: ui_runtime-managed. Clicking a focusable text input focuses it,
 * and clicking elsewhere clears focus. ui_runtime notifies this element when
 * focus changes so SDL text input start/stop stays centralized and consistent.
 * When focused, a blinking caret is drawn at the end of the text.
 *
 * Editing: append-only. Characters are inserted at the end of the buffer.
 * Backspace deletes the last character. No cursor movement or selection.
 *
 * The text input always renders a border; the border color changes to
 * indicate focus state.
 */
typedef struct ui_text_input
{
    ui_element base;
    char buffer[UI_TEXT_INPUT_BUFFER_SIZE];
    char *placeholder;
    size_t length;
    size_t max_length;
    bool is_focused;
    SDL_Color text_color;
    SDL_Color placeholder_color;
    SDL_Color background_color;
    SDL_Color focused_border_color;
    SDL_Color unfocused_border_color;
    float caret_blink_timer;
    SDL_Window *window;
    text_input_submit_handler on_submit;
    void *on_submit_context;
} ui_text_input;

/*
 * Create a single-line text input field.
 *
 * Parameters:
 * - rect: position and size in window coordinates. Width determines the
 *   initial maximum number of visible (and typeable) characters.
 * - text_color: color used for rendered text and the caret.
 * - background_color: fill color of the input area.
 * - border_color: border color when the field is not focused.
 * - focused_border_color: border color when the field has keyboard focus.
 * - placeholder: optional placeholder text shown when value is empty and the
 *   field is unfocused. The string is copied into element-owned storage.
 *   Pass NULL for no placeholder.
 * - placeholder_color: color used to render placeholder text.
 * - window: SDL window handle required by the SDL text input subsystem.
 * - on_submit/on_submit_context: optional callback fired on Enter/Return.
 *
 * Returns a heap-allocated text input or NULL on allocation/validation
 * failure. Ownership transfers to caller (or to ui_runtime after a
 * successful ui_runtime_add call).
 */
ui_text_input *ui_text_input_create(const SDL_FRect *rect, SDL_Color text_color,
                                    SDL_Color background_color, SDL_Color border_color,
                                    SDL_Color focused_border_color, const char *placeholder,
                                    SDL_Color placeholder_color, SDL_Window *window,
                                    text_input_submit_handler on_submit, void *on_submit_context);

/*
 * Get the current text content of the input field.
 *
 * Returns a pointer to the internal null-terminated buffer. The pointer
 * remains valid for the lifetime of the ui_text_input. Do not free.
 */
const char *ui_text_input_get_value(const ui_text_input *input);

/*
 * Replace the text content.
 *
 * Behavior:
 * - Copies value into the internal buffer.
 * - Truncates to current max visible length (derived from latest arranged width).
 *
 * Returns false on invalid arguments.
 */
bool ui_text_input_set_value(ui_text_input *input, const char *value);

/*
 * Replace the placeholder text.
 *
 * Behavior:
 * - Copies placeholder into element-owned storage.
 * - Passing NULL clears the placeholder.
 *
 * Returns false on invalid input or allocation failure.
 */
bool ui_text_input_set_placeholder(ui_text_input *input, const char *placeholder);

/*
 * Clear the text content (sets value to "").
 */
void ui_text_input_clear(ui_text_input *input);

/*
 * Return current focus state.
 */
bool ui_text_input_is_focused(const ui_text_input *input);

#endif
