#include "ui/ui_image.h"

#include <SDL3_image/SDL_image.h>
#include <stdlib.h>

static const char *MISSING_IMAGE_ASSET_PATH = "assets/missing-image.png";

static void render_image(const ui_element *element, SDL_Renderer *renderer)
{
    const ui_image *image = (const ui_image *)element;
    const SDL_FRect sr = ui_element_screen_rect(element);
    SDL_RenderTexture(renderer, image->texture, NULL, &sr);
    if (image->base.has_border)
    {
        ui_element_render_inner_border(renderer, &sr, image->base.border_color,
                                       image->base.border_width);
    }
}

static void destroy_image(ui_element *element)
{
    ui_image *image = (ui_image *)element;
    if (image->texture != NULL)
    {
        SDL_DestroyTexture(image->texture);
    }
    free(image);
}

static const ui_element_ops IMAGE_OPS = {
    .handle_event = NULL,
    .update = NULL,
    .render = render_image,
    .destroy = destroy_image,
};

ui_image *ui_image_create(SDL_Renderer *renderer, float x, float y, float w, float h,
                          const char *file_path, const SDL_Color *border_color)
{
    if (renderer == NULL)
    {
        return NULL;
    }

    const char *primary_path = file_path != NULL ? file_path : MISSING_IMAGE_ASSET_PATH;
    SDL_Texture *texture = IMG_LoadTexture(renderer, primary_path);
    if (texture == NULL)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "IMG_LoadTexture failed for '%s': %s",
                    primary_path, SDL_GetError());

        if (primary_path != MISSING_IMAGE_ASSET_PATH)
        {
            texture = IMG_LoadTexture(renderer, MISSING_IMAGE_ASSET_PATH);
            if (texture != NULL)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Using fallback image asset '%s' for missing image '%s'",
                            MISSING_IMAGE_ASSET_PATH, primary_path);
            }
        }
    }

    if (texture == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_LoadTexture fallback failed for '%s': %s",
                     MISSING_IMAGE_ASSET_PATH, SDL_GetError());
        return NULL;
    }

    ui_image *image = malloc(sizeof(*image));
    if (image == NULL)
    {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    image->base.rect = (SDL_FRect){x, y, w, h};
    image->base.ops = &IMAGE_OPS;
    image->base.visible = true;
    image->base.enabled = false;
    image->base.parent = NULL;
    image->base.align_h = UI_ALIGN_LEFT;
    image->base.align_v = UI_ALIGN_TOP;
    ui_element_set_border(&image->base, border_color, 1.0F);
    image->texture = texture;

    return image;
}
