#ifndef UI_FPS_COUNTER_H
#define UI_FPS_COUNTER_H

#include "ui/ui_element.h"

/*
 * Live FPS label element anchored near the viewport's bottom-right corner.
 *
 * Why this exists: it provides built-in performance feedback during UI iteration
 * without adding manual timing code to each app screen.
 */
typedef struct ui_fps_counter
{
    ui_element base;
    SDL_Color color;
    int viewport_width;
    int viewport_height;
    float update_interval_seconds;
    float elapsed_seconds;
    int frame_count;
    float displayed_fps;
    float padding;
    char label[32];
} ui_fps_counter;

/*
 * Create a bottom-right FPS counter element.
 *
 * Parameters:
 * - viewport_width/viewport_height: dimensions used for anchoring
 * - padding: inset from right/bottom edges in pixels
 * - color: text color
 *
 * Update requirement: ui_context_update must be called each frame with a real
 * delta_seconds value, otherwise FPS output will not be meaningful.
 */
ui_fps_counter *create_fps_counter(int viewport_width, int viewport_height, float padding,
                                   SDL_Color color);

#endif
