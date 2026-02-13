#ifndef UI_PANE_H
#define UI_PANE_H

#include "ui/ui_element.h"

/*
 * Simple rectangular background/border element used to visually group content.
 */
typedef struct ui_pane {
    ui_element base;
    SDL_Color fill_color;
    SDL_Color border_color;
} ui_pane;

/*
 * Create a pane covering rect with fill and border colors.
 *
 * Why this exists: it provides a reusable container-like visual primitive
 * without requiring ad-hoc draw calls in main render code.
 */
ui_pane *create_pane(const SDL_FRect *rect, SDL_Color fill_color, SDL_Color border_color);

#endif
