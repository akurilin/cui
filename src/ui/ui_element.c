#include "ui/ui_element.h"

bool is_point_in_rect(float cursor_x, float cursor_y, const SDL_FRect *rect) {
    return cursor_x >= rect->x && cursor_x <= rect->x + rect->w && cursor_y >= rect->y &&
           cursor_y <= rect->y + rect->h;
}
