#include "pages/showcase_page.h"

#include "ui/ui_button.h"
#include "ui/ui_checkbox.h"
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
#include "ui/ui_window.h"

#include <stdarg.h>
#include <stdlib.h>

struct showcase_page
{
    ui_runtime *context;
    ui_element *registered_elements[8];
    size_t registered_count;

    int viewport_width;
    int viewport_height;

    ui_window *window_root;
    ui_pane *background;
    ui_scroll_view *scroll_view;
    ui_fps_counter *fps_counter;

    ui_text *status_text;
    ui_text *checkbox_state_text;
    ui_text *slider_value_text;
    ui_text *segment_value_text;

    ui_checkbox *checkbox;
    ui_slider *slider;
    ui_segment_group *segment_group;
};

static const float PAGE_MARGIN = 20.0F;
static const float FOOTER_RESERVE = 40.0F;
static const float MAIN_SCROLL_STEP = 24.0F;
static const char *SHOWCASE_SEGMENTS[] = {"FIRST", "SECOND", "THIRD"};

static _Noreturn void fail_fast(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, format, args);
    va_end(args);
    abort();
}

static void register_element(showcase_page *page, ui_element *element)
{
    if (page == NULL || page->context == NULL || element == NULL)
    {
        fail_fast("showcase_page: invalid register_element input");
    }

    if (!ui_runtime_add(page->context, element))
    {
        fail_fast("showcase_page: ui_runtime_add failed");
    }

    if (page->registered_count >= SDL_arraysize(page->registered_elements))
    {
        fail_fast("showcase_page: registered element tracker capacity exceeded");
    }

    page->registered_elements[page->registered_count++] = element;
}

static void unregister_elements(showcase_page *page)
{
    if (page == NULL || page->context == NULL)
    {
        return;
    }

    for (size_t i = page->registered_count; i > 0U; --i)
    {
        ui_element *element = page->registered_elements[i - 1U];
        (void)ui_runtime_remove(page->context, element, true);
    }

    page->registered_count = 0U;
}

static void add_child_or_fail(ui_layout_container *container, ui_element *child)
{
    if (container == NULL || child == NULL)
    {
        fail_fast("showcase_page: invalid add_child_or_fail input");
    }

    if (!ui_layout_container_add_child(container, child))
    {
        fail_fast("showcase_page: failed to add child to layout container");
    }
}

static void set_text_content(ui_text *text, const char *content)
{
    if (text == NULL || content == NULL)
    {
        fail_fast("showcase_page: invalid set_text_content input");
    }

    if (!ui_text_set_content(text, content))
    {
        fail_fast("showcase_page: failed to update text content");
    }
}

static void handle_demo_button_click(void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL)
    {
        fail_fast("showcase_page: demo button callback context is NULL");
    }

    set_text_content(page->status_text, "BUTTON CLICKED");
}

static void handle_checkbox_change(bool checked, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL)
    {
        fail_fast("showcase_page: checkbox callback context is NULL");
    }

    set_text_content(page->checkbox_state_text, checked ? "CHECKBOX: ON" : "CHECKBOX: OFF");
}

static void handle_slider_change(float value, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || page->slider_value_text == NULL)
    {
        fail_fast("showcase_page: slider callback state is invalid");
    }

    char slider_label[64];
    SDL_snprintf(slider_label, sizeof(slider_label), "SLIDER VALUE: %.1f", value);
    set_text_content(page->slider_value_text, slider_label);
}

static void handle_segment_change(size_t selected_index, const char *selected_label, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || page->segment_value_text == NULL || selected_label == NULL)
    {
        fail_fast("showcase_page: segment callback state is invalid");
    }

    char segment_label[80];
    SDL_snprintf(segment_label, sizeof(segment_label), "SEGMENT %zu: %s", selected_index + 1U,
                 selected_label);
    set_text_content(page->segment_value_text, segment_label);
}

static void handle_text_input_submit(const char *value, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || value == NULL)
    {
        fail_fast("showcase_page: text-input callback state is invalid");
    }

    char submitted[160];
    if (value[0] == '\0')
    {
        SDL_snprintf(submitted, sizeof(submitted), "INPUT SUBMIT: (EMPTY)");
    }
    else
    {
        SDL_snprintf(submitted, sizeof(submitted), "INPUT SUBMIT: %s", value);
    }

    set_text_content(page->status_text, submitted);
}

static ui_text *create_text_label_or_fail(const char *content, SDL_Color color)
{
    ui_text *label = ui_text_create(0.0F, 0.0F, content, color, NULL);
    if (label == NULL)
    {
        fail_fast("showcase_page: failed to create text label");
    }

    return label;
}

static void add_text_label(ui_layout_container *content, const char *label, SDL_Color color)
{
    ui_text *text = create_text_label_or_fail(label, color);
    add_child_or_fail(content, (ui_element *)text);
}

static ui_layout_container *create_showcase_content(showcase_page *page, SDL_Window *window,
                                                    SDL_Renderer *renderer)
{
    if (page == NULL || window == NULL || renderer == NULL)
    {
        fail_fast("showcase_page: invalid create_showcase_content input");
    }

    const SDL_Color color_ink = {31, 34, 44, 255};
    const SDL_Color color_muted = {92, 95, 110, 255};
    const SDL_Color color_line = {184, 186, 194, 255};
    const SDL_Color color_border = {210, 212, 218, 255};
    const SDL_Color color_pane_fill = {230, 232, 239, 255};
    const SDL_Color color_button_up = {49, 74, 122, 255};
    const SDL_Color color_button_down = {34, 52, 84, 255};
    const SDL_Color color_input_bg = {252, 252, 252, 255};
    const SDL_Color color_focus_border = {62, 130, 255, 255};
    const SDL_Color color_segment_bg = {232, 233, 238, 255};
    const SDL_Color color_segment_selected = {64, 95, 150, 255};
    const SDL_Color color_segment_pressed = {43, 72, 122, 255};
    const SDL_Color color_slider_track = {183, 185, 196, 255};
    const SDL_Color color_slider_thumb = {74, 79, 96, 255};
    const SDL_Color color_slider_thumb_active = {38, 46, 66, 255};

    ui_layout_container *content = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 720.0F, 1200.0F}, UI_LAYOUT_AXIS_VERTICAL, &color_border);
    if (content == NULL)
    {
        fail_fast("showcase_page: failed to create content container");
    }

    add_text_label(content, "UI SHOWCASE PAGE", color_ink);
    add_text_label(content, "Every built-in widget on one page", color_muted);

    ui_hrule *top_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (top_rule == NULL)
    {
        fail_fast("showcase_page: failed to create top rule");
    }
    add_child_or_fail(content, (ui_element *)top_rule);

    add_text_label(content, "UI_PANE", color_ink);

    ui_pane *pane =
        ui_pane_create(&(SDL_FRect){0.0F, 0.0F, 100.0F, 56.0F}, color_pane_fill, &color_border);
    if (pane == NULL)
    {
        fail_fast("showcase_page: failed to create demo pane");
    }
    add_child_or_fail(content, (ui_element *)pane);

    ui_hrule *controls_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (controls_rule == NULL)
    {
        fail_fast("showcase_page: failed to create controls rule");
    }
    add_child_or_fail(content, (ui_element *)controls_rule);

    add_text_label(content, "UI_BUTTON + UI_CHECKBOX", color_ink);

    ui_layout_container *controls_row = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 100.0F, 44.0F}, UI_LAYOUT_AXIS_HORIZONTAL, NULL);
    if (controls_row == NULL)
    {
        fail_fast("showcase_page: failed to create controls row");
    }

    ui_button *button = ui_button_create(&(SDL_FRect){0.0F, 0.0F, 160.0F, 32.0F}, color_button_up,
                                         color_button_down, "CLICK BUTTON", &color_border,
                                         handle_demo_button_click, page);
    if (button == NULL)
    {
        fail_fast("showcase_page: failed to create demo button");
    }
    add_child_or_fail(controls_row, (ui_element *)button);

    page->checkbox = ui_checkbox_create(0.0F, 0.0F, "TOGGLE CHECKBOX", color_ink, color_ink,
                                        color_ink, false, handle_checkbox_change, page, NULL);
    if (page->checkbox == NULL)
    {
        fail_fast("showcase_page: failed to create checkbox");
    }
    add_child_or_fail(controls_row, (ui_element *)page->checkbox);

    add_child_or_fail(content, (ui_element *)controls_row);

    page->checkbox_state_text = create_text_label_or_fail("CHECKBOX: OFF", color_muted);
    add_child_or_fail(content, (ui_element *)page->checkbox_state_text);

    ui_hrule *input_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (input_rule == NULL)
    {
        fail_fast("showcase_page: failed to create input rule");
    }
    add_child_or_fail(content, (ui_element *)input_rule);

    add_text_label(content, "UI_TEXT_INPUT", color_ink);

    ui_text_input *text_input =
        ui_text_input_create(&(SDL_FRect){0.0F, 0.0F, 520.0F, 36.0F}, color_ink, color_input_bg,
                             color_border, color_focus_border, "Type and press Enter", color_muted,
                             window, handle_text_input_submit, page);
    if (text_input == NULL)
    {
        fail_fast("showcase_page: failed to create text input");
    }
    add_child_or_fail(content, (ui_element *)text_input);

    page->status_text = create_text_label_or_fail("BUTTON/INPUT STATUS: READY", color_muted);
    add_child_or_fail(content, (ui_element *)page->status_text);

    ui_hrule *segment_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (segment_rule == NULL)
    {
        fail_fast("showcase_page: failed to create segment rule");
    }
    add_child_or_fail(content, (ui_element *)segment_rule);

    add_text_label(content, "UI_SEGMENT_GROUP", color_ink);

    page->segment_group = ui_segment_group_create(
        &(SDL_FRect){0.0F, 0.0F, 420.0F, 36.0F}, SHOWCASE_SEGMENTS,
        SDL_arraysize(SHOWCASE_SEGMENTS), 0U, color_segment_bg, color_segment_selected,
        color_segment_pressed, color_ink, (SDL_Color){244, 246, 255, 255}, &color_border,
        handle_segment_change, page);
    if (page->segment_group == NULL)
    {
        fail_fast("showcase_page: failed to create segment group");
    }
    add_child_or_fail(content, (ui_element *)page->segment_group);

    page->segment_value_text = create_text_label_or_fail("SEGMENT 1: FIRST", color_muted);
    add_child_or_fail(content, (ui_element *)page->segment_value_text);

    ui_hrule *slider_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (slider_rule == NULL)
    {
        fail_fast("showcase_page: failed to create slider rule");
    }
    add_child_or_fail(content, (ui_element *)slider_rule);

    add_text_label(content, "UI_SLIDER", color_ink);

    page->slider = ui_slider_create(
        &(SDL_FRect){0.0F, 0.0F, 420.0F, 30.0F}, 0.0F, 100.0F, 35.0F, color_slider_track,
        color_slider_thumb, color_slider_thumb_active, &color_border, handle_slider_change, page);
    if (page->slider == NULL)
    {
        fail_fast("showcase_page: failed to create slider");
    }
    add_child_or_fail(content, (ui_element *)page->slider);

    page->slider_value_text = create_text_label_or_fail("SLIDER VALUE: 35.0", color_muted);
    add_child_or_fail(content, (ui_element *)page->slider_value_text);

    ui_hrule *image_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (image_rule == NULL)
    {
        fail_fast("showcase_page: failed to create image rule");
    }
    add_child_or_fail(content, (ui_element *)image_rule);

    add_text_label(content, "UI_IMAGE", color_ink);

    ui_image *image =
        ui_image_create(renderer, 0.0F, 0.0F, 120.0F, 120.0F, "assets/icon.png", &color_border);
    if (image == NULL)
    {
        fail_fast("showcase_page: failed to create image widget");
    }
    add_child_or_fail(content, (ui_element *)image);

    ui_hrule *end_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (end_rule == NULL)
    {
        fail_fast("showcase_page: failed to create end rule");
    }
    add_child_or_fail(content, (ui_element *)end_rule);

    add_text_label(content, "END OF SHOWCASE", color_muted);

    handle_checkbox_change(ui_checkbox_is_checked(page->checkbox), page);
    handle_slider_change(page->slider->value, page);
    handle_segment_change(ui_segment_group_get_selected_index(page->segment_group),
                          ui_segment_group_get_selected_label(page->segment_group), page);

    return content;
}

static void relayout_page(showcase_page *page)
{
    if (page == NULL || page->window_root == NULL || page->background == NULL ||
        page->scroll_view == NULL)
    {
        fail_fast("showcase_page: invalid relayout state");
    }

    if (!ui_window_set_size(page->window_root, (float)page->viewport_width,
                            (float)page->viewport_height))
    {
        fail_fast("showcase_page: failed to resize window root");
    }

    page->background->base.rect.x = 0.0F;
    page->background->base.rect.y = 0.0F;
    page->background->base.rect.w = (float)page->viewport_width;
    page->background->base.rect.h = (float)page->viewport_height;

    float scroll_width = (float)page->viewport_width - (PAGE_MARGIN * 2.0F);
    float scroll_height = (float)page->viewport_height - (PAGE_MARGIN * 2.0F) - FOOTER_RESERVE;

    if (scroll_width < 120.0F)
    {
        scroll_width = 120.0F;
    }
    if (scroll_height < 80.0F)
    {
        scroll_height = 80.0F;
    }

    page->scroll_view->base.rect.x = PAGE_MARGIN;
    page->scroll_view->base.rect.y = PAGE_MARGIN;
    page->scroll_view->base.rect.w = scroll_width;
    page->scroll_view->base.rect.h = scroll_height;
}

showcase_page *showcase_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                                    int viewport_height)
{
    if (window == NULL || context == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_fast("showcase_page_create called with invalid arguments");
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (renderer == NULL)
    {
        fail_fast("showcase_page_create requires window renderer: %s", SDL_GetError());
    }

    showcase_page *page = calloc(1U, sizeof(*page));
    if (page == NULL)
    {
        fail_fast("showcase_page: failed to allocate page object");
    }

    page->context = context;
    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;

    page->window_root =
        ui_window_create(&(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height});
    if (page->window_root == NULL)
    {
        fail_fast("showcase_page: failed to create window root");
    }
    register_element(page, (ui_element *)page->window_root);

    const SDL_Color color_bg = {243, 245, 250, 255};
    page->background = ui_pane_create(
        &(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height}, color_bg, NULL);
    if (page->background == NULL)
    {
        fail_fast("showcase_page: failed to create background pane");
    }
    page->background->base.parent = &page->window_root->base;
    register_element(page, (ui_element *)page->background);

    ui_layout_container *content = create_showcase_content(page, window, renderer);

    const SDL_Color color_scroll_border = {206, 209, 217, 255};
    page->scroll_view = ui_scroll_view_create(
        &(SDL_FRect){PAGE_MARGIN, PAGE_MARGIN, (float)viewport_width - (2.0F * PAGE_MARGIN),
                     (float)viewport_height - (2.0F * PAGE_MARGIN) - FOOTER_RESERVE},
        (ui_element *)content, MAIN_SCROLL_STEP, &color_scroll_border);
    if (page->scroll_view == NULL)
    {
        fail_fast("showcase_page: failed to create scroll view");
    }
    page->scroll_view->base.parent = &page->window_root->base;
    register_element(page, (ui_element *)page->scroll_view);

    const SDL_Color color_fps = {56, 61, 76, 255};
    page->fps_counter =
        ui_fps_counter_create(viewport_width, viewport_height, 16.0F, color_fps, NULL);
    if (page->fps_counter == NULL)
    {
        fail_fast("showcase_page: failed to create fps counter");
    }
    page->fps_counter->base.parent = &page->window_root->base;
    register_element(page, (ui_element *)page->fps_counter);

    relayout_page(page);

    return page;
}

bool showcase_page_resize(showcase_page *page, int viewport_width, int viewport_height)
{
    if (page == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_fast("showcase_page_resize called with invalid arguments");
    }

    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;
    relayout_page(page);
    return true;
}

bool showcase_page_update(showcase_page *page)
{
    if (page == NULL)
    {
        fail_fast("showcase_page_update called with NULL page");
    }

    return true;
}

void showcase_page_destroy(showcase_page *page)
{
    if (page == NULL)
    {
        fail_fast("showcase_page_destroy called with NULL page");
    }

    unregister_elements(page);
    free(page);
}

static void *create_showcase_page_instance(SDL_Window *window, ui_runtime *context,
                                           int viewport_width, int viewport_height)
{
    return (void *)showcase_page_create(window, context, viewport_width, viewport_height);
}

static bool resize_showcase_page_instance(void *page_instance, int viewport_width,
                                          int viewport_height)
{
    return showcase_page_resize((showcase_page *)page_instance, viewport_width, viewport_height);
}

static bool update_showcase_page_instance(void *page_instance)
{
    return showcase_page_update((showcase_page *)page_instance);
}

static void destroy_showcase_page_instance(void *page_instance)
{
    showcase_page_destroy((showcase_page *)page_instance);
}

const app_page_ops showcase_page_ops = {
    .create = create_showcase_page_instance,
    .resize = resize_showcase_page_instance,
    .update = update_showcase_page_instance,
    .destroy = destroy_showcase_page_instance,
};
