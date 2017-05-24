#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <assert.h>

#include "urast.h"

static inline bool
is_power_of_two(uint64_t v)
{
	return (v & (v - 1)) == 0;
}

static inline uint64_t
align_u64(uint64_t v, uint64_t a)
{
	assert(is_power_of_two(a));

	return (v + a - 1) & ~(a - 1);
}

struct urast_image *
urast_create_image(uint32_t width, uint32_t height)
{
	struct urast_image *image;
	uint32_t aligned_height;
	uint32_t size;
	uint32_t cpp = 4;

	image = malloc(sizeof(*image));
	if (image == NULL)
		return NULL;

	image->width = width;
	image->height = height;
	image->stride = align_u64(width, 4) * cpp;

	aligned_height = align_u64(height, 2);
        size = aligned_height * image->stride;
	image->data = malloc(size);

	return image;
}

void
urast_clear(struct urast_image *image, uint32_t color)
{
	const uint32_t count = image->stride * image->height / 4;
	uint32_t *p = image->data;
	for (uint32_t i = 0; i < count; i++)
		p[i] = color;
}

void urast_free_image(struct urast_image *image)
{
	free(image->data);
	free(image);
}

struct vertex {
	int32_t x, y;
};

static inline struct vertex
snap_vertex(float x, float y)
{
	return (struct vertex) {
		.x = (int32_t) (x * 256.0f),
		.y = (int32_t) (y * 256.0f)
	};
}

struct edge {
	int32_t a, b, c, bias;
};

static inline struct edge
edge(struct vertex p0, struct vertex p1)
{
	struct edge e;

	e.a = (p0.y - p1.y);
	e.b = (p1.x - p0.x);
	e.c = ((int64_t) p1.y * p0.x - (int64_t) p1.x * p0.y) >> 8;
	e.bias = e.a < 0 || (e.a == 0 && e.b < 0);

	return e;
}

static inline int
eval_edge(struct edge e, struct vertex v)
{
	return (((int64_t) e.a * v.x + (int64_t) e.b * v.y) >> 8) + e.c - e.bias;
}

struct triangle {
	struct vertex v[3];
};

static void
render_triangle(struct urast_image *image, const struct triangle *triangle)
{
	struct edge e[3];
	int32_t area;

	e[0] = edge(triangle->v[0], triangle->v[1]);
	e[1] = edge(triangle->v[1], triangle->v[2]);
	e[2] = edge(triangle->v[2], triangle->v[0]);

	area = eval_edge(e[0], triangle->v[2]) + e[0].bias;
	if (area == 0)
		return;
	if (area > 0) {
		e[0] = edge(triangle->v[1], triangle->v[0]);
		e[1] = edge(triangle->v[2], triangle->v[1]);
		e[2] = edge(triangle->v[0], triangle->v[2]);
	}

	uint32_t color = 0xff000080 | (rand() & 0xff);
	for (int32_t y = 0; y < image->height; y++) {
		int32_t b[3];
		b[0] = eval_edge(e[0], snap_vertex(0.0f, y));
		b[1] = eval_edge(e[1], snap_vertex(0.0f, y));
		b[2] = eval_edge(e[2], snap_vertex(0.0f, y));

		uint32_t *p = image->data + y * image->stride;
		for (int32_t x = 0; x < image->width; x++, p++) {
			if ((int32_t) (b[0] & b[1] & b[2]) < 0)
				*p = color;

			b[0] += e[0].a;
			b[1] += e[1].a;
			b[2] += e[2].a;
		}
	}
}

void
urast_render(struct urast_image *image,
	     enum urast_topology topology, const float *vb, uint32_t count)
{
	const float *p = vb;
	for (uint32_t i = 0; i < count - 2; i++) {
		struct triangle triangle;
		triangle.v[0] = snap_vertex(p[0], p[1]);
		triangle.v[1] = snap_vertex(p[2], p[3]);
		triangle.v[2] = snap_vertex(p[4], p[5]);
		render_triangle(image, &triangle);
		p += 2;
	}
}
