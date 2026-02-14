#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "ui/ui_element.h"

/*
 * Lightweight text element rendered with SDL_RenderDebugText.
 */
typedef struct ui_text
{
    ui_element base;
    SDL_Color color;
    char *content;
} ui_text;

/*
 * Create a text element at fixed coordinates.
 *
 * Content ownership: the input content string is copied into element-owned
 * storage, so the caller retains ownership of the input pointer.
 * border_color is optional; pass NULL for no border.
 */
ui_text *ui_text_create(float x, float y, const char *content, SDL_Color color,
                        const SDL_Color *border_color);

/*
 * Replace the text element content.
 *
 * Behavior:
 * - Copies the provided string into element-owned storage.
 * - Updates the element width to fit the new content using debug glyph metrics.
 *
 * Returns false on invalid arguments or allocation failure.
 */
bool ui_text_set_content(ui_text *text, const char *content);

/*
 * Read the current text content.
 *
 * Returns a pointer to element-owned storage valid for the element lifetime.
 */
const char *ui_text_get_content(const ui_text *text);

#endif
