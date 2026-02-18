#ifndef UI_SEGMENT_GROUP_H
#define UI_SEGMENT_GROUP_H

#include "ui/ui_element.h"

#include <stddef.h>

typedef struct ui_segment_group ui_segment_group;

/*
 * Callback invoked when the selected segment changes.
 *
 * Parameters:
 * - selected_index: zero-based index of the newly selected segment
 * - selected_label: borrowed label pointer for the selected segment
 * - context: caller-supplied opaque data pointer
 */
typedef void (*segment_group_change_handler)(size_t selected_index, const char *selected_label,
                                             void *context);

/*
 * Segmented control that allows selecting one option from N labels.
 *
 * The element rect defines the full clickable/rendered area. The control
 * renders equal-width segments and keeps exactly one active selection.
 *
 * Labels are borrowed pointers and are not copied by the widget.
 */
struct ui_segment_group
{
    ui_element base;
    const char **labels;
    size_t segment_count;
    size_t selected_index;
    bool has_pressed_segment;
    size_t pressed_index;
    SDL_Color base_color;
    SDL_Color selected_color;
    SDL_Color pressed_color;
    SDL_Color text_color;
    SDL_Color selected_text_color;
    segment_group_change_handler on_change;
    void *on_change_context;
};

/*
 * Create a segmented control.
 *
 * Behavior:
 * - Left mouse press inside stores a pressed segment.
 * - Left mouse release inside commits selection to the segment under cursor.
 * - When the selected segment changes, on_change is invoked if non-NULL.
 *
 * Parameters:
 * - rect: full control bounds in window coordinates
 * - labels: array of borrowed C strings with segment_count entries
 * - segment_count: number of selectable segments (must be >= 1)
 * - initial_selected_index: initially active segment index (must be in range)
 * - base_color: fill color for unselected segments
 * - selected_color: fill color for selected segment
 * - pressed_color: fill color for currently pressed segment during click
 * - text_color: label color for unselected segments
 * - selected_text_color: label color for selected segment
 * - border_color: optional border color around full control (NULL disables)
 * - on_change/on_change_context: optional selection callback + context
 *
 * Returns:
 * - Heap-allocated ui_segment_group on success
 * - NULL on invalid arguments or allocation failure
 *
 * Ownership:
 * - Caller owns the returned pointer until transferring ownership to ui_runtime
 *   via ui_runtime_add, after which ui_runtime_destroy releases it.
 */
ui_segment_group *
ui_segment_group_create(const SDL_FRect *rect, const char **labels, size_t segment_count,
                        size_t initial_selected_index, SDL_Color base_color,
                        SDL_Color selected_color, SDL_Color pressed_color, SDL_Color text_color,
                        SDL_Color selected_text_color, const SDL_Color *border_color,
                        segment_group_change_handler on_change, void *on_change_context);

/*
 * Read the current selected segment index.
 *
 * Returns 0 when group is NULL.
 */
size_t ui_segment_group_get_selected_index(const ui_segment_group *group);

/*
 * Read the current selected segment label.
 *
 * Returns empty string when group is NULL or not fully initialized.
 */
const char *ui_segment_group_get_selected_label(const ui_segment_group *group);

/*
 * Set the selected segment index.
 *
 * Parameters:
 * - group: destination segmented control
 * - selected_index: new segment index (must be in range)
 * - notify: when true and selection changes, invoke on_change callback
 *
 * Returns:
 * - true if index is valid (selection may or may not change)
 * - false for invalid group or out-of-range index
 */
bool ui_segment_group_set_selected_index(ui_segment_group *group, size_t selected_index,
                                         bool notify);

#endif
