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
    ui_context *context;
    // Tracks only elements added to ui_context by this page so teardown can
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
static const float FOOTER_RESERVE = 80.0F;
static const float FOOTER_GAP = 22.0F;

static const float COL_NUMBER_W = 56.0F;
static const float COL_CHECK_W = 32.0F;
static const float COL_TITLE_W = 620.0F;
static const float COL_TIME_W = 72.0F;
static const float COL_DELETE_W = 96.0F;
static const float COL_DELETE_H = 24.0F;

static const float HEADER_RIGHT_W = 272.0F;
static const float ICON_CELL_W = 56.0F;
static const float ADD_BUTTON_W = 116.0F;
static const float INPUT_FIELD_W = 780.0F;
static const float CLEAR_BUTTON_W = 184.0F;
static const float CLEAR_BUTTON_H = 48.0F;
static const float FILTER_W = 272.0F;
static const float FILTER_H = 40.0F;
static const char *TODO_FILTER_LABELS[] = {"ALL", "ACTIVE", "DONE"};

static void destroy_element(ui_element *element)
{
    if (element != NULL && element->ops != NULL && element->ops->destroy != NULL)
    {
        element->ops->destroy(element);
    }
}

static bool register_element(todo_page *page, ui_element *element)
{
    if (page == NULL || page->context == NULL || element == NULL)
    {
        return false;
    }

    if (!ui_context_add(page->context, element))
    {
        // If registration fails, ownership never transfers to ui_context.
        destroy_element(element);
        return false;
    }

    if (page->registered_count >= SDL_arraysize(page->registered_elements))
    {
        (void)ui_context_remove(page->context, element, true);
        return false;
    }

    page->registered_elements[page->registered_count++] = element;
    return true;
}

static void unregister_elements(todo_page *page)
{
    if (page == NULL || page->context == NULL)
    {
        return;
    }

    for (size_t i = page->registered_count; i > 0U; --i)
    {
        ui_element *element = page->registered_elements[i - 1U];
        // Remove in reverse add order to mirror stack-like construction.
        (void)ui_context_remove(page->context, element, true);
    }
    page->registered_count = 0U;
}

static bool add_child_or_fail(ui_layout_container *container, ui_element *child)
{
    if (!ui_layout_container_add_child(container, child))
    {
        destroy_element(child);
        return false;
    }
    return true;
}

static bool update_task_summary(todo_page *page)
{
    if (page == NULL || page->stats_text == NULL || page->remaining_text == NULL)
    {
        return false;
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
        return false;
    }
    if (!ui_text_set_content(page->remaining_text, remaining_buffer))
    {
        return false;
    }

    return true;
}

static bool rebuild_task_rows(todo_page *page);

static float compute_content_width(const todo_page *page)
{
    return (float)page->viewport_width - 2.0F * LAYOUT_MARGIN;
}

static bool does_task_match_filter(const todo_page *page, const todo_task *task)
{
    if (page == NULL || task == NULL)
    {
        return false;
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

static bool ensure_row_context_capacity(todo_page *page)
{
    if (page == NULL)
    {
        return false;
    }

    if (page->task_count == 0U)
    {
        free(page->row_contexts);
        page->row_contexts = NULL;
        page->row_context_capacity = 0U;
        return true;
    }

    if (page->row_context_capacity >= page->task_count)
    {
        return true;
    }

    // Context slots are indexed by task index, so capacity follows task_count.
    const size_t new_capacity = page->task_count;
    task_row_context *new_contexts =
        realloc((void *)page->row_contexts, new_capacity * sizeof(task_row_context));
    if (new_contexts == NULL)
    {
        return false;
    }

    page->row_contexts = new_contexts;
    page->row_context_capacity = new_capacity;
    return true;
}

static bool delete_task_at_index(todo_page *page, size_t index)
{
    if (page == NULL || index >= page->task_count)
    {
        return false;
    }

    free(page->tasks[index].title);
    page->tasks[index].title = NULL;

    for (size_t i = index; i + 1U < page->task_count; ++i)
    {
        page->tasks[i] = page->tasks[i + 1U];
    }
    page->task_count--;

    return rebuild_task_rows(page);
}

static void handle_delete_button_click(void *context)
{
    task_row_context *row_ctx = (task_row_context *)context;
    if (row_ctx == NULL || row_ctx->page == NULL)
    {
        return;
    }

    if (!delete_task_at_index(row_ctx->page, row_ctx->task_index))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to delete task at index %zu",
                     row_ctx->task_index);
    }
}

static void handle_task_checkbox_change(bool checked, void *context)
{
    task_row_context *row_ctx = (task_row_context *)context;
    if (row_ctx == NULL || row_ctx->page == NULL)
    {
        return;
    }

    todo_page *page = row_ctx->page;
    if (row_ctx->task_index >= page->task_count)
    {
        return;
    }

    page->tasks[row_ctx->task_index].is_done = checked;
    if (!rebuild_task_rows(page))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to rebuild rows after toggle");
    }
}

static bool add_task_row(todo_page *page, size_t index)
{
    if (page == NULL || page->rows_container == NULL || index >= page->task_count)
    {
        return false;
    }

    const todo_task *task = &page->tasks[index];
    char row_number[20];
    SDL_snprintf(row_number, sizeof(row_number), "%llu", (unsigned long long)task->id);

    ui_layout_container *row = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 0.0F, ROW_HEIGHT}, UI_LAYOUT_AXIS_HORIZONTAL, &page->color_ink);
    if (row == NULL)
    {
        return false;
    }

    task_row_context *row_ctx = &page->row_contexts[index];
    row_ctx->page = page;
    row_ctx->task_index = index;

    ui_text *number = ui_text_create(0.0F, 0.0F, row_number, page->color_muted, NULL);
    if (number == NULL)
    {
        destroy_element((ui_element *)row);
        return false;
    }
    number->base.rect.w = COL_NUMBER_W;
    if (!add_child_or_fail(row, (ui_element *)number))
    {
        destroy_element((ui_element *)row);
        return false;
    }

    ui_checkbox *check =
        ui_checkbox_create(0.0F, 0.0F, "", page->color_ink, page->color_ink, page->color_ink,
                           task->is_done, handle_task_checkbox_change, row_ctx, NULL);
    if (check == NULL)
    {
        destroy_element((ui_element *)row);
        return false;
    }
    check->base.rect.w = COL_CHECK_W;
    if (!add_child_or_fail(row, (ui_element *)check))
    {
        destroy_element((ui_element *)row);
        return false;
    }

    ui_text *task_text = ui_text_create(0.0F, 0.0F, task->title, page->color_ink, NULL);
    if (task_text == NULL)
    {
        destroy_element((ui_element *)row);
        return false;
    }
    task_text->base.rect.w = COL_TITLE_W;
    if (!add_child_or_fail(row, (ui_element *)task_text))
    {
        destroy_element((ui_element *)row);
        return false;
    }

    ui_text *time_text = ui_text_create(0.0F, 0.0F, task->due_time, page->color_muted, NULL);
    if (time_text == NULL)
    {
        destroy_element((ui_element *)row);
        return false;
    }
    time_text->base.rect.w = COL_TIME_W;
    if (!add_child_or_fail(row, (ui_element *)time_text))
    {
        destroy_element((ui_element *)row);
        return false;
    }

    ui_button *remove = ui_button_create(&(SDL_FRect){0.0F, 0.0F, COL_DELETE_W, COL_DELETE_H},
                                         page->color_ink, page->color_button_down, "DELETE",
                                         &page->color_ink, handle_delete_button_click, row_ctx);
    if (remove == NULL)
    {
        destroy_element((ui_element *)row);
        return false;
    }
    if (!add_child_or_fail(row, (ui_element *)remove))
    {
        destroy_element((ui_element *)row);
        return false;
    }

    if (!add_child_or_fail(page->rows_container, (ui_element *)row))
    {
        return false;
    }

    return true;
}

static bool rebuild_task_rows(todo_page *page)
{
    if (page == NULL || page->rows_container == NULL)
    {
        return false;
    }
    if (!ensure_row_context_capacity(page))
    {
        return false;
    }

    // Rows are a projection of model state; rebuild from scratch after changes.
    ui_layout_container_clear_children(page->rows_container, true);

    for (size_t i = 0; i < page->task_count; ++i)
    {
        if (!does_task_match_filter(page, &page->tasks[i]))
        {
            continue;
        }

        if (!add_task_row(page, i))
        {
            return false;
        }
    }

    return update_task_summary(page);
}

static bool append_task(todo_page *page, const char *title, const char *due_time, bool is_done)
{
    if (page == NULL || title == NULL || due_time == NULL)
    {
        return false;
    }

    if (page->task_count == page->task_capacity)
    {
        const size_t new_capacity = page->task_capacity == 0U ? 8U : page->task_capacity * 2U;
        todo_task *new_tasks = realloc((void *)page->tasks, new_capacity * sizeof(todo_task));
        if (new_tasks == NULL)
        {
            return false;
        }
        page->tasks = new_tasks;
        page->task_capacity = new_capacity;
    }

    char *task_title = duplicate_string(title);
    if (task_title == NULL)
    {
        return false;
    }

    if (page->next_task_id == UINT64_MAX)
    {
        free(task_title);
        return false;
    }

    todo_task *task = &page->tasks[page->task_count];
    task->id = page->next_task_id;
    page->next_task_id++;
    task->title = task_title;
    SDL_snprintf(task->due_time, sizeof(task->due_time), "%s", due_time);
    task->is_done = is_done;
    page->task_count++;
    return true;
}

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

static bool add_task_from_input(todo_page *page)
{
    if (page == NULL || page->task_input == NULL)
    {
        return false;
    }

    const char *input_value = ui_text_input_get_value(page->task_input);
    if (input_value == NULL || input_value[0] == '\0')
    {
        return true;
    }

    char due_time[6] = "00:00";
    fill_current_time(due_time, sizeof(due_time));

    if (!append_task(page, input_value, due_time, false))
    {
        return false;
    }

    ui_text_input_clear(page->task_input);
    return rebuild_task_rows(page);
}

static void clear_done_tasks(todo_page *page)
{
    if (page == NULL)
    {
        return;
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

static void destroy_task_storage(todo_page *page)
{
    if (page == NULL)
    {
        return;
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

static void handle_clear_button_click(void *context) { clear_done_tasks((todo_page *)context); }

static void handle_add_button_click(void *context)
{
    todo_page *page = (todo_page *)context;
    if (page == NULL)
    {
        return;
    }

    if (!add_task_from_input(page))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to add task from input");
    }
}

static void handle_task_input_submit(const char *value, void *context)
{
    (void)value;
    handle_add_button_click(context);
}

static void handle_filter_change(size_t selected_index, const char *selected_label, void *context)
{
    (void)selected_label;

    todo_page *page = (todo_page *)context;
    if (page == NULL)
    {
        return;
    }

    page->selected_filter_index = selected_index;
    if (!rebuild_task_rows(page))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to rebuild rows after filter change");
    }
}

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

    // Task list containers stretch; leaf elements inside stay fixed.
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
    page->fps_counter->viewport_width = page->viewport_width;
    page->fps_counter->viewport_height = page->viewport_height;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool todo_page_resize(todo_page *page, int viewport_width, int viewport_height)
{
    if (page == NULL)
    {
        return false;
    }

    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;
    relayout_page(page);
    return true;
}

todo_page *todo_page_create(SDL_Window *window, ui_context *context, int viewport_width,
                            int viewport_height)
{
    if (window == NULL || context == NULL)
    {
        return NULL;
    }

    todo_page *page = calloc(1U, sizeof(todo_page));
    if (page == NULL)
    {
        return NULL;
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

    // Container dimensions derive from viewport; leaf elements use fixed sizes.
    const float content_width = compute_content_width(page);
    const float header_left_w = content_width - HEADER_RIGHT_W;
    const float header_right_x = LAYOUT_MARGIN + header_left_w;
    const float input_field_x = LAYOUT_MARGIN + ICON_CELL_W;
    const float add_button_x = LAYOUT_MARGIN + content_width - ADD_BUTTON_W;
    const float filter_x = LAYOUT_MARGIN + content_width - FILTER_W;
    const float task_list_height = (float)viewport_height - LIST_TOP_Y - FOOTER_RESERVE;
    const float footer_rule_y = LIST_TOP_Y + task_list_height + FOOTER_GAP / 2.0F;
    const float footer_y = footer_rule_y + FOOTER_GAP;

    page->header_left =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, LAYOUT_MARGIN, header_left_w, HEADER_HEIGHT},
                       color_panel, &page->color_ink);
    page->header_right =
        ui_pane_create(&(SDL_FRect){header_right_x, LAYOUT_MARGIN, HEADER_RIGHT_W, HEADER_HEIGHT},
                       color_panel, &page->color_ink);

    page->title_text = ui_text_create(LAYOUT_MARGIN + 22.0F, LAYOUT_MARGIN + 28.0F,
                                      "TODO TASK MANAGEMENT SYSTEM V0.1", page->color_ink, NULL);

    char header_datetime[40];
    format_header_datetime(header_datetime, sizeof(header_datetime));
    page->datetime_text = ui_text_create(header_right_x + 24.0F, LAYOUT_MARGIN + 28.0F,
                                         header_datetime, page->color_muted, NULL);

    page->icon_cell =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, INPUT_ROW_Y, ICON_CELL_W, INPUT_ROW_HEIGHT},
                       color_panel, &page->color_ink);
    page->icon_arrow =
        ui_text_create(LAYOUT_MARGIN + 22.0F, INPUT_ROW_Y + 26.0F, ">", color_accent, NULL);

    page->task_input = ui_text_input_create(
        &(SDL_FRect){input_field_x, INPUT_ROW_Y, INPUT_FIELD_W, INPUT_ROW_HEIGHT},
        page->color_muted, color_panel, page->color_ink, page->color_ink, "enter task...",
        page->color_muted, window, handle_task_input_submit, page);

    page->add_button = ui_button_create(
        &(SDL_FRect){add_button_x, INPUT_ROW_Y, ADD_BUTTON_W, INPUT_ROW_HEIGHT}, color_button_dark,
        page->color_button_down, "ADD", &page->color_ink, handle_add_button_click, page);

    page->stats_text =
        ui_text_create(LAYOUT_MARGIN, STATS_ROW_Y, "0 ACTIVE - 0 DONE", page->color_ink, NULL);

    page->filter_group = ui_segment_group_create(
        &(SDL_FRect){filter_x, STATS_ROW_Y, FILTER_W, FILTER_H}, TODO_FILTER_LABELS,
        SDL_arraysize(TODO_FILTER_LABELS), 0U, color_panel, color_button_dark,
        page->color_button_down, page->color_muted, color_panel, &page->color_ink,
        handle_filter_change, page);

    page->top_rule = ui_hrule_create(1.0F, page->color_ink, 0.0F);
    if (page->top_rule != NULL)
    {
        page->top_rule->base.rect =
            (SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y - 6.0F, content_width, 1.0F};
    }

    page->list_frame =
        ui_pane_create(&(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, content_width, task_list_height},
                       color_panel, &page->color_ink);

    page->rows_container = ui_layout_container_create(
        &(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, content_width, task_list_height},
        UI_LAYOUT_AXIS_VERTICAL, NULL);

    if (page->rows_container != NULL)
    {
        // Scroll view takes ownership of rows_container on success.
        page->scroll_view = ui_scroll_view_create(
            &(SDL_FRect){LAYOUT_MARGIN, LIST_TOP_Y, content_width, task_list_height},
            (ui_element *)page->rows_container, SCROLL_STEP, NULL);
    }

    page->bottom_rule = ui_hrule_create(1.0F, page->color_ink, 0.0F);
    if (page->bottom_rule != NULL)
    {
        page->bottom_rule->base.rect =
            (SDL_FRect){LAYOUT_MARGIN, footer_rule_y, content_width, 1.0F};
    }

    page->clear_done = ui_button_create(
        &(SDL_FRect){LAYOUT_MARGIN, footer_y, CLEAR_BUTTON_W, CLEAR_BUTTON_H}, color_button_dark,
        page->color_button_down, "CLEAR DONE", &page->color_ink, handle_clear_button_click, page);

    page->remaining_text = ui_text_create(LAYOUT_MARGIN + content_width - 168.0F, footer_y + 18.0F,
                                          "0 REMAINING", page->color_muted, NULL);

    page->fps_counter =
        ui_fps_counter_create(viewport_width, viewport_height, 12.0F, page->color_ink, NULL);

    if (page->header_left == NULL || page->header_right == NULL || page->title_text == NULL ||
        page->datetime_text == NULL || page->icon_cell == NULL || page->icon_arrow == NULL ||
        page->task_input == NULL || page->add_button == NULL || page->stats_text == NULL ||
        page->filter_group == NULL || page->top_rule == NULL || page->list_frame == NULL ||
        page->rows_container == NULL || page->scroll_view == NULL || page->bottom_rule == NULL ||
        page->clear_done == NULL || page->remaining_text == NULL || page->fps_counter == NULL)
    {
        destroy_element((ui_element *)page->scroll_view);
        if (page->scroll_view == NULL)
        {
            destroy_element((ui_element *)page->rows_container);
        }
        destroy_element((ui_element *)page->header_left);
        destroy_element((ui_element *)page->header_right);
        destroy_element((ui_element *)page->title_text);
        destroy_element((ui_element *)page->datetime_text);
        destroy_element((ui_element *)page->icon_cell);
        destroy_element((ui_element *)page->icon_arrow);
        destroy_element((ui_element *)page->task_input);
        destroy_element((ui_element *)page->add_button);
        destroy_element((ui_element *)page->stats_text);
        destroy_element((ui_element *)page->filter_group);
        destroy_element((ui_element *)page->top_rule);
        destroy_element((ui_element *)page->list_frame);
        destroy_element((ui_element *)page->bottom_rule);
        destroy_element((ui_element *)page->clear_done);
        destroy_element((ui_element *)page->remaining_text);
        destroy_element((ui_element *)page->fps_counter);
        page->rows_container = NULL;
        page->scroll_view = NULL;
        page->task_input = NULL;
        page->stats_text = NULL;
        page->remaining_text = NULL;
        page->datetime_text = NULL;
        free(page);
        return NULL;
    }

    if (!register_element(page, (ui_element *)page->header_left) ||
        !register_element(page, (ui_element *)page->header_right) ||
        !register_element(page, (ui_element *)page->title_text) ||
        !register_element(page, (ui_element *)page->datetime_text) ||
        !register_element(page, (ui_element *)page->icon_cell) ||
        !register_element(page, (ui_element *)page->icon_arrow) ||
        !register_element(page, (ui_element *)page->task_input) ||
        !register_element(page, (ui_element *)page->add_button) ||
        !register_element(page, (ui_element *)page->stats_text) ||
        !register_element(page, (ui_element *)page->filter_group) ||
        !register_element(page, (ui_element *)page->top_rule) ||
        !register_element(page, (ui_element *)page->list_frame) ||
        !register_element(page, (ui_element *)page->scroll_view) ||
        !register_element(page, (ui_element *)page->bottom_rule) ||
        !register_element(page, (ui_element *)page->clear_done) ||
        !register_element(page, (ui_element *)page->remaining_text) ||
        !register_element(page, (ui_element *)page->fps_counter))
    {
        destroy_task_storage(page);
        unregister_elements(page);
        free(page);
        return NULL;
    }

    static const char *initial_task_titles[] = {
        "red", "orange", "yellow", "green", "blue", "indigo", "violet", "cyan", "magenta", "amber",
    };

    char initial_due_time[6];
    for (size_t i = 0U; i < SDL_arraysize(initial_task_titles); ++i)
    {
        fill_random_time(initial_due_time, sizeof(initial_due_time));
        if (!append_task(page, initial_task_titles[i], initial_due_time, false))
        {
            destroy_task_storage(page);
            unregister_elements(page);
            free(page);
            return NULL;
        }
    }

    if (!rebuild_task_rows(page))
    {
        destroy_task_storage(page);
        unregister_elements(page);
        free(page);
        return NULL;
    }

    return page;
}

bool todo_page_update(todo_page *page)
{
    if (page == NULL)
    {
        return false;
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
    return ui_text_set_content(page->datetime_text, header_datetime);
}

void todo_page_destroy(todo_page *page)
{
    if (page == NULL)
    {
        return;
    }

    destroy_task_storage(page);
    unregister_elements(page);
    free(page);
}
