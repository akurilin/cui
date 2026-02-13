// Pull in SDL's core API (windowing, rendering, events, etc.).
#include <SDL3/SDL.h>
// C standard header that provides the bool/true/false types.
#include <stdbool.h>
// C standard I/O header for printf/fprintf/fflush.
#include <stdio.h>

// Window dimensions in pixels.
static const int WINDOW_WIDTH = 1024;
static const int WINDOW_HEIGHT = 768;

// Colors used by the retro-style UI.
static const Uint8 COLOR_BG = 24;
static const Uint8 COLOR_BUTTON_UP = 170;
static const Uint8 COLOR_BUTTON_DOWN = 90;
static const Uint8 COLOR_BLACK = 0;
static const Uint8 COLOR_ALPHA_OPAQUE = 255;

// Helper function: return true if point (px, py) is inside the rectangle.
// We use float here because SDL mouse coordinates in events are floats in SDL3.
static bool is_point_in_rect(float cursor_x, float cursor_y, const SDL_FRect *rect)
{
    // Check x bounds and y bounds (inclusive) to decide if pointer is inside.
    return cursor_x >= rect->x && cursor_x <= rect->x + rect->w && cursor_y >= rect->y &&
           cursor_y <= rect->y + rect->h;
}

// Program entry point. `void` means this program expects no command-line args.
int main(void)
{
    // Initialize only the video subsystem (window + input + rendering support).
    // SDL3 returns true on success, false on failure.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        // SDL_GetError() gives a human-readable error message for the last SDL failure.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        // Non-zero return value means "program failed."
        return 1;
    }

    // Create a window named "SDL Button Demo" using configured dimensions.
    // Last argument is window flags; 0 means default behavior.
    SDL_Window *window = SDL_CreateWindow("SDL Button Demo", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    // SDL returns NULL when creation fails.
    if (window == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        // Shut SDL down before exiting so partially-initialized state is cleaned up.
        SDL_Quit();
        return 1;
    }
    // Move the window to the center of the primary display.
    // "Centered" is requested after creation here for clarity.
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Create a hardware/software renderer associated with our window.
    // Passing NULL lets SDL choose the best backend automatically.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        // If renderer creation fails, we still must destroy any resource already created.
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Ask SDL to show the system cursor.
    // Cursor is usually visible by default, but calling this makes intent explicit.
    if (!SDL_ShowCursor())
    {
        // This is non-fatal; app can still run without explicitly toggling cursor state.
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_ShowCursor failed: %s", SDL_GetError());
    }

    // Define our button rectangle (x, y, width, height) in window coordinates.
    // Keep button centered so layout scales with window size changes.
    const float button_width = 140.0F;
    const float button_height = 80.0F;
    const SDL_FRect button = {((float)WINDOW_WIDTH - button_width) / 2.0F,
                              ((float)WINDOW_HEIGHT - button_height) / 2.0F, button_width,
                              button_height};
    // Main loop control flag. As long as true, app keeps running.
    bool running = true;
    // Tracks whether button is currently pressed (for visual feedback color).
    bool is_pressed = false;

    // Main application loop: process input events and draw one frame repeatedly.
    while (running)
    {
        // SDL_Event is a tagged union that can represent many event types.
        SDL_Event event;
        // Poll all pending events in the queue until it's empty.
        while (SDL_PollEvent(&event))
        {
            // Window close request or app quit signal.
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
                // Left mouse button pressed.
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                     event.button.button == SDL_BUTTON_LEFT)
            {
                // Only treat it as a button press if the click is inside the button rect.
                if (is_point_in_rect(event.button.x, event.button.y, &button))
                {
                    // Save pressed state so next render uses "pressed" color.
                    is_pressed = true;
                    // Print test message every time the button is pressed.
                    printf("button pressed\n");
                    // Force immediate terminal output (useful when stdout is buffered).
                    fflush(stdout);
                }
                // Left mouse button released anywhere in the window.
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                     event.button.button == SDL_BUTTON_LEFT)
            {
                // Return visual state to "not pressed."
                is_pressed = false;
            }
        }

        // Set draw color to a dark gray (background), then clear frame buffer.
        SDL_SetRenderDrawColor(renderer, COLOR_BG, COLOR_BG, COLOR_BG, COLOR_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // Choose button fill color based on current press state.
        if (is_pressed)
        {
            // Pressed color: darker gray.
            SDL_SetRenderDrawColor(renderer, COLOR_BUTTON_DOWN, COLOR_BUTTON_DOWN,
                                   COLOR_BUTTON_DOWN, COLOR_ALPHA_OPAQUE);
        }
        else
        {
            // Unpressed color: lighter gray.
            SDL_SetRenderDrawColor(renderer, COLOR_BUTTON_UP, COLOR_BUTTON_UP, COLOR_BUTTON_UP,
                                   COLOR_ALPHA_OPAQUE);
        }
        // Draw filled button rectangle.
        SDL_RenderFillRect(renderer, &button);

        // Draw a black border around the button so it stands out.
        SDL_SetRenderDrawColor(renderer, COLOR_BLACK, COLOR_BLACK, COLOR_BLACK, COLOR_ALPHA_OPAQUE);
        SDL_RenderRect(renderer, &button);

        // Swap back buffer to front so this frame becomes visible.
        SDL_RenderPresent(renderer);
    }

    // Cleanup in reverse order of creation.
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    // Shut down all SDL subsystems we initialized.
    SDL_Quit();
    // Return 0 to indicate successful exit.
    return 0;
}
