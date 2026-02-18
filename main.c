#include <SDL3/SDL.h>

#include "pages/todo_page.h"
#include "system/ui_runtime.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const int DEFAULT_WINDOW_WIDTH = 1024;
static const int DEFAULT_WINDOW_HEIGHT = 768;
static const int MIN_WINDOW_WIDTH = 640;
static const int MIN_WINDOW_HEIGHT = 480;

typedef struct window_size
{
    int width;
    int height;
} window_size;

static bool parse_positive_int(const char *value, int *out)
{
    if (value == NULL || out == NULL || value[0] == '\0')
    {
        return false;
    }

    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0')
    {
        return false;
    }
    if (parsed < 1L || parsed > INT_MAX)
    {
        return false;
    }

    *out = (int)parsed;
    return true;
}

static void log_usage(const char *program_name)
{
    SDL_Log("Usage: %s [-w|--width <width>] [-h|--height <height>] [--help]", program_name);
}

static void log_help(const char *program_name)
{
    log_usage(program_name);
    SDL_Log("Options:");
    SDL_Log("  -w, --width <width>    Set startup window width in pixels.");
    SDL_Log("  -h, --height <height>  Set startup window height in pixels.");
    SDL_Log("      --help             Show this help message.");
}

typedef enum parse_result
{
    PARSE_RESULT_OK = 0,
    PARSE_RESULT_HELP,
    PARSE_RESULT_ERROR
} parse_result;

static parse_result parse_window_options(int argc, char **argv, window_size *size)
{
    if (size == NULL)
    {
        return PARSE_RESULT_ERROR;
    }

    for (int i = 1; i < argc; i++)
    {
        const char *option = argv[i];
        int *target = NULL;
        if (strcmp(option, "-w") == 0 || strcmp(option, "--width") == 0)
        {
            target = &size->width;
        }
        else if (strcmp(option, "-h") == 0 || strcmp(option, "--height") == 0)
        {
            target = &size->height;
        }
        else if (strcmp(option, "--help") == 0)
        {
            log_help(argv[0]);
            return PARSE_RESULT_HELP;
        }
        else
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown option: %s", option);
            log_usage(argv[0]);
            return PARSE_RESULT_ERROR;
        }

        if (i + 1 >= argc)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Missing value for option: %s", option);
            log_usage(argv[0]);
            return PARSE_RESULT_ERROR;
        }

        const char *value = argv[++i];
        if (!parse_positive_int(value, target))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid value for option %s: %s", option,
                         value);
            log_usage(argv[0]);
            return PARSE_RESULT_ERROR;
        }
    }

    return PARSE_RESULT_OK;
}

/*
 * Application entry point.
 *
 * `main.c` is intentionally narrow in scope:
 * - own SDL startup/shutdown
 * - own top-level frame orchestration
 * - delegate page-specific behavior to page modules (currently `todo_page`)
 */
int main(int argc, char **argv)
{
    window_size startup_size = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT};

    const parse_result parse_args_result = parse_window_options(argc, argv, &startup_size);
    if (parse_args_result == PARSE_RESULT_HELP)
    {
        return 0;
    }
    if (parse_args_result == PARSE_RESULT_ERROR)
    {
        return 1;
    }

    // Initialize SDL video before creating any window or renderer objects.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Create the main high-density-aware, resizable application window.
    SDL_Window *window =
        SDL_CreateWindow("CUI - a minimalist UI framework in C and SDL3", startup_size.width,
                         startup_size.height, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetWindowMinimumSize(window, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

    // Create the renderer used by all UI elements during the render phase.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);

    // Map the render coordinate space to the logical window size so layout
    // code works in points (not physical pixels) on high-DPI displays.
    SDL_SetRenderLogicalPresentation(renderer, startup_size.width, startup_size.height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    const SDL_Color color_bg = {241, 241, 238, 255};

    // Root UI dispatcher/owner for all registered elements.
    ui_runtime context;
    if (!ui_runtime_init(&context))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "ui_runtime_init failed");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Build and register the active page.
    todo_page *page = todo_page_create(window, &context, startup_size.width, startup_size.height);
    if (page == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create todo page");
        ui_runtime_destroy(&context);
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
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                const int new_w = event.window.data1;
                const int new_h = event.window.data2;
                SDL_SetRenderLogicalPresentation(renderer, new_w, new_h,
                                                 SDL_LOGICAL_PRESENTATION_LETTERBOX);
                todo_page_resize(page, new_w, new_h);
            }
            ui_runtime_handle_event(&context, &event);
        }

        // Phase 2: page-specific per-frame logic (outside widget vtables).
        if (!todo_page_update(page))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to update todo page");
            running = false;
        }

        // Phase 3: widget updates via ui_runtime.
        ui_runtime_update(&context, delta_seconds);

        // Phase 4: draw frame.
        SDL_SetRenderDrawColor(renderer, color_bg.r, color_bg.g, color_bg.b, color_bg.a);
        SDL_RenderClear(renderer);
        ui_runtime_render(&context, renderer);
        SDL_RenderPresent(renderer);
    }

    // Teardown order: page -> context -> renderer/window -> SDL runtime.
    todo_page_destroy(page);
    ui_runtime_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
