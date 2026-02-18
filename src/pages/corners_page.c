#include "pages/corners_page.h"

#include "ui/ui_button.h"
#include "ui/ui_window.h"

#include <stdarg.h>
#include <stdlib.h>

typedef enum corner_button_slot
{
    CORNER_BUTTON_TOP_LEFT = 0,
    CORNER_BUTTON_TOP_CENTER,
    CORNER_BUTTON_TOP_RIGHT,
    CORNER_BUTTON_MID_LEFT,
    CORNER_BUTTON_MID_RIGHT,
    CORNER_BUTTON_BOTTOM_LEFT,
    CORNER_BUTTON_BOTTOM_CENTER,
    CORNER_BUTTON_BOTTOM_RIGHT,
    CORNER_BUTTON_COUNT
} corner_button_slot;

typedef struct anchored_button_spec
{
    const char *label;
    ui_align_h align_h;
    ui_align_v align_v;
    float offset_x;
    float offset_y;
} anchored_button_spec;

struct corners_page
{
    ui_runtime *context;
    ui_window *window_root;
    ui_button *buttons[CORNER_BUTTON_COUNT];
};

static const float BUTTON_WIDTH = 128.0F;
static const float BUTTON_HEIGHT = 44.0F;
static const float EDGE_MARGIN = 16.0F;

static const anchored_button_spec BUTTON_SPECS[CORNER_BUTTON_COUNT] = {
    [CORNER_BUTTON_TOP_LEFT] = {"TOP LEFT", UI_ALIGN_LEFT, UI_ALIGN_TOP, EDGE_MARGIN, EDGE_MARGIN},
    [CORNER_BUTTON_TOP_CENTER] = {"TOP CENTER", UI_ALIGN_CENTER_H, UI_ALIGN_TOP, 0.0F, EDGE_MARGIN},
    [CORNER_BUTTON_TOP_RIGHT] = {"TOP RIGHT", UI_ALIGN_RIGHT, UI_ALIGN_TOP, EDGE_MARGIN,
                                 EDGE_MARGIN},
    [CORNER_BUTTON_MID_LEFT] = {"MID LEFT", UI_ALIGN_LEFT, UI_ALIGN_CENTER_V, EDGE_MARGIN, 0.0F},
    [CORNER_BUTTON_MID_RIGHT] = {"MID RIGHT", UI_ALIGN_RIGHT, UI_ALIGN_CENTER_V, EDGE_MARGIN, 0.0F},
    [CORNER_BUTTON_BOTTOM_LEFT] = {"BOTTOM LEFT", UI_ALIGN_LEFT, UI_ALIGN_BOTTOM, EDGE_MARGIN,
                                   EDGE_MARGIN},
    [CORNER_BUTTON_BOTTOM_CENTER] = {"BOTTOM CENTER", UI_ALIGN_CENTER_H, UI_ALIGN_BOTTOM, 0.0F,
                                     EDGE_MARGIN},
    [CORNER_BUTTON_BOTTOM_RIGHT] = {"BOTTOM RIGHT", UI_ALIGN_RIGHT, UI_ALIGN_BOTTOM, EDGE_MARGIN,
                                    EDGE_MARGIN},
};

static _Noreturn void fail_fast(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, format, args);
    va_end(args);
    abort();
}

static void handle_corner_button_click(void *context) { (void)context; }

static ui_button *create_anchored_button(const anchored_button_spec *spec, ui_element *parent)
{
    if (spec == NULL || parent == NULL)
    {
        fail_fast("corners_page: invalid anchored button spec");
    }

    const SDL_Color color_up = {28, 30, 36, 255};
    const SDL_Color color_down = {76, 80, 92, 255};
    const SDL_Color border_color = {214, 214, 214, 255};
    ui_button *button = ui_button_create(
        &(SDL_FRect){spec->offset_x, spec->offset_y, BUTTON_WIDTH, BUTTON_HEIGHT}, color_up,
        color_down, spec->label, &border_color, handle_corner_button_click, NULL);
    if (button == NULL)
    {
        fail_fast("corners_page: failed to create anchored button");
    }

    button->base.parent = parent;
    button->base.align_h = spec->align_h;
    button->base.align_v = spec->align_v;
    return button;
}

static void remove_registered_elements(corners_page *page)
{
    if (page == NULL || page->context == NULL)
    {
        return;
    }

    for (size_t i = 0U; i < SDL_arraysize(page->buttons); ++i)
    {
        if (page->buttons[i] != NULL)
        {
            (void)ui_runtime_remove(page->context, (ui_element *)page->buttons[i], true);
            page->buttons[i] = NULL;
        }
    }

    if (page->window_root != NULL)
    {
        (void)ui_runtime_remove(page->context, (ui_element *)page->window_root, true);
        page->window_root = NULL;
    }
}

corners_page *corners_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                                  int viewport_height)
{
    (void)window;

    if (context == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_fast("corners_page_create called with invalid arguments");
    }

    corners_page *page = calloc(1U, sizeof(*page));
    if (page == NULL)
    {
        fail_fast("corners_page: failed to allocate page object");
    }

    page->context = context;
    page->window_root =
        ui_window_create(&(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height});
    if (page->window_root == NULL)
    {
        fail_fast("corners_page: failed to create window root");
    }

    for (size_t i = 0U; i < SDL_arraysize(page->buttons); ++i)
    {
        page->buttons[i] = create_anchored_button(&BUTTON_SPECS[i], &page->window_root->base);
        if (page->buttons[i] == NULL)
        {
            fail_fast("corners_page: failed to create anchored button %zu", i);
        }
    }

    if (!ui_runtime_add(page->context, (ui_element *)page->window_root))
    {
        fail_fast("corners_page: failed to register window root");
    }

    for (size_t i = 0U; i < SDL_arraysize(page->buttons); ++i)
    {
        if (!ui_runtime_add(page->context, (ui_element *)page->buttons[i]))
        {
            fail_fast("corners_page: failed to register button %zu", i);
        }
    }

    return page;
}

bool corners_page_resize(corners_page *page, int viewport_width, int viewport_height)
{
    if (page == NULL || page->window_root == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_fast("corners_page_resize called with invalid arguments");
    }

    if (!ui_window_set_size(page->window_root, (float)viewport_width, (float)viewport_height))
    {
        fail_fast("corners_page: failed to resize window root");
    }
    return true;
}

bool corners_page_update(corners_page *page)
{
    if (page == NULL)
    {
        fail_fast("corners_page_update called with NULL page");
    }
    return true;
}

void corners_page_destroy(corners_page *page)
{
    if (page == NULL)
    {
        fail_fast("corners_page_destroy called with NULL page");
    }

    remove_registered_elements(page);
    free(page);
}

static void *create_corners_page_instance(SDL_Window *window, ui_runtime *context,
                                          int viewport_width, int viewport_height)
{
    return (void *)corners_page_create(window, context, viewport_width, viewport_height);
}

static bool resize_corners_page_instance(void *page_instance, int viewport_width,
                                         int viewport_height)
{
    return corners_page_resize((corners_page *)page_instance, viewport_width, viewport_height);
}

static bool update_corners_page_instance(void *page_instance)
{
    return corners_page_update((corners_page *)page_instance);
}

static void destroy_corners_page_instance(void *page_instance)
{
    corners_page_destroy((corners_page *)page_instance);
}

const app_page_ops corners_page_ops = {
    .create = create_corners_page_instance,
    .resize = resize_corners_page_instance,
    .update = update_corners_page_instance,
    .destroy = destroy_corners_page_instance,
};
