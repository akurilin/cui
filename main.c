#include <SDL3/SDL.h>

#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_context.h"
#include "ui/ui_hrule.h"
#include "ui/ui_layout_container.h"
#include "ui/ui_pane.h"
#include "ui/ui_segment_group.h"
#include "ui/ui_text.h"
#include "ui/ui_text_input.h"

#include <stdbool.h>

static const int WINDOW_WIDTH = 1024;
static const int WINDOW_HEIGHT = 768;

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
    const SDL_Color color_button_down = {18, 18, 22, 255};
    const SDL_Color color_button_light = {238, 238, 234, 255};
    const SDL_Color color_accent = {211, 92, 52, 255};

    ui_pane *header_left =
        ui_pane_create(&(SDL_FRect){36.0F, 36.0F, 680.0F, 96.0F}, color_panel, &color_ink);
    ui_pane *header_right =
        ui_pane_create(&(SDL_FRect){716.0F, 36.0F, 272.0F, 96.0F}, color_panel, &color_ink);

    ui_text *title = ui_text_create(58.0F, 60.0F, "TODO", color_ink, NULL);
    ui_text *subtitle =
        ui_text_create(58.0F, 94.0F, "TASK MANAGEMENT SYSTEM V0.1", color_muted, NULL);
    ui_text *date_text = ui_text_create(740.0F, 60.0F, "SAT, FEB 14, 2026", color_muted, NULL);
    ui_text *clock_text = ui_text_create(852.0F, 94.0F, "12:24:24", color_muted, NULL);

    ui_pane *icon_cell =
        ui_pane_create(&(SDL_FRect){36.0F, 172.0F, 56.0F, 64.0F}, color_panel, &color_ink);
    ui_text *icon_arrow = ui_text_create(58.0F, 198.0F, ">", color_accent, NULL);

    ui_text_input *task_input = ui_text_input_create(
        &(SDL_FRect){92.0F, 172.0F, 780.0F, 64.0F}, color_muted, color_panel, color_ink, color_ink,
        "enter task...", color_muted, window, NULL, NULL);
    ui_button *add_button =
        ui_button_create(&(SDL_FRect){872.0F, 172.0F, 116.0F, 64.0F}, color_button_dark,
                         color_button_down, "ADD", &color_ink, NULL, NULL);

    ui_text *stats_text = ui_text_create(36.0F, 276.0F, "5 ACTIVE - 0 DONE", color_ink, NULL);

    const char *filter_labels[] = {"ALL", "ACTIVE", "DONE"};
    ui_segment_group *filter_group = ui_segment_group_create(
        &(SDL_FRect){716.0F, 276.0F, 272.0F, 40.0F}, filter_labels, 3U, 0U, color_panel,
        color_button_dark, color_button_down, color_muted, color_panel, &color_ink, NULL, NULL);
    ui_hrule *top_rule = ui_hrule_create(1.0F, color_ink, 0.0F);
    if (top_rule != NULL)
    {
        top_rule->base.rect = (SDL_FRect){36.0F, 332.0F, 952.0F, 1.0F};
    }

    ui_pane *list_frame =
        ui_pane_create(&(SDL_FRect){36.0F, 338.0F, 952.0F, 286.0F}, color_panel, &color_ink);

    ui_layout_container *rows_container = ui_layout_container_create(
        &(SDL_FRect){36.0F, 338.0F, 952.0F, 286.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);

    const char *row_numbers[] = {"01", "02", "03", "04", "05"};
    const char *row_tasks[] = {"asdf", "asdf", "asdf", "asdf", "asdf"};
    const char *row_times[] = {"18:30", "19:59", "19:59", "19:59", "19:59"};

    bool rows_ok = rows_container != NULL;
    for (size_t i = 0; rows_ok && i < 5U; ++i)
    {
        ui_layout_container *row = ui_layout_container_create(
            &(SDL_FRect){0.0F, 0.0F, 0.0F, 48.0F}, UI_LAYOUT_AXIS_HORIZONTAL, &color_ink);
        if (row == NULL)
        {
            rows_ok = false;
            break;
        }

        ui_text *number = ui_text_create(0.0F, 0.0F, row_numbers[i], color_muted, NULL);
        ui_checkbox *check = ui_checkbox_create(0.0F, 0.0F, "", color_ink, color_ink, color_ink,
                                                false, NULL, NULL, NULL);
        ui_text *task = ui_text_create(0.0F, 0.0F, row_tasks[i], color_ink, NULL);
        ui_text *time = ui_text_create(0.0F, 0.0F, row_times[i], color_muted, NULL);
        ui_button *remove =
            ui_button_create(&(SDL_FRect){0.0F, 0.0F, 40.0F, 32.0F}, color_button_light,
                             color_button_light, "-", &color_ink, NULL, NULL);

        if (!add_child_or_fail(row, (ui_element *)number) ||
            !add_child_or_fail(row, (ui_element *)check) ||
            !add_child_or_fail(row, (ui_element *)task) ||
            !add_child_or_fail(row, (ui_element *)time) ||
            !add_child_or_fail(row, (ui_element *)remove) ||
            !add_child_or_fail(rows_container, (ui_element *)row))
        {
            if (row->base.ops != NULL && row->base.ops->destroy != NULL)
            {
                row->base.ops->destroy((ui_element *)row);
            }
            rows_ok = false;
        }
    }

    ui_hrule *bottom_rule = ui_hrule_create(1.0F, color_ink, 0.0F);
    if (bottom_rule != NULL)
    {
        bottom_rule->base.rect = (SDL_FRect){36.0F, 664.0F, 952.0F, 1.0F};
    }

    ui_button *clear_done =
        ui_button_create(&(SDL_FRect){36.0F, 682.0F, 184.0F, 48.0F}, color_button_dark,
                         color_button_down, "CLEAR DONE", &color_ink, NULL, NULL);
    ui_text *remaining_text = ui_text_create(820.0F, 700.0F, "5 REMAINING", color_muted, NULL);

    if (header_left == NULL || header_right == NULL || title == NULL || subtitle == NULL ||
        date_text == NULL || clock_text == NULL || icon_cell == NULL || icon_arrow == NULL ||
        task_input == NULL || add_button == NULL || stats_text == NULL || filter_group == NULL ||
        top_rule == NULL || list_frame == NULL || !rows_ok || bottom_rule == NULL ||
        clear_done == NULL || remaining_text == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create todo mockup UI elements");
        if (rows_container != NULL && rows_container->base.ops != NULL &&
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

    if (!add_element_or_fail(&context, (ui_element *)header_left) ||
        !add_element_or_fail(&context, (ui_element *)header_right) ||
        !add_element_or_fail(&context, (ui_element *)title) ||
        !add_element_or_fail(&context, (ui_element *)subtitle) ||
        !add_element_or_fail(&context, (ui_element *)date_text) ||
        !add_element_or_fail(&context, (ui_element *)clock_text) ||
        !add_element_or_fail(&context, (ui_element *)icon_cell) ||
        !add_element_or_fail(&context, (ui_element *)icon_arrow) ||
        !add_element_or_fail(&context, (ui_element *)task_input) ||
        !add_element_or_fail(&context, (ui_element *)add_button) ||
        !add_element_or_fail(&context, (ui_element *)stats_text) ||
        !add_element_or_fail(&context, (ui_element *)filter_group) ||
        !add_element_or_fail(&context, (ui_element *)top_rule) ||
        !add_element_or_fail(&context, (ui_element *)list_frame) ||
        !add_element_or_fail(&context, (ui_element *)rows_container) ||
        !add_element_or_fail(&context, (ui_element *)bottom_rule) ||
        !add_element_or_fail(&context, (ui_element *)clear_done) ||
        !add_element_or_fail(&context, (ui_element *)remaining_text))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to register todo mockup UI elements");
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

        ui_context_update(&context, delta_seconds);

        SDL_SetRenderDrawColor(renderer, color_bg.r, color_bg.g, color_bg.b, color_bg.a);
        SDL_RenderClear(renderer);
        ui_context_render(&context, renderer);
        SDL_RenderPresent(renderer);
    }

    ui_context_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
