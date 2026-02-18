#include "pages/todo_page.h"

#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_fps_counter.h"
#include "ui/ui_hrule.h"
#include "ui/ui_layout_container.h"
#include "ui/ui_pane.h"
#include "ui/ui_scroll_view.h"
#include "ui/ui_segment_group.h"
#include "ui/ui_text.h"
#include "ui/ui_text_input.h"
#include "ui/ui_window.h"
#include "util/fail_fast.h"
#include "util/string_util.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct todo_task
{
    uint64_t id;
    char *title;
    char due_time[6];
    bool is_done;
} todo_task;

typedef struct task_row_context
{
    struct todo_page *page;
    size_t task_index;
} task_row_context;

struct todo_page
{
    ui_runtime *context;
    // Tracks only elements added to ui_runtime by this page so teardown can
    // remove exactly this page's registrations without touching others.
    ui_element *registered_elements[20];
    size_t registered_count;

    todo_task *tasks;
    size_t task_count;
    size_t task_capacity;
    uint64_t next_task_id;
    task_row_context *row_contexts;
    size_t row_context_capacity;

    // Current viewport dimensions for responsive layout.
    int viewport_width;
    int viewport_height;

    // Element references for relayout on resize.
    ui_pane *header_left;
    ui_pane *header_right;
    ui_pane *icon_cell;
    ui_text *title_text;
    ui_text *icon_arrow;
    ui_button *add_button;
    ui_segment_group *filter_group;
    ui_hrule *top_rule;
    ui_hrule *bottom_rule;
    ui_pane *list_frame;
    ui_scroll_view *scroll_view;
    ui_layout_container *rows_container;
    ui_button *clear_done;
    ui_fps_counter *fps_counter;
    ui_window *window_root;
    ui_text *stats_text;
    ui_text *remaining_text;
    ui_text_input *task_input;
    ui_text *datetime_text;

    size_t selected_filter_index;
    time_t last_header_time;

    SDL_Color color_ink;
    SDL_Color color_muted;
    SDL_Color color_button_down;
};

static const float LAYOUT_MARGIN = 36.0F;
static const float HEADER_HEIGHT = 64.0F;
static const float INPUT_ROW_Y = 140.0F;
static const float INPUT_ROW_HEIGHT = 64.0F;
static const float STATS_ROW_Y = 244.0F;
static const float LIST_TOP_Y = 306.0F;
static const float ROW_HEIGHT = 32.0F;
static const float SCROLL_STEP = 24.0F;
// Matches legacy footer geometry at 1024x768 (list height 304, footer y 650).
static const float FOOTER_RESERVE = 158.0F;
static const float FOOTER_GAP = 22.0F;

static const float COL_NUMBER_W = 56.0F;
static const float COL_CHECK_W = 32.0F;
static const float COL_TITLE_W = 620.0F;
static const float COL_TIME_W = 72.0F;
static const float COL_DELETE_W = 96.0F;
static const float COL_DELETE_H = 24.0F;
// Preserved from legacy 1024px-wide row geometry.
static const float COL_TIME_RIGHT_OFFSET = 124.0F;
static const float COL_DELETE_RIGHT_OFFSET = 20.0F;

static const float HEADER_RIGHT_W = 272.0F;
static const float ICON_CELL_W = 56.0F;
static const float ADD_BUTTON_W = 116.0F;
static const float INPUT_FIELD_W = 780.0F;
static const float CLEAR_BUTTON_W = 184.0F;
static const float CLEAR_BUTTON_H = 48.0F;
static const float FILTER_W = 272.0F;
static const float FILTER_H = 40.0F;
static const char *TODO_FILTER_LABELS[] = {"ALL", "ACTIVE", "DONE"};

/*
 * Register one page-owned element into the UI context and track it for teardown.
 */
static void register_element(todo_page *page, ui_element *element)
{
    if (page == NULL || page->context == NULL || element == NULL)
    {
        fail_fast("todo_page: invalid register_element input");
    }

    if (!ui_runtime_add(page->context, element))
    {
        fail_fast("todo_page: ui_runtime_add failed during registration");
    }

    if (page->registered_count >= SDL_arraysize(page->registered_elements))
    {
        fail_fast("todo_page: registered element tracker capacity exceeded");
    }

    page->registered_elements[page->registered_count++] = element;
}

/*
 * Remove and destroy all elements this page registered in reverse add order.
 */
static void unregister_elements(todo_page *page)
{
    if (page == NULL || page->context == NULL)
    {
        fail_fast("todo_page: invalid state in unregister_elements");
    }

    for (size_t i = page->registered_count; i > 0U; --i)
    {
        ui_element *element = page->registered_elements[i - 1U];
        // Remove in reverse add order to mirror stack-like construction.
        (void)ui_runtime_remove(page->context, element, true);
    }
    page->registered_count = 0U;
}

/*
 * Add a child to a layout container.
 */
static void add_child_or_fail(ui_layout_container *container, ui_element *child)
{
    if (container == NULL || child == NULL)
    {
        fail_fast("todo_page: invalid child add input");
    }

    if (!ui_layout_container_add_child(container, child))
    {
        fail_fast("todo_page: ui_layout_container_add_child failed");
    }
}

/*
 * Recompute and refresh the "active/done/remaining" summary labels.
 */
static void update_task_summary(todo_page *page)
{
    if (page == NULL || page->stats_text == NULL || page->remaining_text == NULL)
    {
        fail_fast("todo_page: summary update called with invalid state");
    }

    size_t done_count = 0U;
    for (size_t i = 0; i < page->task_count; ++i)
    {
        if (page->tasks[i].is_done)
        {
            done_count++;
        }
    }

    const size_t active_count = page->task_count - done_count;
    char stats_buffer[64];
    char remaining_buffer[32];

    SDL_snprintf(stats_buffer, sizeof(stats_buffer), "%zu ACTIVE - %zu DONE", active_count,
                 done_count);
    SDL_snprintf(remaining_buffer, sizeof(remaining_buffer), "%zu REMAINING", active_count);

    if (!ui_text_set_content(page->stats_text, stats_buffer))
    {
        fail_fast("todo_page: failed to update stats summary text");
    }
    if (!ui_text_set_content(page->remaining_text, remaining_buffer))
    {
        fail_fast("todo_page: failed to update remaining summary text");
    }
}

static void rebuild_task_rows(todo_page *page);

/*
 * Compute the usable content width from viewport width and horizontal margins.
 */
static float compute_content_width(const todo_page *page)
{
    return (float)page->viewport_width - 2.0F * LAYOUT_MARGIN;
}

/*
 * Return whether a task should be visible under the currently selected filter.
 */
static bool does_task_match_filter(const todo_page *page, const todo_task *task)
{
    if (page == NULL || task == NULL)
    {
        fail_fast("todo_page: invalid state in does_task_match_filter");
    }

    switch (page->selected_filter_index)
    {
    case 1U:
        return !task->is_done;
    case 2U:
        return task->is_done;
    case 0U:
    default:
        return true;
    }
}

/*
 * Ensure per-row callback context storage can index every task entry.
 */
static void ensure_row_context_capacity(todo_page *page)
{
    if (page == NULL)
    {
        fail_fast("todo_page: NULL page in ensure_row_context_capacity");
    }

    if (page->task_count == 0U)
    {
        free(page->row_contexts);
        page->row_contexts = NULL;
        page->row_context_capacity = 0U;
        return;
    }

    if (page->row_context_capacity >= page->task_count)
    {
        return;
    }

    // Context slots are indexed by task index, so capacity follows task_count.
    const size_t new_capacity = page->task_count;
    task_row_context *new_contexts =
        realloc((void *)page->row_contexts, new_capacity * sizeof(task_row_context));
    if (new_contexts == NULL)
    {
        fail_fast("todo_page: failed to grow row contexts");
    }

    page->row_contexts = new_contexts;
    page->row_context_capacity = new_capacity;
}

/*
 * Delete a task by index and rebuild visible rows afterward.
 */
static void delete_task_at_index(todo_page *page, size_t index)
{
    if (page == NULL || index >= page->task_count)
    {
        fail_fast("todo_page: invalid delete index");
    }

    free(page->tasks[index].title);
    page->tasks[index].title = NULL;

    for (size_t i = index; i + 1U < page->task_count; ++i)
    {
        page->tasks[i] = page->tasks[i + 1U];
    }
    page->task_count--;

    rebuild_task_rows(page);
}

/*
 * Button callback that deletes the row's bound task.
 */
static void handle_delete_button_click(void *context)
{
    task_row_context *row_ctx = (task_row_context *)context;
    if (row_ctx == NULL || row_ctx->page == NULL)
    {
        fail_fast("todo_page: delete callback context is invalid");
    }

    delete_task_at_index(row_ctx->page, row_ctx->task_index);
}

/*
 * Checkbox callback that toggles task completion and refreshes row projection.
 */
static void handle_task_checkbox_change(bool checked, void *context)
{
    task_row_context *row_ctx = (task_row_context *)context;
    if (row_ctx == NULL || row_ctx->page == NULL)
    {
        fail_fast("todo_page: checkbox callback context is invalid");
    }

    todo_page *page = row_ctx->page;
    if (row_ctx->task_index >= page->task_count)
    {
        fail_fast("todo_page: checkbox callback received out-of-range task index");
    }

    page->tasks[row_ctx->task_index].is_done = checked;
    rebuild_task_rows(page);
}

/*
 * Build one horizontal UI row for the task at the provided model index.
 */
static void add_task_row(todo_page *page, size_t index)
{
    if (page == NULL || page->rows_container == NULL || index >= page->task_count)
    {
        fail_fast("todo_page: invalid add_task_row input");
    }

    const todo_task *task = &page->tasks[index];
    char row_number[20];
    SDL_snprintf(row_number, sizeof(row_number), "%llu", (unsigned long long)task->id);

    ui_layout_container *row = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 0.0F, ROW_HEIGHT}, UI_LAYOUT_AXIS_HORIZONTAL, &page->color_ink);
    if (row == NULL)
    {
        fail_fast("todo_page: failed to create task row container");
    }

    task_row_context *row_ctx = &page->row_contexts[index];
    row_ctx->page = page;
    row_ctx->task_index = index;

    ui_text *number = ui_text_create(0.0F, 0.0F, row_number, page->color_muted, NULL);
    if (number == NULL)
    {
        fail_fast("todo_page: failed to create task number text");
    }
    number->base.rect.w = COL_NUMBER_W;
    add_child_or_fail(row, (ui_element *)number);

    ui_checkbox *check =
        ui_checkbox_create(0.0F, 0.0F, "", page->color_ink, page->color_ink, page->color_ink,
                           task->is_done, handle_task_checkbox_change, row_ctx, NULL);
    if (check == NULL)
    {
        fail_fast("todo_page: failed to create task checkbox");
    }
    check->base.rect.w = COL_CHECK_W;
    add_child_or_fail(row, (ui_element *)check);

    ui_text *task_text = ui_text_create(0.0F, 0.0F, task->title, page->color_ink, NULL);
    if (task_text == NULL)
    {
        fail_fast("todo_page: failed to create task title text");
    }
    task_text->base.rect.w = COL_TITLE_W;
    add_child_or_fail(row, (ui_element *)task_text);

    ui_text *time_text = ui_text_create(0.0F, 0.0F, task->due_time, page->color_muted, NULL);
    if (time_text == NULL)
    {
        fail_fast("todo_page: failed to create task due-time text");
    }
    time_text->base.rect.w = COL_TIME_W;
    time_text->base.align_h = UI_ALIGN_RIGHT;
    time_text->base.rect.x = COL_TIME_RIGHT_OFFSET;
    add_child_or_fail(row, (ui_element *)time_text);

    ui_button *remove = ui_button_create(&(SDL_FRect){0.0F, 0.0F, COL_DELETE_W, COL_DELETE_H},
                                         page->color_ink, page->color_button_down, "DELETE",
                                         &page->color_ink, handle_delete_button_click, row_ctx);
    if (remove == NULL)
    {
        fail_fast("todo_page: failed to create delete button");
    }
    remove->base.align_h = UI_ALIGN_RIGHT;
    remove->base.rect.x = COL_DELETE_RIGHT_OFFSET;
    add_child_or_fail(row, (ui_element *)remove);
    add_child_or_fail(page->rows_container, (ui_element *)row);
}

/*
 * Rebuild the entire task-row container from current model/filter state.
 */
static void rebuild_task_rows(todo_page *page)
{
    if (page == NULL || page->rows_container == NULL)
    {
        fail_fast("todo_page: invalid state in rebuild_task_rows");
    }
    ensure_row_context_capacity(page);

    // Rows are a projection of model state; rebuild from scratch after changes.
    ui_layout_container_clear_children(page->rows_container, true);

    for (size_t i = 0; i < page->task_count; ++i)
    {
        if (!does_task_match_filter(page, &page->tasks[i]))
        {
            continue;
        }

        add_task_row(page, i);
    }

    update_task_summary(page);
}

/*
 * Append one task model entry, growing storage as needed.
 */
static void append_task(todo_page *page, const char *title, const char *due_time, bool is_done)
{
    if (page == NULL || title == NULL || due_time == NULL)
    {
        fail_fast("todo_page: invalid append_task input");
    }

    if (page->task_count == page->task_capacity)
    {
        const size_t new_capacity = page->task_capacity == 0U ? 8U : page->task_capacity * 2U;
        todo_task *new_tasks = realloc((void *)page->tasks, new_capacity * sizeof(todo_task));
        if (new_tasks == NULL)
        {
            fail_fast("todo_page: failed to grow task storage");
        }
        page->tasks = new_tasks;
        page->task_capacity = new_capacity;
    }

    char *task_title = duplicate_string(title);
    if (task_title == NULL)
    {
        fail_fast("todo_page: failed to duplicate task title");
    }

    if (page->next_task_id == UINT64_MAX)
    {
        free(task_title);
        fail_fast("todo_page: task id counter overflow");
    }

    todo_task *task = &page->tasks[page->task_count];
    task->id = page->next_task_id;
    page->next_task_id++;
    task->title = task_title;
    SDL_snprintf(task->due_time, sizeof(task->due_time), "%s", due_time);
    task->is_done = is_done;
    page->task_count++;
}

/*
 * Format the header clock string as uppercased local date/time text.
 */
static void format_header_datetime(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    const time_t now = time(NULL);
    struct tm local_tm;
#if defined(_WIN32)
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif

    if (strftime(buffer, buffer_size, "%a, %b %d, %Y %H:%M:%S", &local_tm) == 0U)
    {
        buffer[0] = '\0';
        return;
    }

    for (size_t i = 0; buffer[i] != '\0'; ++i)
    {
        buffer[i] = (char)toupper((unsigned char)buffer[i]);
    }
}

/*
 * Fill buffer with local current time in HH:MM format.
 */
static void fill_current_time(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size < 6U)
    {
        return;
    }

    const time_t now = time(NULL);
    struct tm local_tm;
#if defined(_WIN32)
    localtime_s(&local_tm, &now);
#else
    localtime_r(&now, &local_tm);
#endif

    if (strftime(buffer, buffer_size, "%H:%M", &local_tm) == 0U)
    {
        buffer[0] = '\0';
    }
}

/*
 * Generate a simple pseudo-random 32-bit value from an internal xorshift state.
 */
static uint32_t next_pseudo_random_u32(void)
{
    static uint64_t state = 0U;

    if (state == 0U)
    {
        // Lazy one-time seed using wall clock + SDL monotonic ticks.
        state = ((uint64_t)time(NULL) << 32U) ^ SDL_GetTicksNS();
        if (state == 0U)
        {
            state = 0x9e3779b97f4a7c15ULL;
        }
    }

    state ^= state << 13U;
    state ^= state >> 7U;
    state ^= state << 17U;
    return (uint32_t)(state >> 32U);
}

/*
 * Fill buffer with a pseudo-random HH:MM value.
 */
static void fill_random_time(char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size < 6U)
    {
        return;
    }

    const int hour = (int)(next_pseudo_random_u32() % 24U);
    const int minute = (int)(next_pseudo_random_u32() % 60U);
    SDL_snprintf(buffer, buffer_size, "%02d:%02d", hour, minute);
}

/*
 * Create a new task from the text input value and refresh rows.
 */
static void add_task_from_input(todo_page *page)
{
    if (page == NULL || page->task_input == NULL)
    {
        fail_fast("todo_page: invalid state in add_task_from_input");
    }

    const char *input_value = ui_text_input_get_value(page->task_input);
    if (input_value == NULL || input_value[0] == '\0')
    {
        return;
    }

    char due_time[6] = "00:00";
    fill_current_time(due_time, sizeof(due_time));

    append_task(page, input_value, due_time, false);

    ui_text_input_clear(page->task_input);
    rebuild_task_rows(page);
}

/*
 * Remove every completed task from model storage and refresh rows.
 */
static void clear_done_tasks(todo_page *page)
{
    if (page == NULL)
    {
        fail_fast("todo_page: invalid state in clear_done_tasks");
    }

    size_t write = 0U;
    for (size_t read = 0; read < page->task_count; ++read)
    {
        if (page->tasks[read].is_done)
        {
            free(page->tasks[read].title);
            page->tasks[read].title = NULL;
        }
        else
        {
            if (write != read)
            {
                page->tasks[write] = page->tasks[read];
            }
            write++;
        }
    }
    page->task_count = write;

    (void)rebuild_task_rows(page);
}

/*
 * Release all heap allocations owned by task model storage.
 */
static void destroy_task_storage(todo_page *page)
{
    if (page == NULL)
    {
        fail_fast("todo_page: invalid state in destroy_task_storage");
    }

    for (size_t i = 0; i < page->task_count; ++i)
    {
        free(page->tasks[i].title);
        page->tasks[i].title = NULL;
    }

    free(page->tasks);
    page->tasks = NULL;
    page->task_count = 0U;
    page->task_capacity = 0U;

    free(page->row_contexts);
    page->row_contexts = NULL;
    page->row_context_capacity = 0U;
}

/*
 * Button callback that clears all completed tasks.
 */
static void handle_clear_button_click(void *context) { clear_done_tasks((todo_page *)context); }

/*
 * Button callback that attempts to add a task from input.
 */
static void handle_add_button_click(void *context)
{
    todo_page *page = (todo_page *)context;
    if (page == NULL)
    {
        fail_fast("todo_page: add callback context is invalid");
    }

    add_task_from_input(page);
}

/*
 * Text-input submit callback that reuses add-button behavior.
 */
static void handle_task_input_submit(const char *value, void *context)
{
    (void)value;
    handle_add_button_click(context);
}

/*
 * Segment-group callback that updates the active filter and refreshes rows.
 */
static void handle_filter_change(size_t selected_index, const char *selected_label, void *context)
{
    (void)selected_label;

    todo_page *page = (todo_page *)context;
    if (page == NULL)
    {
        fail_fast("todo_page: filter callback context is invalid");
    }

    page->selected_filter_index = selected_index;
    rebuild_task_rows(page);
}

/*
 * Recompute element rectangles for the current viewport dimensions.
 */
static void relayout_page(todo_page *page)
{
    const float content_width = compute_content_width(page);
    const float header_left_w = content_width - HEADER_RIGHT_W;
    const float header_right_x = LAYOUT_MARGIN + header_left_w;
    const float add_button_x = LAYOUT_MARGIN + content_width - ADD_BUTTON_W;
    const float filter_x = LAYOUT_MARGIN + content_width - FILTER_W;

    const float task_list_height = (float)page->viewport_height - LIST_TOP_Y - FOOTER_RESERVE;
    const float footer_rule_y = LIST_TOP_Y + task_list_height + FOOTER_GAP / 2.0F;
    const float footer_y = footer_rule_y + FOOTER_GAP;

    // Header: left pane stretches, right pane shifts rightward (fixed width).
    page->header_left->base.rect.w = header_left_w;
    page->header_right->base.rect.x = header_right_x;
    page->datetime_text->base.rect.x = header_right_x + 24.0F;

    // Input row: input stays fixed width, add button anchors to right edge.
    page->add_button->base.rect.x = add_button_x;

    // Filter group anchors to right edge.
    page->filter_group->base.rect.x = filter_x;

    // Horizontal rules stretch with content width.
    page->top_rule->base.rect.w = content_width;
    page->bottom_rule->base.rect.w = content_width;
    page->bottom_rule->base.rect.y = footer_rule_y;

    // Task list containers stretch; row internals are laid out by containers.
    page->list_frame->base.rect.w = content_width;
    page->list_frame->base.rect.h = task_list_height;
    page->scroll_view->base.rect.w = content_width;
    page->scroll_view->base.rect.h = task_list_height;
    page->rows_container->base.rect.w = content_width;

    // Footer: buttons stay fixed, remaining text anchors right, both track Y.
    page->clear_done->base.rect.y = footer_y;
    page->remaining_text->base.rect.x = LAYOUT_MARGIN + content_width - 168.0F;
    page->remaining_text->base.rect.y = footer_y + 18.0F;

    // FPS counter anchoring.
    page->window_root->base.rect.w = (float)page->viewport_width;
    page->window_root->base.rect.h = (float)page->viewport_height;
    page->fps_counter->viewport_width = page->viewport_width;
    page->fps_counter->viewport_height = page->viewport_height;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool todo_page_resize(todo_page *page, int viewport_width, int viewport_height)
{
    if (page == NULL)
    {
        fail_fast("todo_page_resize called with NULL page");
    }

    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;
    relayout_page(page);
    return true;
}

todo_page *todo_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                            int viewport_height)
{
    if (window == NULL || context == NULL)
    {
        fail_fast("todo_page_create called with invalid arguments");
    }

    todo_page *page = calloc(1U, sizeof(todo_page));
    if (page == NULL)
    {
        fail_fast("todo_page: failed to allocate page object");
    }

    page->context = context;
    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;
    page->next_task_id = 1U;
    page->selected_filter_index = 0U;
    page->last_header_time = 0;

    const SDL_Color color_panel = {245, 245, 242, 255};
    page->color_ink = (SDL_Color){36, 36, 36, 255};
    page->color_muted = (SDL_Color){158, 158, 158, 255};
    const SDL_Color color_button_dark = {33, 33, 37, 255};
    page->color_button_down = (SDL_Color){86, 86, 94, 255};
    const SDL_Color color_accent = {211, 92, 52, 255};

    // Create with stable geometry first; viewport-dependent geometry is applied
    // once via relayout_page() after all required elements exist.
    const float input_field_x = LAYOUT_MARGIN + ICON_CELL_W;

    page->header_left =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, LAYOUT_MARGIN, 1.0F, HEADER_HEIGHT}, color_panel,
                       &page->color_ink);
    if (page->header_left == NULL)
    {
        fail_fast("todo_page: failed to create header_left pane");
    }
    page->header_right =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, LAYOUT_MARGIN, HEADER_RIGHT_W, HEADER_HEIGHT},
                       color_panel, &page->color_ink);
    if (page->header_right == NULL)
    {
        fail_fast("todo_page: failed to create header_right pane");
    }

    page->title_text = ui_text_create(LAYOUT_MARGIN + 22.0F, LAYOUT_MARGIN + 28.0F,
                                      "TODO TASK MANAGEMENT SYSTEM V0.1", page->color_ink, NULL);
    if (page->title_text == NULL)
    {
        fail_fast("todo_page: failed to create title text");
    }

    char header_datetime[40];
    format_header_datetime(header_datetime, sizeof(header_datetime));
    page->datetime_text = ui_text_create(LAYOUT_MARGIN + 24.0F, LAYOUT_MARGIN + 28.0F,
                                         header_datetime, page->color_muted, NULL);
    if (page->datetime_text == NULL)
    {
        fail_fast("todo_page: failed to create datetime text");
    }

    page->icon_cell =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, INPUT_ROW_Y, ICON_CELL_W, INPUT_ROW_HEIGHT},
                       color_panel, &page->color_ink);
    if (page->icon_cell == NULL)
    {
        fail_fast("todo_page: failed to create icon cell pane");
    }
    page->icon_arrow =
        ui_text_create(LAYOUT_MARGIN + 22.0F, INPUT_ROW_Y + 26.0F, ">", color_accent, NULL);
    if (page->icon_arrow == NULL)
    {
        fail_fast("todo_page: failed to create icon arrow text");
    }

    page->task_input = ui_text_input_create(
        &(SDL_FRect){input_field_x, INPUT_ROW_Y, INPUT_FIELD_W, INPUT_ROW_HEIGHT},
        page->color_muted, color_panel, page->color_ink, page->color_ink, "enter task...",
        page->color_muted, window, handle_task_input_submit, page);
    if (page->task_input == NULL)
    {
        fail_fast("todo_page: failed to create task input");
    }

    page->add_button = ui_button_create(
        &(SDL_FRect){LAYOUT_MARGIN, INPUT_ROW_Y, ADD_BUTTON_W, INPUT_ROW_HEIGHT}, color_button_dark,
        page->color_button_down, "ADD", &page->color_ink, handle_add_button_click, page);
    if (page->add_button == NULL)
    {
        fail_fast("todo_page: failed to create add button");
    }

    page->stats_text =
        ui_text_create(LAYOUT_MARGIN, STATS_ROW_Y, "0 ACTIVE - 0 DONE", page->color_ink, NULL);
    if (page->stats_text == NULL)
    {
        fail_fast("todo_page: failed to create stats text");
    }

    page->filter_group = ui_segment_group_create(
        &(SDL_FRect){LAYOUT_MARGIN, STATS_ROW_Y, FILTER_W, FILTER_H}, TODO_FILTER_LABELS,
        SDL_arraysize(TODO_FILTER_LABELS), 0U, color_panel, color_button_dark,
        page->color_button_down, page->color_muted, color_panel, &page->color_ink,
        handle_filter_change, page);
    if (page->filter_group == NULL)
    {
        fail_fast("todo_page: failed to create filter group");
    }

    page->top_rule = ui_hrule_create(1.0F, page->color_ink, 0.0F);
    if (page->top_rule == NULL)
    {
        fail_fast("todo_page: failed to create top rule");
    }
    else
    {
        page->top_rule->base.rect = (SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y - 6.0F, 1.0F, 1.0F};
    }

    page->list_frame = ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, 1.0F, 1.0F},
                                      color_panel, &page->color_ink);
    if (page->list_frame == NULL)
    {
        fail_fast("todo_page: failed to create list frame");
    }

    page->rows_container = ui_layout_container_create(
        &(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, 1.0F, 1.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    if (page->rows_container == NULL)
    {
        fail_fast("todo_page: failed to create rows container");
    }

    // Scroll view takes ownership of rows_container on success.
    page->scroll_view =
        ui_scroll_view_create(&(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, 1.0F, 1.0F},
                              (ui_element *)page->rows_container, SCROLL_STEP, NULL);
    if (page->scroll_view == NULL)
    {
        fail_fast("todo_page: failed to create scroll view");
    }

    page->bottom_rule = ui_hrule_create(1.0F, page->color_ink, 0.0F);
    if (page->bottom_rule == NULL)
    {
        fail_fast("todo_page: failed to create bottom rule");
    }
    else
    {
        page->bottom_rule->base.rect = (SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, 1.0F, 1.0F};
    }

    page->clear_done = ui_button_create(
        &(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, CLEAR_BUTTON_W, CLEAR_BUTTON_H}, color_button_dark,
        page->color_button_down, "CLEAR DONE", &page->color_ink, handle_clear_button_click, page);
    if (page->clear_done == NULL)
    {
        fail_fast("todo_page: failed to create clear-done button");
    }

    page->remaining_text =
        ui_text_create(LAYOUT_MARGIN, LIST_TOP_Y, "0 REMAINING", page->color_muted, NULL);
    if (page->remaining_text == NULL)
    {
        fail_fast("todo_page: failed to create remaining text");
    }

    page->fps_counter =
        ui_fps_counter_create(viewport_width, viewport_height, 12.0F, page->color_ink, NULL);
    if (page->fps_counter == NULL)
    {
        fail_fast("todo_page: failed to create fps counter");
    }
    page->window_root =
        ui_window_create(&(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height});
    if (page->window_root == NULL)
    {
        fail_fast("todo_page: failed to create window root");
    }
    page->fps_counter->base.parent = &page->window_root->base;
    page->fps_counter->base.align_h = UI_ALIGN_RIGHT;
    page->fps_counter->base.align_v = UI_ALIGN_BOTTOM;

    // Single source of truth for viewport-dependent geometry at startup.
    relayout_page(page);

    register_element(page, (ui_element *)page->header_left);
    register_element(page, (ui_element *)page->header_right);
    register_element(page, (ui_element *)page->window_root);
    register_element(page, (ui_element *)page->title_text);
    register_element(page, (ui_element *)page->datetime_text);
    register_element(page, (ui_element *)page->icon_cell);
    register_element(page, (ui_element *)page->icon_arrow);
    register_element(page, (ui_element *)page->task_input);
    register_element(page, (ui_element *)page->add_button);
    register_element(page, (ui_element *)page->stats_text);
    register_element(page, (ui_element *)page->filter_group);
    register_element(page, (ui_element *)page->top_rule);
    register_element(page, (ui_element *)page->list_frame);
    register_element(page, (ui_element *)page->scroll_view);
    register_element(page, (ui_element *)page->bottom_rule);
    register_element(page, (ui_element *)page->clear_done);
    register_element(page, (ui_element *)page->remaining_text);
    register_element(page, (ui_element *)page->fps_counter);

    static const char *initial_task_titles[] = {
        "red", "orange", "yellow", "green", "blue", "indigo", "violet", "cyan", "magenta", "amber",
    };

    char initial_due_time[6];
    for (size_t i = 0U; i < SDL_arraysize(initial_task_titles); ++i)
    {
        fill_random_time(initial_due_time, sizeof(initial_due_time));
        append_task(page, initial_task_titles[i], initial_due_time, false);
    }

    rebuild_task_rows(page);

    return page;
}

bool todo_page_update(todo_page *page)
{
    if (page == NULL)
    {
        fail_fast("todo_page_update called with NULL page");
    }

    const time_t now = time(NULL);
    if (now == page->last_header_time)
    {
        // Avoid reformatting header time every frame when second did not change.
        return true;
    }

    page->last_header_time = now;
    char header_datetime[40];
    format_header_datetime(header_datetime, sizeof(header_datetime));
    if (!ui_text_set_content(page->datetime_text, header_datetime))
    {
        fail_fast("todo_page: failed to update header datetime text");
    }
    return true;
}

static void *create_todo_page_instance(SDL_Window *window, ui_runtime *context, int viewport_width,
                                       int viewport_height)
{
    return (void *)todo_page_create(window, context, viewport_width, viewport_height);
}

static bool resize_todo_page_instance(void *page_instance, int viewport_width, int viewport_height)
{
    return todo_page_resize((todo_page *)page_instance, viewport_width, viewport_height);
}

static bool update_todo_page_instance(void *page_instance)
{
    return todo_page_update((todo_page *)page_instance);
}

static void destroy_todo_page_instance(void *page_instance)
{
    todo_page_destroy((todo_page *)page_instance);
}

const app_page_ops todo_page_ops = {
    .create = create_todo_page_instance,
    .resize = resize_todo_page_instance,
    .update = update_todo_page_instance,
    .destroy = destroy_todo_page_instance,
};

void todo_page_destroy(todo_page *page)
{
    if (page == NULL)
    {
        fail_fast("todo_page_destroy called with NULL page");
    }

    destroy_task_storage(page);
    unregister_elements(page);
    free(page);
}
