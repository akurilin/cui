#include <SDL3/SDL.h>

#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_context.h"
#include "ui/ui_fps_counter.h"
#include "ui/ui_hrule.h"
#include "ui/ui_layout_container.h"
#include "ui/ui_pane.h"
#include "ui/ui_scroll_view.h"
#include "ui/ui_segment_group.h"
#include "ui/ui_text.h"
#include "ui/ui_text_input.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const int WINDOW_WIDTH = 1024;
static const int WINDOW_HEIGHT = 768;
static const float TASK_LIST_HEIGHT = 304.0F;

typedef struct todo_task
{
    uint64_t id;
    char *title;
    char due_time[6];
    bool is_done;
} todo_task;

typedef struct todo_controller todo_controller;

typedef struct delete_button_context
{
    // Row buttons are recreated on every rebuild. Store controller + row index
    // in stable callback context slots so each button can delete the correct task.
    todo_controller *controller;
    size_t task_index;
} delete_button_context;

struct todo_controller
{
    todo_task *tasks;
    size_t task_count;
    size_t task_capacity;
    uint64_t next_task_id;
    delete_button_context *delete_contexts;
    size_t delete_context_capacity;

    ui_layout_container *rows_container;
    ui_text *stats_text;
    ui_text *remaining_text;
    ui_text_input *task_input;
    size_t selected_filter_index;

    SDL_Color color_ink;
    SDL_Color color_muted;
    SDL_Color color_button_down;
};

static bool add_element_or_fail(ui_context *context, ui_element *element)
{
    if (!ui_context_add(context, element))
    {
        if (element != NULL && element->ops != NULL && element->ops->destroy != NULL)
        {
            element->ops->destroy(element);
        }
        return false;
    }
    return true;
}

static bool add_child_or_fail(ui_layout_container *container, ui_element *child)
{
    if (!ui_layout_container_add_child(container, child))
    {
        if (child != NULL && child->ops != NULL && child->ops->destroy != NULL)
        {
            child->ops->destroy(child);
        }
        return false;
    }
    return true;
}

static char *duplicate_string(const char *source)
{
    if (source == NULL)
    {
        return NULL;
    }

    const size_t source_length = strlen(source);
    char *copy = malloc(source_length + 1U);
    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, source, source_length + 1U);
    return copy;
}

static bool update_task_summary(todo_controller *controller)
{
    if (controller == NULL || controller->stats_text == NULL || controller->remaining_text == NULL)
    {
        return false;
    }

    size_t done_count = 0U;
    for (size_t i = 0; i < controller->task_count; ++i)
    {
        if (controller->tasks[i].is_done)
        {
            done_count++;
        }
    }

    const size_t active_count = controller->task_count - done_count;
    char stats_buffer[64];
    char remaining_buffer[32];

    SDL_snprintf(stats_buffer, sizeof(stats_buffer), "%zu ACTIVE - %zu DONE", active_count,
                 done_count);
    SDL_snprintf(remaining_buffer, sizeof(remaining_buffer), "%zu REMAINING", active_count);

    if (!ui_text_set_content(controller->stats_text, stats_buffer))
    {
        return false;
    }
    if (!ui_text_set_content(controller->remaining_text, remaining_buffer))
    {
        return false;
    }

    return true;
}

static bool rebuild_task_rows(todo_controller *controller);

static bool does_task_match_filter(const todo_controller *controller, const todo_task *task)
{
    if (controller == NULL || task == NULL)
    {
        return false;
    }

    switch (controller->selected_filter_index)
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

static bool ensure_delete_context_capacity(todo_controller *controller)
{
    if (controller == NULL)
    {
        return false;
    }

    if (controller->task_count == 0U)
    {
        free(controller->delete_contexts);
        controller->delete_contexts = NULL;
        controller->delete_context_capacity = 0U;
        return true;
    }

    if (controller->delete_context_capacity >= controller->task_count)
    {
        return true;
    }

    // Match context capacity to task_count so each visible row can bind a
    // dedicated callback context carrying its current task index.
    const size_t new_capacity = controller->task_count;
    delete_button_context *new_contexts =
        realloc((void *)controller->delete_contexts, new_capacity * sizeof(delete_button_context));
    if (new_contexts == NULL)
    {
        return false;
    }

    controller->delete_contexts = new_contexts;
    controller->delete_context_capacity = new_capacity;
    return true;
}

static bool delete_task_at_index(todo_controller *controller, size_t index)
{
    if (controller == NULL || index >= controller->task_count)
    {
        return false;
    }

    free(controller->tasks[index].title);
    controller->tasks[index].title = NULL;

    // Keep tasks densely packed so row numbering and rebuild mapping stay simple.
    for (size_t i = index; i + 1U < controller->task_count; ++i)
    {
        controller->tasks[i] = controller->tasks[i + 1U];
    }
    controller->task_count--;

    return rebuild_task_rows(controller);
}

static void handle_delete_button_click(void *context)
{
    delete_button_context *delete_context = (delete_button_context *)context;
    if (delete_context == NULL || delete_context->controller == NULL)
    {
        return;
    }

    if (!delete_task_at_index(delete_context->controller, delete_context->task_index))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to delete task at index %zu",
                     delete_context->task_index);
    }
}

static void handle_task_checkbox_change(bool checked, void *context)
{
    delete_button_context *check_context = (delete_button_context *)context;
    if (check_context == NULL || check_context->controller == NULL)
    {
        return;
    }

    todo_controller *controller = check_context->controller;
    if (check_context->task_index >= controller->task_count)
    {
        return;
    }

    controller->tasks[check_context->task_index].is_done = checked;
    if (!rebuild_task_rows(controller))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to rebuild rows after toggle");
    }
}

static bool add_task_row(todo_controller *controller, size_t index)
{
    if (controller == NULL || controller->rows_container == NULL || index >= controller->task_count)
    {
        return false;
    }

    const todo_task *task = &controller->tasks[index];
    char row_number[20];
    SDL_snprintf(row_number, sizeof(row_number), "%llu", (unsigned long long)task->id);

    ui_layout_container *row = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 0.0F, 32.0F}, UI_LAYOUT_AXIS_HORIZONTAL, &controller->color_ink);
    if (row == NULL)
    {
        return false;
    }

    ui_text *number = ui_text_create(0.0F, 0.0F, row_number, controller->color_muted, NULL);
    if (number != NULL)
    {
        number->base.rect.w = 56.0F;
    }
    if (!add_child_or_fail(row, (ui_element *)number))
    {
        row->base.ops->destroy((ui_element *)row);
        return false;
    }

    delete_button_context *row_context = &controller->delete_contexts[index];
    row_context->controller = controller;
    row_context->task_index = index;

    ui_checkbox *check = ui_checkbox_create(
        0.0F, 0.0F, "", controller->color_ink, controller->color_ink, controller->color_ink,
        task->is_done, handle_task_checkbox_change, row_context, NULL);
    if (check != NULL)
    {
        check->base.rect.w = 32.0F;
    }
    if (!add_child_or_fail(row, (ui_element *)check))
    {
        row->base.ops->destroy((ui_element *)row);
        return false;
    }

    ui_text *task_text = ui_text_create(0.0F, 0.0F, task->title, controller->color_ink, NULL);
    if (task_text != NULL)
    {
        task_text->base.rect.w = 620.0F;
    }
    if (!add_child_or_fail(row, (ui_element *)task_text))
    {
        row->base.ops->destroy((ui_element *)row);
        return false;
    }

    ui_text *time_text = ui_text_create(0.0F, 0.0F, task->due_time, controller->color_muted, NULL);
    if (time_text != NULL)
    {
        time_text->base.rect.w = 72.0F;
    }
    if (!add_child_or_fail(row, (ui_element *)time_text))
    {
        row->base.ops->destroy((ui_element *)row);
        return false;
    }

    ui_button *remove =
        ui_button_create(&(SDL_FRect){0.0F, 0.0F, 96.0F, 24.0F}, controller->color_ink,
                         controller->color_button_down, "DELETE", &controller->color_ink,
                         handle_delete_button_click, row_context);
    if (!add_child_or_fail(row, (ui_element *)remove))
    {
        row->base.ops->destroy((ui_element *)row);
        return false;
    }

    if (!add_child_or_fail(controller->rows_container, (ui_element *)row))
    {
        return false;
    }

    return true;
}

static bool rebuild_task_rows(todo_controller *controller)
{
    if (controller == NULL || controller->rows_container == NULL)
    {
        return false;
    }
    if (!ensure_delete_context_capacity(controller))
    {
        return false;
    }

    // Rows are a pure projection of model state; always rebuild after mutation.
    ui_layout_container_clear_children(controller->rows_container, true);

    for (size_t i = 0; i < controller->task_count; ++i)
    {
        if (!does_task_match_filter(controller, &controller->tasks[i]))
        {
            continue;
        }

        if (!add_task_row(controller, i))
        {
            return false;
        }
    }

    return update_task_summary(controller);
}

static bool append_task(todo_controller *controller, const char *title, const char *due_time,
                        bool is_done)
{
    if (controller == NULL || title == NULL || due_time == NULL)
    {
        return false;
    }

    if (controller->task_count == controller->task_capacity)
    {
        const size_t new_capacity =
            controller->task_capacity == 0U ? 8U : controller->task_capacity * 2U;
        todo_task *new_tasks = realloc((void *)controller->tasks, new_capacity * sizeof(todo_task));
        if (new_tasks == NULL)
        {
            return false;
        }
        controller->tasks = new_tasks;
        controller->task_capacity = new_capacity;
    }

    char *task_title = duplicate_string(title);
    if (task_title == NULL)
    {
        return false;
    }

    todo_task *task = &controller->tasks[controller->task_count];
    if (controller->next_task_id == UINT64_MAX)
    {
        free(task_title);
        return false;
    }

    task->id = controller->next_task_id;
    controller->next_task_id++;
    task->title = task_title;
    SDL_snprintf(task->due_time, sizeof(task->due_time), "%s", due_time);
    task->is_done = is_done;
    controller->task_count++;
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
    strftime(buffer, buffer_size, "%H:%M", &local_tm);
}

static uint32_t next_pseudo_random_u32(void)
{
    static uint64_t state = 0U;

    if (state == 0U)
    {
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

static bool add_task_from_input(todo_controller *controller)
{
    if (controller == NULL || controller->task_input == NULL)
    {
        return false;
    }

    const char *input_value = ui_text_input_get_value(controller->task_input);
    if (input_value == NULL || input_value[0] == '\0')
    {
        return true;
    }

    char due_time[6] = "00:00";
    fill_current_time(due_time, sizeof(due_time));

    if (!append_task(controller, input_value, due_time, false))
    {
        return false;
    }

    ui_text_input_clear(controller->task_input);
    return rebuild_task_rows(controller);
}

static void clear_all_tasks(todo_controller *controller)
{
    if (controller == NULL)
    {
        return;
    }

    for (size_t i = 0; i < controller->task_count; ++i)
    {
        free(controller->tasks[i].title);
        controller->tasks[i].title = NULL;
    }
    controller->task_count = 0U;

    if (controller->rows_container != NULL)
    {
        ui_layout_container_clear_children(controller->rows_container, true);
    }
    free(controller->delete_contexts);
    controller->delete_contexts = NULL;
    controller->delete_context_capacity = 0U;

    (void)update_task_summary(controller);
}

static void destroy_task_storage(todo_controller *controller)
{
    if (controller == NULL)
    {
        return;
    }

    for (size_t i = 0; i < controller->task_count; ++i)
    {
        free(controller->tasks[i].title);
        controller->tasks[i].title = NULL;
    }

    free(controller->tasks);
    controller->tasks = NULL;
    controller->task_count = 0U;
    controller->task_capacity = 0U;
    free(controller->delete_contexts);
    controller->delete_contexts = NULL;
    controller->delete_context_capacity = 0U;
}

static void handle_clear_button_click(void *context)
{
    clear_all_tasks((todo_controller *)context);
}

static void handle_add_button_click(void *context)
{
    todo_controller *controller = (todo_controller *)context;
    if (controller == NULL)
    {
        return;
    }

    if (!add_task_from_input(controller))
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

    todo_controller *controller = (todo_controller *)context;
    if (controller == NULL)
    {
        return;
    }

    controller->selected_filter_index = selected_index;
    if (!rebuild_task_rows(controller))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to rebuild rows after filter change");
    }
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("SDL Todo Mockup", WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    ui_context context;
    if (!ui_context_init(&context))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ui_context_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const SDL_Color color_bg = {241, 241, 238, 255};
    const SDL_Color color_panel = {245, 245, 242, 255};
    const SDL_Color color_ink = {36, 36, 36, 255};
    const SDL_Color color_muted = {158, 158, 158, 255};
    const SDL_Color color_button_dark = {33, 33, 37, 255};
    const SDL_Color color_button_down = {86, 86, 94, 255};
    const SDL_Color color_accent = {211, 92, 52, 255};

    ui_pane *header_left =
        ui_pane_create(&(SDL_FRect){36.0F, 36.0F, 680.0F, 64.0F}, color_panel, &color_ink);
    ui_pane *header_right =
        ui_pane_create(&(SDL_FRect){716.0F, 36.0F, 272.0F, 64.0F}, color_panel, &color_ink);

    ui_text *title =
        ui_text_create(58.0F, 64.0F, "TODO TASK MANAGEMENT SYSTEM V0.1", color_ink, NULL);
    char header_datetime[40];
    format_header_datetime(header_datetime, sizeof(header_datetime));
    ui_text *datetime_text = ui_text_create(740.0F, 64.0F, header_datetime, color_muted, NULL);

    ui_pane *icon_cell =
        ui_pane_create(&(SDL_FRect){36.0F, 140.0F, 56.0F, 64.0F}, color_panel, &color_ink);
    ui_text *icon_arrow = ui_text_create(58.0F, 166.0F, ">", color_accent, NULL);

    ui_text_input *task_input = ui_text_input_create(
        &(SDL_FRect){92.0F, 140.0F, 780.0F, 64.0F}, color_muted, color_panel, color_ink, color_ink,
        "enter task...", color_muted, window, handle_task_input_submit, NULL);
    ui_button *add_button =
        ui_button_create(&(SDL_FRect){872.0F, 140.0F, 116.0F, 64.0F}, color_button_dark,
                         color_button_down, "ADD", &color_ink, handle_add_button_click, NULL);

    ui_text *stats_text = ui_text_create(36.0F, 244.0F, "0 ACTIVE - 0 DONE", color_ink, NULL);

    const char *filter_labels[] = {"ALL", "ACTIVE", "DONE"};
    ui_segment_group *filter_group =
        ui_segment_group_create(&(SDL_FRect){716.0F, 244.0F, 272.0F, 40.0F}, filter_labels, 3U, 0U,
                                color_panel, color_button_dark, color_button_down, color_muted,
                                color_panel, &color_ink, handle_filter_change, NULL);
    ui_hrule *top_rule = ui_hrule_create(1.0F, color_ink, 0.0F);
    if (top_rule != NULL)
    {
        top_rule->base.rect = (SDL_FRect){36.0F, 300.0F, 952.0F, 1.0F};
    }

    ui_pane *list_frame = ui_pane_create(&(SDL_FRect){36.0F, 306.0F, 952.0F, TASK_LIST_HEIGHT},
                                         color_panel, &color_ink);

    ui_layout_container *rows_container = ui_layout_container_create(
        &(SDL_FRect){36.0F, 306.0F, 952.0F, TASK_LIST_HEIGHT}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    ui_scroll_view *rows_scroll_view = NULL;
    if (rows_container != NULL)
    {
        rows_scroll_view =
            ui_scroll_view_create(&(SDL_FRect){36.0F, 306.0F, 952.0F, TASK_LIST_HEIGHT},
                                  (ui_element *)rows_container, 24.0F, NULL);
    }

    ui_hrule *bottom_rule = ui_hrule_create(1.0F, color_ink, 0.0F);
    if (bottom_rule != NULL)
    {
        bottom_rule->base.rect = (SDL_FRect){36.0F, 632.0F, 952.0F, 1.0F};
    }

    ui_text *remaining_text = ui_text_create(820.0F, 668.0F, "0 REMAINING", color_muted, NULL);
    ui_fps_counter *fps_counter =
        ui_fps_counter_create(WINDOW_WIDTH, WINDOW_HEIGHT, 12.0F, color_ink, NULL);

    todo_controller controller = {
        .tasks = NULL,
        .task_count = 0U,
        .task_capacity = 0U,
        .next_task_id = 1U,
        .delete_contexts = NULL,
        .delete_context_capacity = 0U,
        .rows_container = rows_container,
        .stats_text = stats_text,
        .remaining_text = remaining_text,
        .task_input = task_input,
        .selected_filter_index = 0U,
        .color_ink = color_ink,
        .color_muted = color_muted,
        .color_button_down = color_button_down,
    };

    ui_button *clear_done = ui_button_create(&(SDL_FRect){36.0F, 650.0F, 184.0F, 48.0F},
                                             color_button_dark, color_button_down, "CLEAR DONE",
                                             &color_ink, handle_clear_button_click, &controller);

    static const char *initial_task_titles[] = {
        "red", "orange", "yellow", "green", "blue", "indigo", "violet", "cyan", "magenta", "amber",
    };
    char initial_due_time[6];
    bool rows_ok = rows_container != NULL && rows_scroll_view != NULL;
    fill_random_time(initial_due_time, sizeof(initial_due_time));
    rows_ok = rows_ok && append_task(&controller, initial_task_titles[0], initial_due_time, false);
    for (size_t i = 1U; i < SDL_arraysize(initial_task_titles) && rows_ok; ++i)
    {
        fill_random_time(initial_due_time, sizeof(initial_due_time));
        rows_ok = append_task(&controller, initial_task_titles[i], initial_due_time, false);
    }
    rows_ok = rows_ok && rebuild_task_rows(&controller);

    if (header_left == NULL || header_right == NULL || title == NULL || datetime_text == NULL ||
        icon_cell == NULL || icon_arrow == NULL || task_input == NULL || add_button == NULL ||
        stats_text == NULL || filter_group == NULL || top_rule == NULL || list_frame == NULL ||
        !rows_ok || bottom_rule == NULL || clear_done == NULL || remaining_text == NULL ||
        fps_counter == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create todo mockup UI elements");
        destroy_task_storage(&controller);
        if (rows_scroll_view != NULL && rows_scroll_view->base.ops != NULL &&
            rows_scroll_view->base.ops->destroy != NULL)
        {
            rows_scroll_view->base.ops->destroy((ui_element *)rows_scroll_view);
        }
        else if (rows_container != NULL && rows_container->base.ops != NULL &&
                 rows_container->base.ops->destroy != NULL)
        {
            rows_container->base.ops->destroy((ui_element *)rows_container);
        }
        ui_context_destroy(&context);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    task_input->on_submit_context = &controller;
    add_button->on_click_context = &controller;
    filter_group->on_change_context = &controller;

    if (!add_element_or_fail(&context, (ui_element *)header_left) ||
        !add_element_or_fail(&context, (ui_element *)header_right) ||
        !add_element_or_fail(&context, (ui_element *)title) ||
        !add_element_or_fail(&context, (ui_element *)datetime_text) ||
        !add_element_or_fail(&context, (ui_element *)icon_cell) ||
        !add_element_or_fail(&context, (ui_element *)icon_arrow) ||
        !add_element_or_fail(&context, (ui_element *)task_input) ||
        !add_element_or_fail(&context, (ui_element *)add_button) ||
        !add_element_or_fail(&context, (ui_element *)stats_text) ||
        !add_element_or_fail(&context, (ui_element *)filter_group) ||
        !add_element_or_fail(&context, (ui_element *)top_rule) ||
        !add_element_or_fail(&context, (ui_element *)list_frame) ||
        !add_element_or_fail(&context, (ui_element *)rows_scroll_view) ||
        !add_element_or_fail(&context, (ui_element *)bottom_rule) ||
        !add_element_or_fail(&context, (ui_element *)clear_done) ||
        !add_element_or_fail(&context, (ui_element *)remaining_text) ||
        !add_element_or_fail(&context, (ui_element *)fps_counter))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to register todo mockup UI elements");
        destroy_task_storage(&controller);
        ui_context_destroy(&context);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    Uint64 previous_ns = SDL_GetTicksNS();

    while (running)
    {
        const Uint64 current_ns = SDL_GetTicksNS();
        const float delta_seconds = (float)(current_ns - previous_ns) / (float)SDL_NS_PER_SECOND;
        previous_ns = current_ns;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
                continue;
            }
            ui_context_handle_event(&context, &event);
        }

        format_header_datetime(header_datetime, sizeof(header_datetime));
        if (!ui_text_set_content(datetime_text, header_datetime))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to update header datetime text");
            running = false;
        }

        ui_context_update(&context, delta_seconds);

        SDL_SetRenderDrawColor(renderer, color_bg.r, color_bg.g, color_bg.b, color_bg.a);
        SDL_RenderClear(renderer);
        ui_context_render(&context, renderer);
        SDL_RenderPresent(renderer);
    }

    destroy_task_storage(&controller);
    ui_context_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
