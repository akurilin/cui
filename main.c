// Pull in SDL's core API (windowing, rendering, events, timing, etc.).
#include <SDL3/SDL.h>

#include "ui/ui_button.h"
#include "ui/ui_context.h"
#include "ui/ui_fps_counter.h"
#include "ui/ui_pane.h"
#include "ui/ui_text.h"

#include <stdbool.h>
#include <stdio.h>

// Window dimensions in pixels. Keeping these centralized avoids scattered
// "magic numbers" and makes it obvious where to change app resolution.
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

// Demo click callback used by the button element.
// The framework exposes a generic context pointer for caller-owned data;
// this sample does not need extra data yet, so the pointer is ignored.
static void log_button_press(void *context)
{
    (void)context;
    printf("button pressed\n");
    fflush(stdout);
}

// Registers a UI element with the root UI context.
//
// Why this helper exists:
// - `ui_context_add` can fail if capacity/allocation constraints are hit.
// - Elements are already heap-allocated before registration.
// - On registration failure we must destroy the element immediately to avoid
//   leaking memory.
//
// The helper centralizes that ownership rule so call sites stay concise and
// do not accidentally forget cleanup.
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
    // SDL uses explicit subsystem initialization. We initialize only `VIDEO`
    // because this app currently needs windowing/rendering only.
    //
    // SDL3 returns `true` on success here (note: many C APIs use 0/success),
    // so we negate the call for failure handling.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Create a native OS window. Doing this before renderer creation matters:
    // the renderer needs a target window/surface to present into.
    const char *window_title = "SDL Button Demo";
    const int window_flags = 0;
    SDL_Window *window = SDL_CreateWindow(window_title, WINDOW_WIDTH, WINDOW_HEIGHT, window_flags);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // SDL may place windows according to platform defaults; we explicitly center
    // for predictable startup behavior across systems.
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Create a renderer bound to the window. The renderer is the drawing backend
    // abstraction (GPU/driver selected by SDL).
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // This demo expects a visible cursor for button interaction.
    // Failure is non-fatal, so we warn and continue.
    if (!SDL_ShowCursor())
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShowCursor failed: %s", SDL_GetError());
    }

    // `ui_context` is the root owner/dispatcher for UI elements:
    // - owns lifecycle of registered elements
    // - forwards input events
    // - updates and renders each frame
    ui_context context;
    if (!ui_context_init(&context))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ui_context_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Layout: left sidebar pane takes one-third of the window width; main content
    // area takes the remainder.
    const float pane_width = (float)WINDOW_WIDTH / 3.0F;
    const float content_width = (float)WINDOW_WIDTH - pane_width;

    const SDL_FRect pane_rect = {0.0F, 0.0F, pane_width, (float)WINDOW_HEIGHT};
    const SDL_Color pane_fill_color =
        (SDL_Color){COLOR_PANE, COLOR_PANE, COLOR_PANE, COLOR_ALPHA_OPAQUE};
    const SDL_Color pane_border_color =
        (SDL_Color){COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_ALPHA_OPAQUE};

    // Sidebar background panel.
    ui_pane *pane = ui_pane_create(&pane_rect, pane_fill_color, pane_border_color);

    // Sidebar labels. Using shared spacing constants keeps vertical rhythm
    // consistent and makes future layout adjustments straightforward.
    const float sidebar_text_x = 24.0F;
    const float sidebar_text_start_y = 40.0F;
    const float sidebar_text_spacing = 28.0F;
    const SDL_Color sidebar_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    const char *item_1_label = "item 1";
    const char *item_2_label = "item 2";
    const char *item_3_label = "item 3";
    ui_text *item_1 =
        ui_text_create(sidebar_text_x, sidebar_text_start_y, item_1_label, sidebar_text_color);
    ui_text *item_2 = ui_text_create(sidebar_text_x, sidebar_text_start_y + sidebar_text_spacing,
                                     item_2_label, sidebar_text_color);
    ui_text *item_3 =
        ui_text_create(sidebar_text_x, sidebar_text_start_y + sidebar_text_spacing * 2.0F,
                       item_3_label, sidebar_text_color);

    // Main button centered within the content area (not the full window).
    // The X position intentionally offsets by `pane_width` so sidebar and content
    // remain visually independent.
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

    // Interactive button that triggers `log_button_press` on click.
    ui_button *button =
        ui_button_create(&button_rect, button_up_color, button_down_color, button_border_color,
                         button_click_handler_fn, button_click_context);

    // FPS counter overlays diagnostic text in the window's lower-right area.
    // Viewport dimensions are supplied so the element can anchor itself
    // correctly regardless of frame timing.
    const int fps_viewport_width = WINDOW_WIDTH;
    const int fps_viewport_height = WINDOW_HEIGHT;
    const float fps_padding = 12.0F;
    const SDL_Color fps_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    ui_fps_counter *fps_counter =
        ui_fps_counter_create(fps_viewport_width, fps_viewport_height, fps_padding, fps_text_color);

    // Register all created elements with the UI context.
    // If any registration fails, previously-added elements remain owned by
    // `context` and get cleaned up by `ui_context_destroy`.
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

    // High-resolution timer setup for frame delta time computation.
    // `SDL_GetPerformanceFrequency` gives counter ticks per second.
    const Uint64 performance_frequency = SDL_GetPerformanceFrequency();
    Uint64 previous_counter = SDL_GetPerformanceCounter();

    // Main frame loop (a standard real-time app/game loop):
    // 1) measure elapsed time since previous frame
    // 2) pump and handle OS/input events
    // 3) update UI state using delta time
    // 4) render the current frame
    // 5) present rendered frame to the screen
    bool running = true;
    while (running)
    {
        // Delta time lets update logic remain frame-rate independent.
        // Faster/slower machines still simulate time consistently.
        const Uint64 current_counter = SDL_GetPerformanceCounter();
        const float delta_seconds =
            (float)(current_counter - previous_counter) / (float)performance_frequency;
        previous_counter = current_counter;

        // Event pump:
        // - Pulls pending events from SDL's internal queue
        // - Handles quit requests
        // - Forwards all other events to UI elements via the context
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

        // Update pass: lets animated/time-dependent widgets advance state.
        ui_context_update(&context, delta_seconds);

        // Render pass begins by clearing the previous frame.
        SDL_SetRenderDrawColor(renderer, COLOR_BG, COLOR_BG, COLOR_BG, COLOR_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // Draw all UI elements in context-managed order.
        ui_context_render(&context, renderer);

        // Swap back buffer to front buffer so the rendered frame becomes visible.
        SDL_RenderPresent(renderer);
    }

    // Reverse-order teardown:
    // - UI context first (releases all element resources)
    // - renderer/window next
    // - SDL global shutdown last
    ui_context_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
