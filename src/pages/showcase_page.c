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

static void destroy_element(ui_element *element)
{
    if (element != NULL && element->ops != NULL && element->ops->destroy != NULL)
    {
        element->ops->destroy(element);
    }
}

static bool register_element(showcase_page *page, ui_element *element)
{
    if (page == NULL || page->context == NULL || element == NULL)
    {
        return false;
    }

    if (!ui_runtime_add(page->context, element))
    {
        destroy_element(element);
        return false;
    }

    if (page->registered_count >= SDL_arraysize(page->registered_elements))
    {
        (void)ui_runtime_remove(page->context, element, true);
        return false;
    }

    page->registered_elements[page->registered_count++] = element;
    return true;
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

static bool add_child_or_fail(ui_layout_container *container, ui_element *child)
{
    if (!ui_layout_container_add_child(container, child))
    {
        destroy_element(child);
        return false;
    }

    return true;
}

static bool set_text_content(ui_text *text, const char *content)
{
    if (text == NULL || content == NULL)
    {
        return false;
    }

    if (!ui_text_set_content(text, content))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to update showcase text: %s", content);
        return false;
    }

    return true;
}

static void handle_demo_button_click(void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL)
    {
        return;
    }

    (void)set_text_content(page->status_text, "BUTTON CLICKED");
}

static void handle_checkbox_change(bool checked, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL)
    {
        return;
    }

    (void)set_text_content(page->checkbox_state_text, checked ? "CHECKBOX: ON" : "CHECKBOX: OFF");
}

static void handle_slider_change(float value, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || page->slider_value_text == NULL)
    {
        return;
    }

    char slider_label[64];
    SDL_snprintf(slider_label, sizeof(slider_label), "SLIDER VALUE: %.1f", value);
    (void)set_text_content(page->slider_value_text, slider_label);
}

static void handle_segment_change(size_t selected_index, const char *selected_label, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || page->segment_value_text == NULL)
    {
        return;
    }

    char segment_label[80];
    SDL_snprintf(segment_label, sizeof(segment_label), "SEGMENT %zu: %s", selected_index + 1U,
                 selected_label);
    (void)set_text_content(page->segment_value_text, segment_label);
}

static void handle_text_input_submit(const char *value, void *context)
{
    showcase_page *page = (showcase_page *)context;
    if (page == NULL || value == NULL)
    {
        return;
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

    (void)set_text_content(page->status_text, submitted);
}

static ui_text *create_text_label(const char *content, SDL_Color color)
{
    return ui_text_create(0.0F, 0.0F, content, color, NULL);
}

static ui_layout_container *create_showcase_content(showcase_page *page, SDL_Window *window,
                                                    SDL_Renderer *renderer)
{
    if (page == NULL || window == NULL || renderer == NULL)
    {
        return NULL;
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
        return NULL;
    }

    ui_text *title = create_text_label("UI SHOWCASE PAGE", color_ink);
    if (title == NULL || !add_child_or_fail(content, (ui_element *)title))
    {
        destroy_element((ui_element *)title);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *subtitle = create_text_label("Every built-in widget on one page", color_muted);
    if (subtitle == NULL || !add_child_or_fail(content, (ui_element *)subtitle))
    {
        destroy_element((ui_element *)subtitle);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *top_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (top_rule == NULL || !add_child_or_fail(content, (ui_element *)top_rule))
    {
        destroy_element((ui_element *)top_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *pane_label = create_text_label("UI_PANE", color_ink);
    if (pane_label == NULL || !add_child_or_fail(content, (ui_element *)pane_label))
    {
        destroy_element((ui_element *)pane_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_pane *pane =
        ui_pane_create(&(SDL_FRect){0.0F, 0.0F, 100.0F, 56.0F}, color_pane_fill, &color_border);
    if (pane == NULL || !add_child_or_fail(content, (ui_element *)pane))
    {
        destroy_element((ui_element *)pane);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *controls_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (controls_rule == NULL || !add_child_or_fail(content, (ui_element *)controls_rule))
    {
        destroy_element((ui_element *)controls_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *controls_label = create_text_label("UI_BUTTON + UI_CHECKBOX", color_ink);
    if (controls_label == NULL || !add_child_or_fail(content, (ui_element *)controls_label))
    {
        destroy_element((ui_element *)controls_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_layout_container *controls_row = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 100.0F, 44.0F}, UI_LAYOUT_AXIS_HORIZONTAL, NULL);
    if (controls_row == NULL)
    {
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_button *button = ui_button_create(&(SDL_FRect){0.0F, 0.0F, 160.0F, 32.0F}, color_button_up,
                                         color_button_down, "CLICK BUTTON", &color_border,
                                         handle_demo_button_click, page);
    if (button == NULL || !add_child_or_fail(controls_row, (ui_element *)button))
    {
        destroy_element((ui_element *)button);
        destroy_element((ui_element *)controls_row);
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->checkbox = ui_checkbox_create(0.0F, 0.0F, "TOGGLE CHECKBOX", color_ink, color_ink,
                                        color_ink, false, handle_checkbox_change, page, NULL);
    if (page->checkbox == NULL || !add_child_or_fail(controls_row, (ui_element *)page->checkbox))
    {
        destroy_element((ui_element *)page->checkbox);
        page->checkbox = NULL;
        destroy_element((ui_element *)controls_row);
        destroy_element((ui_element *)content);
        return NULL;
    }

    if (!add_child_or_fail(content, (ui_element *)controls_row))
    {
        destroy_element((ui_element *)controls_row);
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->checkbox_state_text = create_text_label("CHECKBOX: OFF", color_muted);
    if (page->checkbox_state_text == NULL ||
        !add_child_or_fail(content, (ui_element *)page->checkbox_state_text))
    {
        destroy_element((ui_element *)page->checkbox_state_text);
        page->checkbox_state_text = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *input_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (input_rule == NULL || !add_child_or_fail(content, (ui_element *)input_rule))
    {
        destroy_element((ui_element *)input_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *input_label = create_text_label("UI_TEXT_INPUT", color_ink);
    if (input_label == NULL || !add_child_or_fail(content, (ui_element *)input_label))
    {
        destroy_element((ui_element *)input_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text_input *text_input =
        ui_text_input_create(&(SDL_FRect){0.0F, 0.0F, 520.0F, 36.0F}, color_ink, color_input_bg,
                             color_border, color_focus_border, "Type and press Enter", color_muted,
                             window, handle_text_input_submit, page);
    if (text_input == NULL || !add_child_or_fail(content, (ui_element *)text_input))
    {
        destroy_element((ui_element *)text_input);
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->status_text = create_text_label("BUTTON/INPUT STATUS: READY", color_muted);
    if (page->status_text == NULL || !add_child_or_fail(content, (ui_element *)page->status_text))
    {
        destroy_element((ui_element *)page->status_text);
        page->status_text = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *segment_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (segment_rule == NULL || !add_child_or_fail(content, (ui_element *)segment_rule))
    {
        destroy_element((ui_element *)segment_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *segment_label = create_text_label("UI_SEGMENT_GROUP", color_ink);
    if (segment_label == NULL || !add_child_or_fail(content, (ui_element *)segment_label))
    {
        destroy_element((ui_element *)segment_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->segment_group = ui_segment_group_create(
        &(SDL_FRect){0.0F, 0.0F, 420.0F, 36.0F}, SHOWCASE_SEGMENTS,
        SDL_arraysize(SHOWCASE_SEGMENTS), 0U, color_segment_bg, color_segment_selected,
        color_segment_pressed, color_ink, (SDL_Color){244, 246, 255, 255}, &color_border,
        handle_segment_change, page);
    if (page->segment_group == NULL ||
        !add_child_or_fail(content, (ui_element *)page->segment_group))
    {
        destroy_element((ui_element *)page->segment_group);
        page->segment_group = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->segment_value_text = create_text_label("SEGMENT 1: FIRST", color_muted);
    if (page->segment_value_text == NULL ||
        !add_child_or_fail(content, (ui_element *)page->segment_value_text))
    {
        destroy_element((ui_element *)page->segment_value_text);
        page->segment_value_text = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *slider_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (slider_rule == NULL || !add_child_or_fail(content, (ui_element *)slider_rule))
    {
        destroy_element((ui_element *)slider_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *slider_label = create_text_label("UI_SLIDER", color_ink);
    if (slider_label == NULL || !add_child_or_fail(content, (ui_element *)slider_label))
    {
        destroy_element((ui_element *)slider_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->slider = ui_slider_create(
        &(SDL_FRect){0.0F, 0.0F, 420.0F, 30.0F}, 0.0F, 100.0F, 35.0F, color_slider_track,
        color_slider_thumb, color_slider_thumb_active, &color_border, handle_slider_change, page);
    if (page->slider == NULL || !add_child_or_fail(content, (ui_element *)page->slider))
    {
        destroy_element((ui_element *)page->slider);
        page->slider = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    page->slider_value_text = create_text_label("SLIDER VALUE: 35.0", color_muted);
    if (page->slider_value_text == NULL ||
        !add_child_or_fail(content, (ui_element *)page->slider_value_text))
    {
        destroy_element((ui_element *)page->slider_value_text);
        page->slider_value_text = NULL;
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *image_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (image_rule == NULL || !add_child_or_fail(content, (ui_element *)image_rule))
    {
        destroy_element((ui_element *)image_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *image_label = create_text_label("UI_IMAGE", color_ink);
    if (image_label == NULL || !add_child_or_fail(content, (ui_element *)image_label))
    {
        destroy_element((ui_element *)image_label);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_image *image =
        ui_image_create(renderer, 0.0F, 0.0F, 120.0F, 120.0F, "assets/icon.png", &color_border);
    if (image == NULL || !add_child_or_fail(content, (ui_element *)image))
    {
        destroy_element((ui_element *)image);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_hrule *end_rule = ui_hrule_create(8.0F, color_line, 0.0F);
    if (end_rule == NULL || !add_child_or_fail(content, (ui_element *)end_rule))
    {
        destroy_element((ui_element *)end_rule);
        destroy_element((ui_element *)content);
        return NULL;
    }

    ui_text *footer_text = create_text_label("END OF SHOWCASE", color_muted);
    if (footer_text == NULL || !add_child_or_fail(content, (ui_element *)footer_text))
    {
        destroy_element((ui_element *)footer_text);
        destroy_element((ui_element *)content);
        return NULL;
    }

    handle_checkbox_change(ui_checkbox_is_checked(page->checkbox), page);
    handle_slider_change(page->slider->value, page);
    handle_segment_change(ui_segment_group_get_selected_index(page->segment_group),
                          ui_segment_group_get_selected_label(page->segment_group), page);

    return content;
}

static bool relayout_page(showcase_page *page)
{
    if (page == NULL || page->window_root == NULL || page->background == NULL ||
        page->scroll_view == NULL)
    {
        return false;
    }

    if (!ui_window_set_size(page->window_root, (float)page->viewport_width,
                            (float)page->viewport_height))
    {
        return false;
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

    return true;
}

showcase_page *showcase_page_create(SDL_Window *window, ui_runtime *context, int viewport_width,
                                    int viewport_height)
{
    if (window == NULL || context == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        return NULL;
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (renderer == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "showcase_page_create requires window renderer: %s", SDL_GetError());
        return NULL;
    }

    showcase_page *page = calloc(1U, sizeof(*page));
    if (page == NULL)
    {
        return NULL;
    }

    page->context = context;
    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;

    page->window_root =
        ui_window_create(&(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height});
    if (page->window_root == NULL || !register_element(page, (ui_element *)page->window_root))
    {
        showcase_page_destroy(page);
        return NULL;
    }

    const SDL_Color color_bg = {243, 245, 250, 255};
    page->background = ui_pane_create(
        &(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height}, color_bg, NULL);
    if (page->background == NULL)
    {
        showcase_page_destroy(page);
        return NULL;
    }
    page->background->base.parent = &page->window_root->base;

    if (!register_element(page, (ui_element *)page->background))
    {
        page->background = NULL;
        showcase_page_destroy(page);
        return NULL;
    }

    ui_layout_container *content = create_showcase_content(page, window, renderer);
    if (content == NULL)
    {
        showcase_page_destroy(page);
        return NULL;
    }

    const SDL_Color color_scroll_border = {206, 209, 217, 255};
    page->scroll_view = ui_scroll_view_create(
        &(SDL_FRect){PAGE_MARGIN, PAGE_MARGIN, (float)viewport_width - (2.0F * PAGE_MARGIN),
                     (float)viewport_height - (2.0F * PAGE_MARGIN) - FOOTER_RESERVE},
        (ui_element *)content, MAIN_SCROLL_STEP, &color_scroll_border);
    if (page->scroll_view == NULL)
    {
        destroy_element((ui_element *)content);
        showcase_page_destroy(page);
        return NULL;
    }
    page->scroll_view->base.parent = &page->window_root->base;

    if (!register_element(page, (ui_element *)page->scroll_view))
    {
        page->scroll_view = NULL;
        showcase_page_destroy(page);
        return NULL;
    }

    const SDL_Color color_fps = {56, 61, 76, 255};
    page->fps_counter =
        ui_fps_counter_create(viewport_width, viewport_height, 16.0F, color_fps, NULL);
    if (page->fps_counter == NULL)
    {
        showcase_page_destroy(page);
        return NULL;
    }
    page->fps_counter->base.parent = &page->window_root->base;

    if (!register_element(page, (ui_element *)page->fps_counter))
    {
        page->fps_counter = NULL;
        showcase_page_destroy(page);
        return NULL;
    }

    if (!relayout_page(page))
    {
        showcase_page_destroy(page);
        return NULL;
    }

    return page;
}

bool showcase_page_resize(showcase_page *page, int viewport_width, int viewport_height)
{
    if (page == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        return false;
    }

    page->viewport_width = viewport_width;
    page->viewport_height = viewport_height;
    return relayout_page(page);
}

bool showcase_page_update(showcase_page *page) { return page != NULL; }

void showcase_page_destroy(showcase_page *page)
{
    if (page == NULL)
    {
        return;
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
