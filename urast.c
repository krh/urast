#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define noHAS_AVX2
#ifdef HAS_AVX2
#include <immintrin.h>
#endif

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

struct box {
	struct vertex v[2];
};

static inline struct box
find_bounding_box(const struct triangle *triangle)
{
	struct box b;

	b.v[0] = triangle->v[0];
	b.v[1] = triangle->v[0];

	for (uint32_t i = 1; i < 3; i++) {
		if (triangle->v[i].x < b.v[0].x)
			b.v[0].x = triangle->v[i].x;
		if (triangle->v[i].y < b.v[0].y)
			b.v[0].y = triangle->v[i].y;
		if (triangle->v[i].x > b.v[1].x)
			b.v[1].x = triangle->v[i].x;
		if (triangle->v[i].y > b.v[1].y)
			b.v[1].y = triangle->v[i].y;
	}

	return b;
}

#define tile_size 8

static int32_t
edge_delta_to_tile_min(struct edge e)
{
	const int32_t sign_x = (uint32_t) e.a >> 31;
	const int32_t sign_y = (uint32_t) e.b >> 31;

	const int tile_max_x = tile_size - 1;
	const int tile_max_y = tile_size - 1;

	/* This is the delta from w in top-left corner to minimum w in tile. */

	return e.a * sign_x * tile_max_x + e.b * sign_y * tile_max_y;
}

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

	struct box bbox = find_bounding_box(triangle);
	const uint32_t start_x = (bbox.v[0].x >> 8) & ~(tile_size - 1);
	const uint32_t start_y = (bbox.v[0].y >> 8) & ~(tile_size - 1);
	const uint32_t end_x = (((bbox.v[1].x + 0xff) >> 8) + (tile_size - 1)) & ~(tile_size - 1);
	const uint32_t end_y = (((bbox.v[1].y + 0xff) >> 8) + (tile_size - 1)) & ~(tile_size - 1);

	int32_t d[3];
	d[0] = edge_delta_to_tile_min(e[0]);
	d[1] = edge_delta_to_tile_min(e[1]);
	d[2] = edge_delta_to_tile_min(e[2]);

	int32_t b[3];
	b[0] = eval_edge(e[0], snap_vertex(start_x, start_y)) + d[0];
	b[1] = eval_edge(e[1], snap_vertex(start_x, start_y)) + d[1];
	b[2] = eval_edge(e[2], snap_vertex(start_x, start_y)) + d[2];

#ifdef HAS_AVX2
	static const int32_t x[8] __attribute__((aligned (32))) = { 0, 1, 2, 3, 4, 5, 6, 7 };
	__m256i offsets[3];
	offsets[0] =
		_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(e[0].a),
						    _mm256_load_si256((void *) x)),
				 _mm256_set1_epi32(-d[0]));
	offsets[1] =
		_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(e[1].a),
						    _mm256_load_si256((void *) x)),
				 _mm256_set1_epi32(-d[1]));
	offsets[2] =
		_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(e[2].a),
						    _mm256_load_si256((void *) x)),
				 _mm256_set1_epi32(-d[2]));
#endif

	uint32_t color = 0xff000080 | (rand() & 0xff);
	for (int32_t y = start_y; y < end_y; y += tile_size) {
		uint32_t *p = image->data + start_x * 4 + y * image->stride;
		for (uint32_t x = start_x; x < end_x; x += tile_size) {
			if ((int32_t) (b[0] & b[1] & b[2]) < 0) {
#ifdef HAS_AVX2
				__m256i tb[3], c, mask;
				void *tp = p;
				tb[0] = _mm256_add_epi32(offsets[0], _mm256_set1_epi32(b[0]));
				tb[1] = _mm256_add_epi32(offsets[1], _mm256_set1_epi32(b[1]));
				tb[2] = _mm256_add_epi32(offsets[2], _mm256_set1_epi32(b[2]));
				c = _mm256_set1_epi32(color);

				for (uint32_t ty = 0; ty < tile_size; ty++) {
					mask = _mm256_and_si256(_mm256_and_si256(tb[0], tb[1]), tb[2]);
					_mm256_maskstore_epi32(tp, mask, c);
					tp += image->stride;
					tb[0] = _mm256_add_epi32(tb[0], _mm256_set1_epi32(e[0].b));
					tb[1] = _mm256_add_epi32(tb[1], _mm256_set1_epi32(e[1].b));
					tb[2] = _mm256_add_epi32(tb[2], _mm256_set1_epi32(e[2].b));
				}
#else
				int32_t tb[3];
				tb[0] = b[0] - d[0];
				tb[1] = b[1] - d[1];
				tb[2] = b[2] - d[2];
				for (uint32_t ty = 0; ty < tile_size; ty++) {
					for (uint32_t tx = 0; tx < tile_size; tx++) {
						if ((int32_t) (tb[0] & tb[1] & tb[2]) < 0)
							*p = color;
						p++;
						tb[0] += e[0].a;
						tb[1] += e[1].a;
						tb[2] += e[2].a;
					}
					p = (void *) (p - tile_size) + image->stride;
					tb[0] += e[0].b - e[0].a * tile_size;
					tb[1] += e[1].b - e[1].a * tile_size;
					tb[2] += e[2].b - e[2].a * tile_size;
				}

				p = (void *) p - tile_size * image->stride;
#endif
			}

			p += tile_size;
			b[0] += tile_size * e[0].a;
			b[1] += tile_size * e[1].a;
			b[2] += tile_size * e[2].a;
		}

		uint32_t span_width = end_x - start_x;
		b[0] += tile_size * e[0].b - e[0].a * span_width;
		b[1] += tile_size * e[1].b - e[1].a * span_width;
		b[2] += tile_size * e[2].b - e[2].a * span_width;
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
