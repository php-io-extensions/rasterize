/*
 * The stroker: a polyline turned into the contours that, filled non-zero, are
 * its stroke, and a polyline cut into the pieces a dash pattern leaves. Plain
 * C; nothing here knows PHP.
 *
 * outline(): a quad a segment; at each corner the join (a miter, cut to a
 * bevel where its tip would sit more than the miter limit of half-strokes from
 * the corner; a bevel; or a round arc); at the ends of an open polyline the cap
 * (butt, square or round). Every contour is turned to positive signed area, so
 * pieces that overlap unite instead of cancelling; one of no area is dropped.
 *
 * The arithmetic is IEEE double with + - * /, sqrt, floor, ceil, hypot, fmod,
 * atan2, acos, sin and cos, in a fixed order, built with -ffp-contract=off and
 * without the compiler's own sincos: the same input answers the same bits as a
 * PHP implementation of the same steps, on one machine. The trigonometric
 * functions are the C library's, as PHP's are, so another platform's library
 * may differ in the last bit.
 */

#ifndef PHPRASTER_STROKE_H
#define PHPRASTER_STROKE_H

#include <stddef.h>
#include <stdint.h>

#include "core.h"

#define RASTER_CAP_BUTT 0
#define RASTER_CAP_ROUND 1
#define RASTER_CAP_SQUARE 2

#define RASTER_JOIN_MITER 0
#define RASTER_JOIN_ROUND 1
#define RASTER_JOIN_BEVEL 2

#define RASTER_MITER_LIMIT 4.0

/* Why a stroke was not made. */
#define RASTER_STROKE_OK 0
#define RASTER_STROKE_DIVISION 1   /* a length or a width so small that a division by it is by zero */

/* The contours of an outline: x, y, x, y… of all of them one after the other, and where each ends. */
typedef struct {
	double *v;
	size_t length;       /* doubles used */
	size_t capacity;
	size_t *ends;        /* each contour's end, an index into v */
	size_t count;
	size_t ends_capacity;
} raster_outline;

/* One piece of a dashed polyline. */
typedef struct {
	double *v;           /* x, y, x, y… */
	size_t length;
	size_t capacity;
	int closed;
	double dx, dy;       /* the way it runs where it ends */
} raster_piece;

typedef struct {
	raster_piece *pieces;
	size_t count;
	size_t capacity;
} raster_dashes;

/*
 * The stroke of `count` points (x, y pairs, all finite) `stroke` wide, which must be above 0 (else nothing).
 * `direction`, or NULL, is the way a stroke with no length runs, for a square cap: none is along x.
 * Out starts empty.
 */
int raster_stroke_outline(const raster_memory *memory, const double *points, size_t count, double stroke, int closed, int cap, int join, double miter_limit, const double *direction, raster_outline *out);

/*
 * The pieces of a polyline cut by a dash pattern: on, off, on, off… (an odd count is repeated), starting `offset`
 * into it. A piece that runs round a corner keeps the corner. An on element of length 0 leaves a piece of no
 * length. A closed polyline is walked once round; a dash that runs across its start is one piece, and one that
 * never stops is the whole polyline, closed. The lengths are not negative and finite, `offset` is finite.
 * Out starts empty.
 */
int raster_stroke_dash(const raster_memory *memory, const double *points, size_t count, int closed, const double *dash, size_t dash_count, double offset, raster_dashes *out);

void raster_outline_free(const raster_memory *memory, raster_outline *out);
void raster_dashes_free(const raster_memory *memory, raster_dashes *out);

#endif
