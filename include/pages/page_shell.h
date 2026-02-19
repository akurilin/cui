#ifndef PAGE_SHELL_H
#define PAGE_SHELL_H

#include "system/ui_runtime.h"
#include "ui/ui_window.h"

#include <stddef.h>

#define APP_PAGE_SHELL_MAX_REGISTERED_ELEMENTS 64U

/*
 * Shared page runtime shell used by concrete app pages.
 *
 * Purpose:
 * - Standardize the page root-window model across all pages.
 * - Track elements registered into ui_runtime for reliable teardown.
 *
 * Behavior/contract:
 * - Every page initializes this shell once during create.
 * - The shell always owns a `ui_window` root that is registered in runtime.
 * - Additional page-owned top-level elements can be registered via this shell.
 */
typedef struct app_page_shell
{
    ui_runtime *context;
    ui_window *window_root;
    ui_element *registered_elements[APP_PAGE_SHELL_MAX_REGISTERED_ELEMENTS];
    size_t registered_count;
} app_page_shell;

/*
 * Initialize one page shell and create/register its required root window.
 *
 * Parameters:
 * - `shell`: destination shell storage.
 * - `context`: destination runtime that owns registered elements.
 * - `viewport_width`: initial root width in logical pixels (> 0).
 * - `viewport_height`: initial root height in logical pixels (> 0).
 * - `page_name`: page identifier used in fail-fast logs.
 */
void app_page_shell_init(app_page_shell *shell, ui_runtime *context, int viewport_width,
                         int viewport_height, const char *page_name);

/*
 * Register one page-owned top-level element in runtime and track it.
 *
 * Parameters:
 * - `shell`: initialized page shell.
 * - `element`: non-NULL element to register.
 * - `page_name`: page identifier used in fail-fast logs.
 */
void app_page_shell_register_element(app_page_shell *shell, ui_element *element,
                                     const char *page_name);

/*
 * Add one child to the required root window.
 *
 * Parameters:
 * - `shell`: initialized page shell.
 * - `child`: unparented child element to attach under the root.
 * - `page_name`: page identifier used in fail-fast logs.
 */
void app_page_shell_add_window_child(app_page_shell *shell, ui_element *child,
                                     const char *page_name);

/*
 * Resize the required root window for viewport changes.
 *
 * Parameters:
 * - `shell`: initialized page shell.
 * - `viewport_width`: new root width in logical pixels (> 0).
 * - `viewport_height`: new root height in logical pixels (> 0).
 * - `page_name`: page identifier used in fail-fast logs.
 */
void app_page_shell_resize_root(app_page_shell *shell, int viewport_width, int viewport_height,
                                const char *page_name);

/*
 * Measure and arrange the page root window for the current viewport.
 *
 * Parameters:
 * - `shell`: initialized page shell.
 * - `viewport_width`: current viewport width in logical pixels (> 0).
 * - `viewport_height`: current viewport height in logical pixels (> 0).
 * - `page_name`: page identifier used in fail-fast logs.
 */
void app_page_shell_arrange_root(app_page_shell *shell, int viewport_width, int viewport_height,
                                 const char *page_name);

/*
 * Unregister and destroy all elements tracked by the shell in reverse add order.
 *
 * Parameters:
 * - `shell`: initialized page shell.
 * - `page_name`: page identifier used in fail-fast logs.
 *
 * Ownership/lifecycle:
 * - After this call, tracked runtime registrations are removed.
 * - The shell can be safely discarded.
 */
void app_page_shell_unregister_all(app_page_shell *shell, const char *page_name);

#endif
