/*
 * The scanning core: shapes already lowered to paths, ellipses, rings and
 * one-pixel polylines, answered as span bytes inside a clip. Plain C; nothing
 * here knows PHP.
 *
 * A span is 7 bytes, little-endian: y (uint16), x (uint16), length (uint16),
 * coverage (uint8). Spans come out rows ascending, x ascending, never
 * overlapping, coverage 1..255 (255 only, with hard edges).
 *
 * The arithmetic is IEEE double with + - * /, sqrt, floor and ceil only, in a
 * fixed order, built with -ffp-contract=off: every platform gets the same
 * bits, and so does any other implementation that keeps the same order.
 */

#ifndef PHPRASTER_CORE_H
#define PHPRASTER_CORE_H

#include <stddef.h>
#include <stdint.h>

#define RASTER_NON_ZERO 0
#define RASTER_EVEN_ODD 1
#define RASTER_SPAN_BYTES 7
#define RASTER_SAMPLES 16
#define RASTER_LIMIT 1073741824   /* 2^30: past it, one-pixel line arithmetic would leave 64-bit integers */
#define RASTER_MAX_SIDE 65535

/* The shapes RasterScanner::batch() takes: [op, ...the arguments of the method of that name]. */
#define RASTER_OP_PATH 1
#define RASTER_OP_ELLIPSE 2
#define RASTER_OP_RING 3
#define RASTER_OP_ROUND_RECT 4
#define RASTER_OP_ROUND_RING 5
#define RASTER_OP_SHADOW 6
#define RASTER_OP_POLYLINE 7

typedef struct {
	void *(*grow)(void *pointer, size_t size);   /* realloc that never answers NULL */
	void (*release)(void *pointer);
} raster_memory;

typedef struct {
	int left, top, right, bottom;   /* right and bottom exclusive */
	int antialias;
	raster_memory memory;
} raster_scan;

typedef struct {
	uint8_t *bytes;
	size_t length;
	size_t capacity;
} raster_spans;

typedef struct {
	const double *points;   /* x, y pairs */
	size_t count;           /* pairs */
} raster_contour;

void raster_path(const raster_scan *scan, const raster_contour *contours, size_t contour_count, int rule, raster_spans *out);
void raster_ellipse(const raster_scan *scan, double cx, double cy, double rx, double ry, raster_spans *out);
/* The band between radii r + stroke / 2 and r - stroke / 2. */
void raster_ring(const raster_scan *scan, double cx, double cy, double rx, double ry, double stroke, raster_spans *out);
/* A box [x0, x1) x [y0, y1) with each corner cut by a quarter circle; radii [tl, tr, br, bl], already held to 0..min(w, h)/2. */
void raster_round_rect(const raster_scan *scan, double x0, double y0, double x1, double y1, const double radii[4], raster_spans *out);
/* The band between the box and the box inset by `inset`, whose radii are max(0, r - inset); the whole box when the inset box is empty. */
void raster_round_ring(const raster_scan *scan, double x0, double y0, double x1, double y1, const double radii[4], double inset, raster_spans *out);
/*
 * Coverage 1 - smoothstep(-blur, blur, d) at each pixel centre, d the signed distance to the box rounded by
 * `radius`; hard edges keep coverage >= 128 at 255. A blur of 0 or less is the hard box, in where d < 0.
 */
void raster_shadow(const raster_scan *scan, double x0, double y0, double x1, double y1, double radius, double blur, raster_spans *out);
/* One-pixel lines between the floored points, every pixel once, coverage 255 whatever the edges. */
void raster_polyline(const raster_scan *scan, const double *points, size_t count, int closed, raster_spans *out);

#endif
