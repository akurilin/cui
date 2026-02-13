// Pull in SDL's core API (windowing, rendering, events, etc.).
#include <SDL3/SDL.h>

#include "ui/ui_button.h"
#include "ui/ui_context.h"
#include "ui/ui_fps_counter.h"
#include "ui/ui_pane.h"
#include "ui/ui_text.h"

#include <stdbool.h>
#include <stdio.h>

// Window dimensions in pixels.
static const int WINDOW_WIDTH = 1024;
static const int WINDOW_HEIGHT = 768;

// Colors used by the retro-style UI.
static const Uint8 COLOR_BG = 24;
static const Uint8 COLOR_PANE = 52;
static const Uint8 COLOR_BUTTON_UP = 170;
static const Uint8 COLOR_BUTTON_DOWN = 90;
static const Uint8 COLOR_BLACK = 0;
static const Uint8 COLOR_TEXT = 235;
static const Uint8 COLOR_ALPHA_OPAQUE = 255;

static void log_button_press(void *context)
{
    (void)context;
    printf("button pressed\n");
    fflush(stdout);
}

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

// Program entry point. `void` means this program expects no command-line args.
int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const char *window_title = "SDL Button Demo";
    const int window_flags = 0;
    SDL_Window *window = SDL_CreateWindow(window_title, WINDOW_WIDTH, WINDOW_HEIGHT, window_flags);
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

    if (!SDL_ShowCursor())
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShowCursor failed: %s", SDL_GetError());
    }

    ui_context context;
    if (!ui_context_init(&context))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ui_context_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const float pane_width = (float)WINDOW_WIDTH / 3.0F;
    const float content_width = (float)WINDOW_WIDTH - pane_width;

    const SDL_FRect pane_rect = {0.0F, 0.0F, pane_width, (float)WINDOW_HEIGHT};
    const SDL_Color pane_fill_color =
        (SDL_Color){COLOR_PANE, COLOR_PANE, COLOR_PANE, COLOR_ALPHA_OPAQUE};
    const SDL_Color pane_border_color =
        (SDL_Color){COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_ALPHA_OPAQUE};
    ui_pane *pane = create_pane(&pane_rect, pane_fill_color, pane_border_color);

    const float sidebar_text_x = 24.0F;
    const float sidebar_text_start_y = 40.0F;
    const float sidebar_text_spacing = 28.0F;
    const SDL_Color sidebar_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    const char *item_1_label = "item 1";
    const char *item_2_label = "item 2";
    const char *item_3_label = "item 3";
    ui_text *item_1 =
        create_text(sidebar_text_x, sidebar_text_start_y, item_1_label, sidebar_text_color);
    ui_text *item_2 = create_text(sidebar_text_x, sidebar_text_start_y + sidebar_text_spacing,
                                  item_2_label, sidebar_text_color);
    ui_text *item_3 =
        create_text(sidebar_text_x, sidebar_text_start_y + sidebar_text_spacing * 2.0F,
                    item_3_label, sidebar_text_color);

    const SDL_FRect button_rect = {
        pane_width + (content_width - 140.0F) / 2.0F,
        ((float)WINDOW_HEIGHT - 80.0F) / 2.0F,
        140.0F,
        80.0F,
    };
    const SDL_Color button_up_color =
        (SDL_Color){COLOR_BUTTON_UP, COLOR_BUTTON_UP, COLOR_BUTTON_UP, COLOR_ALPHA_OPAQUE};
    const SDL_Color button_down_color =
        (SDL_Color){COLOR_BUTTON_DOWN, COLOR_BUTTON_DOWN, COLOR_BUTTON_DOWN, COLOR_ALPHA_OPAQUE};
    const SDL_Color button_border_color =
        (SDL_Color){COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_ALPHA_OPAQUE};
    button_click_handler button_click_handler_fn = log_button_press;
    void *button_click_context = NULL;
    ui_button *button =
        create_button(&button_rect, button_up_color, button_down_color, button_border_color,
                      button_click_handler_fn, button_click_context);

    const int fps_viewport_width = WINDOW_WIDTH;
    const int fps_viewport_height = WINDOW_HEIGHT;
    const float fps_padding = 12.0F;
    const SDL_Color fps_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    ui_fps_counter *fps_counter =
        create_fps_counter(fps_viewport_width, fps_viewport_height, fps_padding, fps_text_color);

    if (!add_element_or_fail(&context, (ui_element *)pane) ||
        !add_element_or_fail(&context, (ui_element *)item_1) ||
        !add_element_or_fail(&context, (ui_element *)item_2) ||
        !add_element_or_fail(&context, (ui_element *)item_3) ||
        !add_element_or_fail(&context, (ui_element *)button) ||
        !add_element_or_fail(&context, (ui_element *)fps_counter))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create or register UI elements");
        ui_context_destroy(&context);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    Uint64 previous_counter = SDL_GetPerformanceCounter();

    bool running = true;
    while (running)
    {
        const Uint64 current_counter = SDL_GetPerformanceCounter();
        const float delta_seconds =
            (float)(current_counter - previous_counter) / (float)performance_frequency;
        previous_counter = current_counter;

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

        SDL_SetRenderDrawColor(renderer, COLOR_BG, COLOR_BG, COLOR_BG, COLOR_ALPHA_OPAQUE);
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
