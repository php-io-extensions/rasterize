#include "core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Where a sample line crosses a shape's inside: [a, b) pairs, ascending and disjoint. */
typedef struct {
	double *pairs;
	size_t count;
	size_t capacity;
} raster_intervals;

typedef void (*raster_at)(void *shape, double y, raster_intervals *intervals, const raster_memory *memory);

static void raster_reserve(const raster_memory *memory, void **items, size_t *capacity, size_t needed, size_t size)
{
	if (needed <= *capacity) {
		return;
	}
	size_t grown = *capacity < 16 ? 16 : *capacity;
	while (grown < needed) {
		grown *= 2;
	}
	*items = memory->grow(*items, grown * size);
	*capacity = grown;
}

static void raster_interval(raster_intervals *intervals, const raster_memory *memory, double a, double b)
{
	raster_reserve(memory, (void **) &intervals->pairs, &intervals->capacity, (intervals->count + 1) * 2, sizeof(double));
	intervals->pairs[intervals->count * 2] = a;
	intervals->pairs[intervals->count * 2 + 1] = b;
	intervals->count++;
}

static void raster_emit(const raster_scan *scan, raster_spans *out, int y, int x, int length, int coverage)
{
	raster_reserve(&scan->memory, (void **) &out->bytes, &out->capacity, out->length + RASTER_SPAN_BYTES, 1);
	uint8_t *p = out->bytes + out->length;
	p[0] = (uint8_t) y;
	p[1] = (uint8_t) (y >> 8);
	p[2] = (uint8_t) x;
	p[3] = (uint8_t) (x >> 8);
	p[4] = (uint8_t) length;
	p[5] = (uint8_t) (length >> 8);
	p[6] = (uint8_t) coverage;
	out->length += RASTER_SPAN_BYTES;
}

static double raster_clamp(double value, double low, double high)
{
	return value < low ? low : (value > high ? high : value);
}

/* Rows of the clip into spans, each sample line's intervals asked of `at`. */
static void raster_scan_rows(const raster_scan *scan, raster_at at, void *shape, raster_spans *out)
{
	const raster_memory *memory = &scan->memory;
	raster_intervals intervals = {NULL, 0, 0};
	int left = scan->left;
	int right = scan->right;

	if (!scan->antialias) {
		for (int row = scan->top; row < scan->bottom; row++) {
			int start = -1, end = -1;

			intervals.count = 0;
			at(shape, (double) row + 0.5, &intervals, memory);
			for (size_t i = 0; i < intervals.count; i++) {
				int first = (int) raster_clamp(ceil(intervals.pairs[2 * i] - 0.5), (double) left, (double) right);
				int stop = (int) raster_clamp(ceil(intervals.pairs[2 * i + 1] - 0.5), (double) left, (double) right);

				if (stop <= first) {
					continue;
				}
				if (first == end) {
					end = stop;
					continue;
				}
				if (end > start) {
					raster_emit(scan, out, row, start, end - start, 255);
				}
				start = first;
				end = stop;
			}
			if (end > start) {
				raster_emit(scan, out, row, start, end - start, 255);
			}
		}
		memory->release(intervals.pairs);
		return;
	}

	int width = right - left;
	double weight = 1.0 / RASTER_SAMPLES;
	double *cover = memory->grow(NULL, (size_t) width * sizeof(double));
	double *delta = memory->grow(NULL, ((size_t) width + 1) * sizeof(double));

	for (int i = 0; i < width; i++) {
		cover[i] = 0.0;
	}
	for (int i = 0; i <= width; i++) {
		delta[i] = 0.0;
	}

	for (int row = scan->top; row < scan->bottom; row++) {
		int low = width, high = -1;

		for (int k = 0; k < RASTER_SAMPLES; k++) {
			intervals.count = 0;
			at(shape, (double) row + ((double) k + 0.5) / RASTER_SAMPLES, &intervals, memory);
			for (size_t i = 0; i < intervals.count; i++) {
				double a = intervals.pairs[2 * i];
				double b = intervals.pairs[2 * i + 1];

				if (a < (double) left) {
					a = (double) left;
				}
				if (b > (double) right) {
					b = (double) right;
				}
				if (b <= a) {
					continue;
				}
				int ia = (int) floor(a);
				int ib = (int) floor(b);

				if (ia == ib) {
					cover[ia - left] += (b - a) * weight;
				} else {
					cover[ia - left] += ((double) (ia + 1) - a) * weight;
					if (ia + 1 < ib) {
						delta[ia + 1 - left] += weight;
						delta[ib - left] -= weight;
					}
					if (b > (double) ib) {
						cover[ib - left] += (b - (double) ib) * weight;
					}
				}
				if (ia - left < low) {
					low = ia - left;
				}
				if (ib - left > high) {
					high = ib - left;
				}
			}
		}
		if (high < 0) {
			continue;
		}

		int last = high < width - 1 ? high : width - 1;
		double running = 0.0;
		int start = -1, value = 0;

		for (int i = low; i <= last; i++) {
			running += delta[i];
			double c = cover[i] + running;
			int v = (int) floor(raster_clamp(c, 0.0, 1.0) * 255 + 0.5);

			if (start >= 0 && v == value) {
				continue;
			}
			if (start >= 0 && value > 0) {
				raster_emit(scan, out, row, left + start, i - start, value);
			}
			start = i;
			value = v;
		}
		if (value > 0) {
			raster_emit(scan, out, row, left + start, last + 1 - start, value);
		}
		for (int i = low; i <= high; i++) {
			delta[i] = 0.0;
			if (i < width) {
				cover[i] = 0.0;
			}
		}
	}

	memory->release(cover);
	memory->release(delta);
	memory->release(intervals.pairs);
}

/* Paths --------------------------------------------------------------------------------------- */

typedef struct {
	double ymin, ymax, x0, y0, slope;
	int direction;
	size_t order;
} raster_edge;

typedef struct {
	double x;
	size_t order;
	int direction;
} raster_crossing;

typedef struct {
	raster_edge *edges;
	size_t count;
	size_t next;
	size_t *active;
	size_t active_count;
	raster_crossing *crossings;
	int even_odd;
} raster_path_shape;

static int raster_edge_order(const void *left, const void *right)
{
	const raster_edge *a = left, *b = right;

	if (a->ymin != b->ymin) {
		return a->ymin < b->ymin ? -1 : 1;
	}
	return a->order < b->order ? -1 : (a->order > b->order ? 1 : 0);
}

static int raster_crossing_order(const void *left, const void *right)
{
	const raster_crossing *a = left, *b = right;

	if (a->x != b->x) {
		return a->x < b->x ? -1 : 1;
	}
	return a->order < b->order ? -1 : (a->order > b->order ? 1 : 0);
}

static void raster_path_at(void *shape, double y, raster_intervals *intervals, const raster_memory *memory)
{
	raster_path_shape *p = shape;

	/* Sample lines arrive in increasing y, so edges join once and leave once. */
	while (p->next < p->count && p->edges[p->next].ymin <= y) {
		p->active[p->active_count++] = p->next++;
	}
	size_t kept = 0;
	for (size_t i = 0; i < p->active_count; i++) {
		if (p->edges[p->active[i]].ymax > y) {
			p->active[kept++] = p->active[i];
		}
	}
	p->active_count = kept;

	for (size_t i = 0; i < kept; i++) {
		const raster_edge *e = &p->edges[p->active[i]];

		p->crossings[i] = (raster_crossing) {e->x0 + (y - e->y0) * e->slope, e->order, e->direction};
	}
	qsort(p->crossings, kept, sizeof(raster_crossing), raster_crossing_order);

	long winding = 0;
	double start = 0.0;
	for (size_t i = 0; i < kept; i++) {
		int was = p->even_odd ? (winding & 1) == 1 : winding != 0;
		winding += p->even_odd ? 1 : p->crossings[i].direction;
		int is = p->even_odd ? (winding & 1) == 1 : winding != 0;

		if (!was && is) {
			start = p->crossings[i].x;
		} else if (was && !is && p->crossings[i].x > start) {
			raster_interval(intervals, memory, start, p->crossings[i].x);
		}
	}
}

void raster_path(const raster_scan *scan, const raster_contour *contours, size_t contour_count, int rule, raster_spans *out)
{
	const raster_memory *memory = &scan->memory;
	size_t total = 0;

	for (size_t c = 0; c < contour_count; c++) {
		total += contours[c].count;
	}
	if (total == 0) {
		return;
	}

	raster_path_shape p = {NULL, 0, 0, NULL, 0, NULL, rule == RASTER_EVEN_ODD};
	p.edges = memory->grow(NULL, total * sizeof(raster_edge));
	for (size_t c = 0; c < contour_count; c++) {
		const double *f = contours[c].points;
		size_t n = contours[c].count;

		for (size_t i = 0; i < n; i++) {
			size_t j = (i + 1) % n;
			double x0 = f[2 * i], y0 = f[2 * i + 1], x1 = f[2 * j], y1 = f[2 * j + 1];

			if (y0 == y1) {
				continue;
			}
			p.edges[p.count] = (raster_edge) {
				y0 < y1 ? y0 : y1, y0 < y1 ? y1 : y0, x0, y0, (x1 - x0) / (y1 - y0), y1 > y0 ? 1 : -1, p.count,
			};
			p.count++;
		}
	}
	if (p.count > 0) {
		qsort(p.edges, p.count, sizeof(raster_edge), raster_edge_order);
		p.active = memory->grow(NULL, p.count * sizeof(size_t));
		p.crossings = memory->grow(NULL, p.count * sizeof(raster_crossing));
		raster_scan_rows(scan, raster_path_at, &p, out);
		memory->release(p.active);
		memory->release(p.crossings);
	}
	memory->release(p.edges);
}

/* Ellipses and rings ------------------------------------------------------------------------- */

typedef struct {
	double cx, cy, rx, ry, half;
} raster_ellipse_shape;

/* Where line y crosses the inside of an ellipse: [cx - h, cx + h), h = rx * sqrt(1 - t^2), t = (y - cy) / ry. */
static int raster_across(double cx, double cy, double rx, double ry, double y, double *a, double *b)
{
	double t = (y - cy) / ry;
	double s = 1.0 - t * t;

	if (s <= 0) {
		return 0;
	}
	double h = rx * sqrt(s);
	*a = cx - h;
	*b = cx + h;

	return 1;
}

static void raster_ellipse_at(void *shape, double y, raster_intervals *intervals, const raster_memory *memory)
{
	const raster_ellipse_shape *e = shape;
	double a, b;

	if (raster_across(e->cx, e->cy, e->rx, e->ry, y, &a, &b)) {
		raster_interval(intervals, memory, a, b);
	}
}

static void raster_ring_at(void *shape, double y, raster_intervals *intervals, const raster_memory *memory)
{
	const raster_ellipse_shape *e = shape;
	double oa, ob, ia, ib;

	if (!raster_across(e->cx, e->cy, e->rx + e->half, e->ry + e->half, y, &oa, &ob)) {
		return;
	}
	if (e->rx - e->half <= 0 || e->ry - e->half <= 0 || !raster_across(e->cx, e->cy, e->rx - e->half, e->ry - e->half, y, &ia, &ib)) {
		raster_interval(intervals, memory, oa, ob);
		return;
	}
	raster_interval(intervals, memory, oa, ia);
	raster_interval(intervals, memory, ib, ob);
}

void raster_ellipse(const raster_scan *scan, double cx, double cy, double rx, double ry, raster_spans *out)
{
	raster_ellipse_shape e = {cx, cy, rx, ry, 0.0};

	if (rx <= 0 || ry <= 0) {
		return;
	}
	raster_scan_rows(scan, raster_ellipse_at, &e, out);
}

void raster_ring(const raster_scan *scan, double cx, double cy, double rx, double ry, double stroke, raster_spans *out)
{
	raster_ellipse_shape e = {cx, cy, rx, ry, stroke / 2};

	if (rx <= 0 || ry <= 0 || stroke <= 0) {
		return;
	}
	raster_scan_rows(scan, raster_ring_at, &e, out);
}

/* One-pixel polylines ------------------------------------------------------------------------- */

typedef struct {
	int64_t y, x;
} raster_pixel;

typedef struct {
	raster_pixel *pixels;
	size_t count;
	size_t capacity;
} raster_pixels;

static int raster_pixel_order(const void *left, const void *right)
{
	const raster_pixel *a = left, *b = right;

	if (a->y != b->y) {
		return a->y < b->y ? -1 : 1;
	}
	return a->x < b->x ? -1 : (a->x > b->x ? 1 : 0);
}

/*
 * The pixels of one one-pixel line inside the clip: along the major axis from
 * the lower end, step i has minor offset floor((2|minor|i + |major|) / (2|major|)).
 * Endpoints are within 2^30, so every product stays inside 64 bits.
 */
static void raster_segment(const raster_scan *scan, int64_t x0, int64_t y0, int64_t x1, int64_t y1, raster_pixels *pixels)
{
	int steep = llabs(y1 - y0) > llabs(x1 - x0);
	int64_t a0 = steep ? y0 : x0, b0 = steep ? x0 : y0, a1 = steep ? y1 : x1, b1 = steep ? x1 : y1;

	if (a0 > a1) {
		int64_t t;
		t = a0; a0 = a1; a1 = t;
		t = b0; b0 = b1; b1 = t;
	}
	int64_t low = steep ? scan->top : scan->left, high = steep ? scan->bottom : scan->right;
	int64_t minor_low = steep ? scan->left : scan->top, minor_high = steep ? scan->right : scan->bottom;
	int64_t major = a1 - a0;
	int64_t minor = llabs(b1 - b0);
	int64_t sign = b1 >= b0 ? 1 : -1;
	int64_t last = a1 < high - 1 ? a1 : high - 1;

	for (int64_t a = a0 > low ? a0 : low; a <= last; a++) {
		int64_t b = b0 + sign * (major == 0 ? 0 : (2 * minor * (a - a0) + major) / (2 * major));

		if (b < minor_low || b >= minor_high) {
			continue;
		}
		raster_reserve(&scan->memory, (void **) &pixels->pixels, &pixels->capacity, pixels->count + 1, sizeof(raster_pixel));
		pixels->pixels[pixels->count++] = steep ? (raster_pixel) {a, b} : (raster_pixel) {b, a};
	}
}

void raster_polyline(const raster_scan *scan, const double *points, size_t count, int closed, raster_spans *out)
{
	raster_pixels pixels = {NULL, 0, 0};

	if (count == 0) {
		return;
	}
	if (count == 1) {
		raster_segment(scan, (int64_t) floor(points[0]), (int64_t) floor(points[1]), (int64_t) floor(points[0]), (int64_t) floor(points[1]), &pixels);
	}
	for (size_t i = 0; i + 1 < count; i++) {
		raster_segment(scan, (int64_t) floor(points[2 * i]), (int64_t) floor(points[2 * i + 1]),
			(int64_t) floor(points[2 * i + 2]), (int64_t) floor(points[2 * i + 3]), &pixels);
	}
	if (closed && count > 1) {
		raster_segment(scan, (int64_t) floor(points[2 * count - 2]), (int64_t) floor(points[2 * count - 1]),
			(int64_t) floor(points[0]), (int64_t) floor(points[1]), &pixels);
	}

	qsort(pixels.pixels, pixels.count, sizeof(raster_pixel), raster_pixel_order);
	for (size_t i = 0; i < pixels.count;) {
		size_t j = i + 1;
		int64_t end = pixels.pixels[i].x + 1;

		/* A run: the next pixel is the same one (a shared corner) or the one beside it. */
		while (j < pixels.count && pixels.pixels[j].y == pixels.pixels[i].y && pixels.pixels[j].x <= end) {
			if (pixels.pixels[j].x == end) {
				end++;
			}
			j++;
		}
		raster_emit(scan, out, (int) pixels.pixels[i].y, (int) pixels.pixels[i].x, (int) (end - pixels.pixels[i].x), 255);
		i = j;
	}
	scan->memory.release(pixels.pixels);
}
