#ifndef MEDIA_H
#define MEDIA_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	void *data;
	size_t size;
	uint32_t width;
	uint32_t height;
} image_t;

extern image_t *image_png(const char *data, size_t size);
extern void image_free(image_t *image);

#endif