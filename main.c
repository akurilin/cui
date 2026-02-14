#include <SDL3/SDL.h>

#include "pages/todo_page.h"
#include "ui/ui_context.h"

#include <stdbool.h>

static const int WINDOW_WIDTH = 1024;
static const int WINDOW_HEIGHT = 768;

/*
 * Application entry point.
 *
 * `main.c` is intentionally narrow in scope:
 * - own SDL startup/shutdown
 * - own top-level frame orchestration
 * - delegate page-specific behavior to page modules (currently `todo_page`)
 */
int main(void)
{
    // Initialize SDL video before creating any window or renderer objects.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Create the main high-density-aware application window.
    SDL_Window *window =
        SDL_CreateWindow("CUI - a minimalist UI framework in C and SDL3", WINDOW_WIDTH,
                         WINDOW_HEIGHT, SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Create the renderer used by all UI elements during the render phase.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Keep presentation consistent across displays with a fixed logical viewport.
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    const SDL_Color color_bg = {241, 241, 238, 255};

    // Root UI dispatcher/owner for all registered elements.
    ui_context context;
    if (!ui_context_init(&context))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ui_context_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Build and register the active page.
    todo_page *page = todo_page_create(window, &context, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (page == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create todo page");
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
        // Compute frame delta once and pass it to the update phase.
        const Uint64 current_ns = SDL_GetTicksNS();
        const float delta_seconds = (float)(current_ns - previous_ns) / (float)SDL_NS_PER_SECOND;
        previous_ns = current_ns;

        // Phase 1: collect and dispatch SDL events.
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

        // Phase 2: page-specific per-frame logic (outside widget vtables).
        if (!todo_page_update(page))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to update todo page");
            running = false;
        }

        // Phase 3: widget updates via ui_context.
        ui_context_update(&context, delta_seconds);

        // Phase 4: draw frame.
        SDL_SetRenderDrawColor(renderer, color_bg.r, color_bg.g, color_bg.b, color_bg.a);
        SDL_RenderClear(renderer);
        ui_context_render(&context, renderer);
        SDL_RenderPresent(renderer);
    }

    // Teardown order: page -> context -> renderer/window -> SDL runtime.
    todo_page_destroy(page);
    ui_context_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
