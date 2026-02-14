// Pull in SDL's core API (windowing, rendering, events, timing, etc.).
#include <SDL3/SDL.h>

#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_context.h"
#include "ui/ui_fps_counter.h"
#include "ui/ui_hrule.h"
#include "ui/ui_image.h"
#include "ui/ui_layout_container.h"
#include "ui/ui_pane.h"
#include "ui/ui_scroll_view.h"
#include "ui/ui_segment_group.h"
#include "ui/ui_slider.h"
#include "ui/ui_text.h"
#include "ui/ui_text_input.h"

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
static const Uint8 COLOR_RED = 255;
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

// Demo change callback used by the checkbox element.
static void log_checkbox_change(bool checked, void *context)
{
    (void)context;
    printf("checkbox toggled: %s\n", checked ? "checked" : "unchecked");
    fflush(stdout);
}

static void log_slider_change(float value, void *context)
{
    (void)context;
    printf("slider value: %.2f\n", value);
    fflush(stdout);
}

static void log_text_input_submit(const char *value, void *context)
{
    (void)context;
    printf("text input submit: \"%s\"\n", value);
    fflush(stdout);
}

static void log_filter_change(size_t selected_index, const char *selected_label, void *context)
{
    (void)context;
    printf("filter changed: %zu (%s)\n", selected_index, selected_label);
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

// Registers a child with a layout container.
//
// Ownership rule:
// - On success, the container owns the child.
// - On failure, ownership remains with caller and the child is destroyed here
//   to avoid leaks at the call site.
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
    //
    // SDL_WINDOW_HIGH_PIXEL_DENSITY requests a full-resolution framebuffer on
    // HiDPI/Retina displays.  Without this flag the OS upscales a 1x buffer,
    // producing visibly blurry text and edges.  The logical-to-physical mapping
    // is handled later by SDL_SetRenderLogicalPresentation so that all layout
    // code can keep using WINDOW_WIDTH x WINDOW_HEIGHT coordinates.
    const char *window_title = "SDL Button Demo";
    const int window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
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

    // Create a renderer bound to the window. Passing NULL for the driver name
    // lets SDL pick the best available backend — Metal on macOS, Direct3D on
    // Windows, Vulkan/OpenGL on Linux.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Enable vertical sync so the renderer presents at the display's refresh
    // rate.  Without this the main loop would spin uncapped, wasting CPU/GPU.
    SDL_SetRenderVSync(renderer, 1);

    // With SDL_WINDOW_HIGH_PIXEL_DENSITY the actual framebuffer may be larger
    // than the requested window size (e.g. 2048x1536 on a 2x Retina display).
    // Logical presentation maps our WINDOW_WIDTH x WINDOW_HEIGHT coordinate
    // space onto that larger buffer, so every drawing call and mouse event
    // continues to use the same logical units while SDL handles the scaling.
    // LETTERBOX preserves the aspect ratio and adds black bars if needed.
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

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
    const SDL_Color border_color_red =
        (SDL_Color){COLOR_RED, COLOR_BLACK, COLOR_BLACK, COLOR_ALPHA_OPAQUE};

    // Sidebar background panel.
    ui_pane *pane = ui_pane_create(&pane_rect, pane_fill_color, &border_color_red);

    // Sidebar stack container: child widgets are auto-positioned vertically.
    // The container auto-sizes its rect.h to content height, so we pass a
    // nominal height here — the scroll view viewport controls the visible area.
    const SDL_FRect sidebar_stack_rect = {16.0F, 32.0F, pane_width - 32.0F, 0.0F};
    ui_layout_container *sidebar_stack =
        ui_layout_container_create(&sidebar_stack_rect, UI_LAYOUT_AXIS_VERTICAL, &border_color_red);

    // Sidebar child content.
    const SDL_Color sidebar_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    const char *item_1_label = "item 1";
    const char *item_2_label = "item 2";
    const char *item_3_label = "item 3";
    ui_text *item_1 =
        ui_text_create(0.0F, 0.0F, item_1_label, sidebar_text_color, &border_color_red);
    ui_text *item_2 =
        ui_text_create(0.0F, 0.0F, item_2_label, sidebar_text_color, &border_color_red);
    ui_text *item_3 =
        ui_text_create(0.0F, 0.0F, item_3_label, sidebar_text_color, &border_color_red);
    ui_checkbox *checkbox =
        ui_checkbox_create(0.0F, 0.0F, "toggle me", sidebar_text_color, sidebar_text_color,
                           sidebar_text_color, false, log_checkbox_change, NULL, &border_color_red);
    bool sidebar_item_1_in_stack = item_1 != NULL;
    bool sidebar_item_2_in_stack = item_2 != NULL;
    bool sidebar_item_3_in_stack = item_3 != NULL;
    bool checkbox_in_stack = checkbox != NULL;

    // Extra sidebar items to demonstrate scroll overflow.
    static const char *extra_labels[] = {"extra 1", "extra 2", "extra 3", "extra 4",
                                         "extra 5", "extra 6", "extra 7", "extra 8"};
#define EXTRA_ITEM_COUNT (sizeof(extra_labels) / sizeof(extra_labels[0]))
    ui_text *extra_items[EXTRA_ITEM_COUNT] = {NULL};
    for (size_t i = 0; i < EXTRA_ITEM_COUNT; ++i)
    {
        extra_items[i] =
            ui_text_create(0.0F, 0.0F, extra_labels[i], sidebar_text_color, &border_color_red);
    }

    if (!add_child_or_fail(sidebar_stack, (ui_element *)item_1) ||
        !add_child_or_fail(sidebar_stack, (ui_element *)item_2) ||
        !add_child_or_fail(sidebar_stack, (ui_element *)item_3) ||
        !add_child_or_fail(sidebar_stack, (ui_element *)checkbox))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create or register sidebar children");
        if (sidebar_stack != NULL && sidebar_stack->base.ops != NULL &&
            sidebar_stack->base.ops->destroy != NULL)
        {
            sidebar_stack->base.ops->destroy((ui_element *)sidebar_stack);
        }
        ui_context_destroy(&context);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // Horizontal divider between primary sidebar items and extra items.
    ui_hrule *sidebar_divider = ui_hrule_create(1.0F, sidebar_text_color, 0.05F);
    if (sidebar_divider != NULL)
    {
        ui_layout_container_add_child(sidebar_stack, (ui_element *)sidebar_divider);
    }

    for (size_t i = 0; i < EXTRA_ITEM_COUNT; ++i)
    {
        if (extra_items[i] != NULL)
        {
            ui_layout_container_add_child(sidebar_stack, (ui_element *)extra_items[i]);
        }
    }

    // Wrap the sidebar stack in a scroll view. The scroll view's rect defines
    // the visible viewport; the stack's auto-sized rect.h determines content
    // height and scroll range.
    // Viewport is intentionally shorter than the content to demonstrate scrolling.
    const float sidebar_scroll_height = 150.0F;
    const SDL_FRect sidebar_scroll_rect = {16.0F, 32.0F, pane_width - 32.0F, sidebar_scroll_height};
    const float sidebar_scroll_step = 24.0F;
    ui_scroll_view *sidebar_scroll = ui_scroll_view_create(
        &sidebar_scroll_rect, (ui_element *)sidebar_stack, sidebar_scroll_step, &border_color_red);

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
    button_click_handler button_click_handler_fn = log_button_press;
    void *button_click_context = NULL;

    // Todo filter segmented control in the main content area.
    const char *todo_filter_labels[] = {"ALL", "ACTIVE", "DONE"};
    const SDL_FRect filter_segment_rect = {pane_width + content_width - 304.0F, 40.0F, 264.0F,
                                           32.0F};
    ui_segment_group *filter_segment_group =
        ui_segment_group_create(&filter_segment_rect, todo_filter_labels, 3U, 0U, button_up_color,
                                pane_fill_color, button_down_color, pane_fill_color,
                                sidebar_text_color, &border_color_red, log_filter_change, NULL);

    // Interactive button that triggers `log_button_press` on click.
    ui_button *button =
        ui_button_create(&button_rect, button_up_color, button_down_color, "click me",
                         &border_color_red, button_click_handler_fn, button_click_context);

    // Demo image displayed above the button in the content area.
    // If assets/icon.png is missing, ui_image uses assets/missing-image.png.
    const float image_size = 64.0F;
    const float image_x = pane_width + (content_width - image_size) / 2.0F;
    const float image_y = button_rect.y - image_size - 24.0F;
    ui_image *icon = ui_image_create(renderer, image_x, image_y, image_size, image_size,
                                     "assets/icon.png", &border_color_red);

    const SDL_FRect slider_rect = {
        pane_width + ((content_width - 240.0F) / 2.0F),
        button_rect.y + button_rect.h + 32.0F,
        240.0F,
        24.0F,
    };
    ui_slider *slider =
        ui_slider_create(&slider_rect, 0.0F, 100.0F, 50.0F, sidebar_text_color, button_up_color,
                         button_down_color, &border_color_red, log_slider_change, NULL);

    // Demo text input below the slider in the content area.
    const SDL_FRect text_input_rect = {
        pane_width + ((content_width - 240.0F) / 2.0F),
        slider_rect.y + slider_rect.h + 32.0F,
        240.0F,
        20.0F,
    };
    const SDL_Color text_input_bg =
        (SDL_Color){COLOR_PANE, COLOR_PANE, COLOR_PANE, COLOR_ALPHA_OPAQUE};
    const SDL_Color focused_border_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    ui_text_input *text_input =
        ui_text_input_create(&text_input_rect, sidebar_text_color, text_input_bg, border_color_red,
                             focused_border_color, window, log_text_input_submit, NULL);

    // FPS counter overlays diagnostic text in the window's lower-right area.
    // Viewport dimensions are supplied so the element can anchor itself
    // correctly regardless of frame timing.
    const int fps_viewport_width = WINDOW_WIDTH;
    const int fps_viewport_height = WINDOW_HEIGHT;
    const float fps_padding = 12.0F;
    const SDL_Color fps_text_color =
        (SDL_Color){COLOR_TEXT, COLOR_TEXT, COLOR_TEXT, COLOR_ALPHA_OPAQUE};
    ui_fps_counter *fps_counter = ui_fps_counter_create(
        fps_viewport_width, fps_viewport_height, fps_padding, fps_text_color, &border_color_red);
    bool fps_counter_in_context = fps_counter != NULL;

    ui_text *debug_text = ui_text_create(pane_width + 24.0F, (float)WINDOW_HEIGHT - 48.0F,
                                         "F1-F11: debug controls", sidebar_text_color, NULL);
    int dynamic_text_counter = 0;
    bool button_label_toggled = false;

    // Register all created elements with the UI context.
    // If any registration fails, previously-added elements remain owned by
    // `context` and get cleaned up by `ui_context_destroy`.
    if (!add_element_or_fail(&context, (ui_element *)pane) ||
        !add_element_or_fail(&context, (ui_element *)sidebar_scroll) ||
        !add_element_or_fail(&context, (ui_element *)filter_segment_group) ||
        !add_element_or_fail(&context, (ui_element *)button) ||
        !add_element_or_fail(&context, (ui_element *)icon) ||
        !add_element_or_fail(&context, (ui_element *)slider) ||
        !add_element_or_fail(&context, (ui_element *)text_input) ||
        !add_element_or_fail(&context, (ui_element *)fps_counter) ||
        !add_element_or_fail(&context, (ui_element *)debug_text))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create or register UI elements");
        ui_context_destroy(&context);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // High-resolution timer for frame delta time.  SDL_GetTicksNS (SDL 3)
    // returns nanoseconds since SDL_Init; SDL_NS_PER_SECOND (= 1 000 000 000)
    // converts the elapsed delta to seconds.  This replaces the older
    // SDL_GetPerformanceCounter / SDL_GetPerformanceFrequency pattern.
    Uint64 previous_ns = SDL_GetTicksNS();

    // Main frame loop (a standard real-time app/game loop):
    // 1) measure elapsed time since previous frame
    // 2) pump and handle OS/input events
    // 3) update UI state using delta time
    // 4) render the current frame
    // 5) present rendered frame to the screen
    bool running = true;
    bool last_button_pressed = false;
    puts("Debug controls:");
    puts("F1 toggle fps in context, F2 remove item2, F3 add item2, F4 clear sidebar children");
    puts("F5 restore sidebar children, F6 set dynamic text, F7 set input, F8 clear input");
    puts("F9 toggle checkbox (notify), F10 toggle button label, F11 print input focus");
    fflush(stdout);

    while (running)
    {
        // Delta time lets update logic remain frame-rate independent.
        // Faster/slower machines still simulate time consistently.
        const Uint64 current_ns = SDL_GetTicksNS();
        const float delta_seconds = (float)(current_ns - previous_ns) / (float)SDL_NS_PER_SECOND;
        previous_ns = current_ns;

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

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
            {
                if (event.key.key == SDLK_F1)
                {
                    if (fps_counter_in_context)
                    {
                        const bool removed =
                            ui_context_remove(&context, (ui_element *)fps_counter, false);
                        fps_counter_in_context = !removed;
                        printf("F1: ui_context_remove(fps_counter) -> %s\n",
                               removed ? "true" : "false");
                    }
                    else
                    {
                        const bool added = ui_context_add(&context, (ui_element *)fps_counter);
                        fps_counter_in_context = added;
                        printf("F1: ui_context_add(fps_counter) -> %s\n", added ? "true" : "false");
                    }
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F2)
                {
                    const bool removed = ui_layout_container_remove_child(
                        sidebar_stack, (ui_element *)item_2, false);
                    if (removed)
                    {
                        sidebar_item_2_in_stack = false;
                    }
                    printf("F2: remove item_2 -> %s\n", removed ? "true" : "false");
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F3)
                {
                    bool added = false;
                    if (!sidebar_item_2_in_stack)
                    {
                        added = ui_layout_container_add_child(sidebar_stack, (ui_element *)item_2);
                        if (added)
                        {
                            sidebar_item_2_in_stack = true;
                        }
                    }
                    printf("F3: add item_2 -> %s\n", added ? "true" : "false");
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F4)
                {
                    ui_layout_container_clear_children(sidebar_stack, false);
                    sidebar_item_1_in_stack = false;
                    sidebar_item_2_in_stack = false;
                    sidebar_item_3_in_stack = false;
                    checkbox_in_stack = false;
                    puts("F4: cleared sidebar children (destroy=false)");
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F5)
                {
                    bool ok = true;
                    if (!sidebar_item_1_in_stack)
                    {
                        ok = ok &&
                             ui_layout_container_add_child(sidebar_stack, (ui_element *)item_1);
                        sidebar_item_1_in_stack = ok;
                    }
                    if (!sidebar_item_2_in_stack)
                    {
                        const bool added =
                            ui_layout_container_add_child(sidebar_stack, (ui_element *)item_2);
                        ok = ok && added;
                        sidebar_item_2_in_stack = sidebar_item_2_in_stack || added;
                    }
                    if (!sidebar_item_3_in_stack)
                    {
                        const bool added =
                            ui_layout_container_add_child(sidebar_stack, (ui_element *)item_3);
                        ok = ok && added;
                        sidebar_item_3_in_stack = sidebar_item_3_in_stack || added;
                    }
                    if (!checkbox_in_stack)
                    {
                        const bool added =
                            ui_layout_container_add_child(sidebar_stack, (ui_element *)checkbox);
                        ok = ok && added;
                        checkbox_in_stack = checkbox_in_stack || added;
                    }
                    printf("F5: restore sidebar children -> %s\n", ok ? "true" : "false");
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F6)
                {
                    char label[64];
                    dynamic_text_counter++;
                    snprintf(label, sizeof(label), "dynamic text #%d", dynamic_text_counter);
                    const bool set_ok = ui_text_set_content(item_1, label);
                    printf("F6: ui_text_set_content -> %s, current=\"%s\"\n",
                           set_ok ? "true" : "false", ui_text_get_content(item_1));
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F7)
                {
                    const bool set_ok = ui_text_input_set_value(text_input, "prefilled task");
                    printf("F7: ui_text_input_set_value -> %s, value=\"%s\"\n",
                           set_ok ? "true" : "false", ui_text_input_get_value(text_input));
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F8)
                {
                    ui_text_input_clear(text_input);
                    printf("F8: ui_text_input_clear, value=\"%s\"\n",
                           ui_text_input_get_value(text_input));
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F9)
                {
                    const bool next_checked = !ui_checkbox_is_checked(checkbox);
                    ui_checkbox_set_checked(checkbox, next_checked, true);
                    printf("F9: ui_checkbox_set_checked(notify=true), checked=%s\n",
                           ui_checkbox_is_checked(checkbox) ? "true" : "false");
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F10)
                {
                    button_label_toggled = !button_label_toggled;
                    ui_button_set_label(button, button_label_toggled ? "press me" : "click me");
                    printf("F10: ui_button_set_label, label=\"%s\"\n", ui_button_get_label(button));
                    fflush(stdout);
                }
                else if (event.key.key == SDLK_F11)
                {
                    printf("F11: ui_text_input_is_focused -> %s\n",
                           ui_text_input_is_focused(text_input) ? "true" : "false");
                    fflush(stdout);
                }
            }
            ui_context_handle_event(&context, &event);
        }

        const bool button_pressed_now = ui_button_is_pressed(button);
        if (button_pressed_now != last_button_pressed)
        {
            printf("button pressed state changed -> %s\n", button_pressed_now ? "true" : "false");
            fflush(stdout);
            last_button_pressed = button_pressed_now;
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
