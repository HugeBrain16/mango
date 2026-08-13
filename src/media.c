#include "media.h"
#include "heap.h"
#include <external/spng/spng.h>

image_t *image_png(const char *data, size_t size) {
	image_t *image = heap_alloc(sizeof(image_t));
	if (!image)
		return NULL;

	spng_ctx *ctx = spng_ctx_new(0);
    spng_set_png_buffer(ctx, data, size);

    struct spng_ihdr ihdr = {0};
    spng_get_ihdr(ctx, &ihdr);
    image->width = ihdr.width;
    image->height = ihdr.height;
    spng_decoded_image_size(ctx, SPNG_FMT_RGBA8, &image->size);

    image->data = heap_alloc(image->size);
    if (!image->data) {
    	heap_free(image);
    	spng_ctx_free(ctx);
    	return NULL;
    }
    spng_decode_image(ctx, image->data, image->size, SPNG_FMT_RGBA8, 0);
    spng_ctx_free(ctx);
    return image;
}

void image_free(image_t *image) {
	if (!image) return;

	heap_free(image->data);
	heap_free(image);
}