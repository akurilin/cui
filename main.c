#include <SDL3/SDL.h>

#include "pages/app_page.h"
#include "system/ui_runtime.h"
#include "util/fail_fast.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const int DEFAULT_WINDOW_WIDTH = 1024;
static const int DEFAULT_WINDOW_HEIGHT = 768;
static const int MIN_WINDOW_WIDTH = 640;
static const int MIN_WINDOW_HEIGHT = 480;
static const char *DEFAULT_PAGE_ID = "todo";

typedef struct window_size
{
    int width;
    int height;
} window_size;

typedef struct startup_options
{
    window_size size;
    const char *page_id;
} startup_options;

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
    SDL_Log("Usage: %s [--page <id>] [-w|--width <width>] [-h|--height <height>] [--help]",
            program_name);
}

static void log_available_pages(void)
{
    if (app_page_count == 0U)
    {
        SDL_Log("Pages: (none)");
        return;
    }

    SDL_Log("Pages:");
    for (size_t i = 0U; i < app_page_count; ++i)
    {
        if (app_pages[i].id == NULL)
        {
            fail_fast("page index contains NULL id at index %zu", i);
        }
        SDL_Log("  %s", app_pages[i].id);
    }
}

static void log_help(const char *program_name)
{
    log_usage(program_name);
    SDL_Log("Options:");
    SDL_Log("      --page <id>        Select page to load (default: %s).", DEFAULT_PAGE_ID);
    SDL_Log("  -w, --width <width>    Set startup window width in pixels.");
    SDL_Log("  -h, --height <height>  Set startup window height in pixels.");
    SDL_Log("      --help             Show this help message.");
    log_available_pages();
}

static const app_page_entry *find_page_descriptor_by_id(const char *page_id)
{
    if (page_id == NULL)
    {
        fail_fast("find_page_descriptor_by_id called with NULL page_id");
    }

    for (size_t i = 0U; i < app_page_count; ++i)
    {
        if (app_pages[i].id == NULL)
        {
            fail_fast("page index contains NULL id at index %zu", i);
        }
        if (strcmp(app_pages[i].id, page_id) == 0)
        {
            return &app_pages[i];
        }
    }

    return NULL;
}

typedef enum parse_result
{
    PARSE_RESULT_OK = 0,
    PARSE_RESULT_HELP,
    PARSE_RESULT_ERROR
} parse_result;

static parse_result parse_startup_options(int argc, char **argv, startup_options *options)
{
    if (options == NULL)
    {
        fail_fast("parse_startup_options called with NULL options");
    }

    for (int i = 1; i < argc; i++)
    {
        const char *option = argv[i];
        int *target = NULL;

        if (strcmp(option, "--page") == 0)
        {
            if (i + 1 >= argc)
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Missing value for option: %s", option);
                log_usage(argv[0]);
                return PARSE_RESULT_ERROR;
            }
            options->page_id = argv[++i];
            continue;
        }

        if (strcmp(option, "-w") == 0 || strcmp(option, "--width") == 0)
        {
            target = &options->size.width;
        }
        else if (strcmp(option, "-h") == 0 || strcmp(option, "--height") == 0)
        {
            target = &options->size.height;
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
 * - delegate page-specific behavior through the page descriptor interface
 */
int main(int argc, char **argv)
{
    startup_options options = {
        .size = {DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT},
        .page_id = DEFAULT_PAGE_ID,
    };

    const parse_result parse_args_result = parse_startup_options(argc, argv, &options);
    if (parse_args_result == PARSE_RESULT_HELP)
    {
        return 0;
    }
    if (parse_args_result == PARSE_RESULT_ERROR)
    {
        return 1;
    }

    const app_page_entry *selected_page = find_page_descriptor_by_id(options.page_id);
    if (selected_page == NULL || selected_page->ops == NULL || selected_page->ops->create == NULL ||
        selected_page->ops->resize == NULL || selected_page->ops->update == NULL ||
        selected_page->ops->destroy == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown or invalid page id: %s",
                     options.page_id);
        log_usage(argv[0]);
        log_available_pages();
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
        SDL_CreateWindow("CUI - a minimalist UI framework in C and SDL3", options.size.width,
                         options.size.height, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
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
    SDL_SetRenderLogicalPresentation(renderer, options.size.width, options.size.height,
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

    // Build and register the selected page.
    void *page_instance =
        selected_page->ops->create(window, &context, options.size.width, options.size.height);
    if (page_instance == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create page: %s", selected_page->id);
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
                if (!selected_page->ops->resize(page_instance, new_w, new_h))
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to resize page: %s",
                                 selected_page->id);
                    running = false;
                }
            }
            ui_runtime_handle_event(&context, &event);
        }

        // Phase 2: page-specific per-frame logic (outside widget vtables).
        if (!selected_page->ops->update(page_instance))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to update page: %s",
                         selected_page->id);
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
    selected_page->ops->destroy(page_instance);
    ui_runtime_destroy(&context);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
