#include "pages/page_shell.h"

#include "util/fail_fast.h"

#include <string.h>

static void validate_page_name(const char *page_name)
{
    if (page_name == NULL || page_name[0] == '\0')
    {
        fail_fast("app_page_shell: missing page_name");
    }
}

static void fail_invalid_shell(const char *page_name, const char *action)
{
    fail_fast("%s: invalid page shell in %s", page_name, action);
}

void app_page_shell_register_element(app_page_shell *shell, ui_element *element,
                                     const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || shell->context == NULL || element == NULL)
    {
        fail_invalid_shell(page_name, "app_page_shell_register_element");
    }

    if (!ui_runtime_add(shell->context, element))
    {
        fail_fast("%s: ui_runtime_add failed", page_name);
    }

    if (shell->registered_count >= SDL_arraysize(shell->registered_elements))
    {
        fail_fast("%s: page shell registration capacity exceeded", page_name);
    }

    shell->registered_elements[shell->registered_count++] = element;
}

void app_page_shell_init(app_page_shell *shell, ui_runtime *context, int viewport_width,
                         int viewport_height, const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || context == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_invalid_shell(page_name, "app_page_shell_init");
    }

    memset(shell, 0, sizeof(*shell));
    shell->context = context;

    shell->window_root =
        ui_window_create(&(SDL_FRect){0.0F, 0.0F, (float)viewport_width, (float)viewport_height});
    if (shell->window_root == NULL)
    {
        fail_fast("%s: failed to create window root", page_name);
    }

    app_page_shell_register_element(shell, (ui_element *)shell->window_root, page_name);
}

void app_page_shell_add_window_child(app_page_shell *shell, ui_element *child,
                                     const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || shell->window_root == NULL || child == NULL)
    {
        fail_invalid_shell(page_name, "app_page_shell_add_window_child");
    }

    if (!ui_window_add_child(shell->window_root, child))
    {
        fail_fast("%s: ui_window_add_child failed", page_name);
    }
}

void app_page_shell_resize_root(app_page_shell *shell, int viewport_width, int viewport_height,
                                const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || shell->window_root == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_invalid_shell(page_name, "app_page_shell_resize_root");
    }

    if (!ui_window_set_size(shell->window_root, (float)viewport_width, (float)viewport_height))
    {
        fail_fast("%s: failed to resize window root", page_name);
    }
}

static void measure_and_arrange_element(ui_element *element, const SDL_FRect *rect,
                                        const char *page_name)
{
    validate_page_name(page_name);
    if (element == NULL || rect == NULL)
    {
        fail_fast("%s: invalid measure_and_arrange_element input", page_name);
    }

    ui_element_measure(element, rect);
    SDL_FRect final_rect = *rect;
    if (element->ops != NULL && element->ops->measure != NULL)
    {
        final_rect.w = element->rect.w;
        final_rect.h = element->rect.h;
    }
    ui_element_arrange(element, &final_rect);
}

void app_page_shell_arrange_root(app_page_shell *shell, int viewport_width, int viewport_height,
                                 const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || shell->window_root == NULL || viewport_width <= 0 || viewport_height <= 0)
    {
        fail_invalid_shell(page_name, "app_page_shell_arrange_root");
    }

    const SDL_FRect window_root_rect = {0.0F, 0.0F, (float)viewport_width, (float)viewport_height};
    measure_and_arrange_element((ui_element *)shell->window_root, &window_root_rect, page_name);
}

void app_page_shell_unregister_all(app_page_shell *shell, const char *page_name)
{
    validate_page_name(page_name);
    if (shell == NULL || shell->context == NULL)
    {
        fail_invalid_shell(page_name, "app_page_shell_unregister_all");
    }

    for (size_t i = shell->registered_count; i > 0U; --i)
    {
        ui_element *element = shell->registered_elements[i - 1U];
        (void)ui_runtime_remove(shell->context, element, true);
    }

    shell->registered_count = 0U;
    shell->window_root = NULL;
}
