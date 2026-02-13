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
    const char *content;
} ui_text;

/*
 * Create a text element at fixed coordinates.
 *
 * Content lifetime: content pointer is borrowed, not copied, so the pointed-to
 * string must remain valid for as long as the element is alive.
 */
ui_text *ui_text_create(float x, float y, const char *content, SDL_Color color);

#endif
