#ifndef URAST_H
#define URAST_H

#include <stdint.h>

enum urast_topology {
	URAST_TRIANGLE_STRIP
};

struct urast_image {
	uint32_t width, height, stride;
	void *data;
};

struct urast_image *
urast_create_image(uint32_t width, uint32_t height);

void
urast_clear(struct urast_image *image, uint32_t color);

void
urast_render(struct urast_image *image,
	     enum urast_topology topology, const float *vb, uint32_t count);

void urast_free_image(struct urast_image *image);

#endif /* URAST_H */
