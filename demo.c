#include <stdlib.h>
#include <libpng16/png.h>
#include <math.h>
#include <time.h>

#include "urast.h"

const uint32_t black = 0xff000000;

int main(int argc, char *argv[])
{
	struct urast_image *image = urast_create_image(800, 500);
	int iterations = 1;

	if (argc == 2)
		iterations = atoi(argv[1]);

	urast_clear(image, black);

	const uint32_t count = 64;
	float vb[4 * count], *p;
	float x = 0.0f;
	float y = 100.0f;
	float c, s;

	sincosf(2 * M_PI / 16, &s, &c);
	p = vb;
	for (uint32_t i = 0; i < count; i++) {
		float f = i / 30.0f + 0.1f;
		p[0] = 400.0f + x * f;
		p[1] = 250.0f + y * f;
		p[2] = 400.0f + x * (f + 0.4f);
		p[3] = 250.0f + y * (f + 0.4f);

		float nx = x * c - y * s;
		float ny = x * s + y * c;
		x = nx;
		y = ny;

		p += 4;
	};

	struct timespec begin, end;
	clock_gettime(CLOCK_MONOTONIC_RAW, &begin);

	for (uint32_t i = 0; i < iterations; i++)
		urast_render(image, URAST_TRIANGLE_STRIP, vb, 2 * count);

	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	if (iterations > 1) {
		int64_t begin_ms = begin.tv_sec * 1000 + begin.tv_nsec / 1000000;
		int64_t end_ms = end.tv_sec * 1000 + end.tv_nsec / 1000000;
		printf("elapsed time for %d iterations: %ldms\n", iterations, end_ms - begin_ms);
	}

	png_image pi = {
		.version = PNG_IMAGE_VERSION,
		.width = image->width,
		.height = image->height,
		.format = PNG_FORMAT_RGBA
	};

	if (iterations == 1)
		png_image_write_to_file(&pi, "test.png", 0, image->data, image->stride, NULL);

	urast_free_image(image);

	return 0;
}
