#include "ui/ui_layout_container.h"
#include "ui/ui_pane.h"
#include "ui/ui_scroll_view.h"

#include <stdbool.h>
#include <stdio.h>

static bool test_add_child_rejects_self_cycle(void)
{
    ui_layout_container *container = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 100.0F, 100.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    if (container == NULL)
    {
        return false;
    }

    const bool added = ui_layout_container_add_child(container, &container->base);
    if (added)
    {
        return false;
    }

    const bool ok = container->child_count == 0U;
    container->base.ops->destroy((ui_element *)container);
    return ok;
}

static bool test_add_child_rejects_ancestor_cycle(void)
{
    ui_layout_container *parent = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 200.0F, 200.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    ui_layout_container *child = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 100.0F, 100.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);

    if (parent == NULL || child == NULL)
    {
        return false;
    }

    if (!ui_layout_container_add_child(parent, &child->base))
    {
        return false;
    }

    const bool cycle_added = ui_layout_container_add_child(child, &parent->base);
    if (cycle_added)
    {
        return false;
    }

    const bool ok = child->base.parent == &parent->base;
    parent->base.ops->destroy((ui_element *)parent);
    return ok;
}

static bool test_add_child_rejects_reparenting(void)
{
    ui_layout_container *container_a = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 200.0F, 200.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    ui_layout_container *container_b = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 200.0F, 200.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    ui_pane *child =
        ui_pane_create(&(SDL_FRect){0.0F, 0.0F, 40.0F, 40.0F}, (SDL_Color){20, 20, 20, 255}, NULL);

    if (container_a == NULL || container_b == NULL || child == NULL)
    {
        return false;
    }

    if (!ui_layout_container_add_child(container_a, &child->base))
    {
        return false;
    }

    const bool reparented = ui_layout_container_add_child(container_b, &child->base);
    if (reparented)
    {
        return false;
    }

    const bool ok = child->base.parent == &container_a->base;
    container_b->base.ops->destroy((ui_element *)container_b);
    container_a->base.ops->destroy((ui_element *)container_a);
    return ok;
}

static bool test_scroll_view_create_rejects_parented_child(void)
{
    ui_layout_container *container = ui_layout_container_create(
        &(SDL_FRect){0.0F, 0.0F, 120.0F, 120.0F}, UI_LAYOUT_AXIS_VERTICAL, NULL);
    ui_pane *child =
        ui_pane_create(&(SDL_FRect){0.0F, 0.0F, 40.0F, 20.0F}, (SDL_Color){20, 20, 20, 255}, NULL);

    if (container == NULL || child == NULL)
    {
        return false;
    }

    if (!ui_layout_container_add_child(container, &child->base))
    {
        return false;
    }

    ui_scroll_view *scroll =
        ui_scroll_view_create(&(SDL_FRect){0.0F, 0.0F, 100.0F, 100.0F}, &child->base, 20.0F, NULL);
    if (scroll != NULL)
    {
        return false;
    }

    container->base.ops->destroy((ui_element *)container);
    return true;
}

int main(void)
{
    struct test_case
    {
        const char *name;
        bool (*run)(void);
    };

    static const struct test_case TESTS[] = {
        {"add_child rejects self cycle", test_add_child_rejects_self_cycle},
        {"add_child rejects ancestor cycle", test_add_child_rejects_ancestor_cycle},
        {"add_child rejects reparenting", test_add_child_rejects_reparenting},
        {"scroll_view rejects already parented child",
         test_scroll_view_create_rejects_parented_child},
    };

    size_t passed = 0U;
    const size_t count = sizeof(TESTS) / sizeof(TESTS[0]);

    for (size_t i = 0U; i < count; ++i)
    {
        const bool ok = TESTS[i].run();
        if (ok)
        {
            passed++;
            printf("PASS: %s\n", TESTS[i].name);
        }
        else
        {
            printf("FAIL: %s\n", TESTS[i].name);
        }
    }

    if (passed == count)
    {
        printf("All %zu hierarchy tests passed.\n", count);
        return 0;
    }

    printf("%zu/%zu hierarchy tests passed.\n", passed, count);
    return 1;
}
