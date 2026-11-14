#include "stroke.h"

#include <math.h>
#include <string.h>

/* A round join or cap strays from its arc by less than this many pixels. */
#define STROKE_FLATNESS 0.1

typedef struct {
	const raster_memory *memory;
	raster_outline *out;
	int failed;
} stroker;

static void reserve(const raster_memory *memory, void **pointer, size_t *capacity, size_t needed, size_t unit)
{
	size_t grown;

	if (needed <= *capacity) {
		return;
	}
	grown = *capacity == 0 ? 64 : *capacity;
	while (grown < needed) {
		grown *= 2;
	}
	*pointer = memory->grow(*pointer, grown * unit);
	*capacity = grown;
}

static void put(stroker *s, double value)
{
	raster_outline *o = s->out;

	reserve(s->memory, (void **) &o->v, &o->capacity, o->length + 1, sizeof(double));
	o->v[o->length++] = value;
}

static void put2(stroker *s, double x, double y)
{
	put(s, x);
	put(s, y);
}

/* The contour ends here, as it is. */
static void record(stroker *s)
{
	raster_outline *o = s->out;

	reserve(s->memory, (void **) &o->ends, &o->ends_capacity, o->count + 1, sizeof(size_t));
	o->ends[o->count++] = o->length;
}

/* The contour begun at `start` is whole: keep it turned to positive area, or drop it when it has none. */
static void seal(stroker *s, size_t start)
{
	raster_outline *o = s->out;
	size_t n = (o->length - start) / 2;
	const double *f = o->v + start;
	double sum = 0.0;

	for (size_t i = 0; i < n; i++) {
		size_t j = (i + 1) % n;

		sum += f[2 * i] * f[2 * j + 1] - f[2 * j] * f[2 * i + 1];
	}
	if (sum > 0) {
		/* as it is */
	} else if (sum < 0) {
		double *w = o->v + start;

		for (size_t i = 0; i < n / 2; i++) {
			size_t k = n - 1 - i;
			double x = w[2 * i], y = w[2 * i + 1];

			w[2 * i] = w[2 * k];
			w[2 * i + 1] = w[2 * k + 1];
			w[2 * k] = x;
			w[2 * k + 1] = y;
		}
	} else {
		o->length = start;
		return;
	}
	record(s);
}

/* How many straight pieces an arc of `sweep` radians at `radius` takes to stray less than the flatness: 2 at least. */
static long arc_steps(stroker *s, double sweep, double radius)
{
	double step, pieces;
	long n;

	if (radius <= STROKE_FLATNESS) {
		return 2;
	}
	step = 2 * acos(1 - STROKE_FLATNESS / radius);
	if (step == 0.0) {
		s->failed = 1;
		return 2;
	}
	pieces = ceil(sweep / step);
	n = isfinite(pieces) && fabs(pieces) < 9.0e18 ? (long) pieces : 0;
	return n > 2 ? n : 2;
}

/* A zero-length stroke: a circle the width across (round), or a square the width on a side turned to the direction (square). */
static void dot(stroker *s, double x, double y, double half, int cap, const double direction[2])
{
	size_t start = s->out->length;

	if (cap == RASTER_CAP_SQUARE) {
		double ux = direction[0] * half;
		double uy = direction[1] * half;
		double nx = -uy;
		double ny = ux;

		put2(s, x - ux + nx, y - uy + ny);
		put2(s, x + ux + nx, y + uy + ny);
		put2(s, x + ux - nx, y + uy - ny);
		put2(s, x - ux - nx, y - uy - ny);
	} else {
		/* a multiple of four, so the four points straight out are vertices */
		long steps = (long) (ceil((double) arc_steps(s, 2 * M_PI, half) / 4) * 4);

		for (long i = 0; i < steps; i++) {
			double angle = 2 * M_PI * (double) i / (double) steps;

			put2(s, x + half * cos(angle), y + half * sin(angle));
		}
	}
	/* a dot is kept as it is made, whichever way it turns */
	(void) start;
	record(s);
}

/* The piece that fills the outside of a corner; nothing where the polyline runs straight on or straight back. */
static void corner(stroker *s, double vx, double vy, const double in[2], const double out[2], double half, int join, double miter_limit)
{
	double cross = in[0] * out[1] - in[1] * out[0];
	double side, ax, ay, bx, by, mx, my, squared, reach;
	size_t start;

	if (cross == 0.0) {
		return;
	}

	/* The outside of the turn is the side away from it. */
	side = cross > 0 ? -1.0 : 1.0;
	ax = side * -in[1] * half;
	ay = side * in[0] * half;
	bx = side * -out[1] * half;
	by = side * out[0] * half;
	start = s->out->length;

	if (join == RASTER_JOIN_ROUND) {
		double from = atan2(ay, ax);
		double sweep = atan2(by, bx) - from;
		long steps;

		put2(s, vx, vy);
		put2(s, vx + ax, vy + ay);
		while (sweep > M_PI) {
			sweep -= 2 * M_PI;
		}
		while (sweep < -M_PI) {
			sweep += 2 * M_PI;
		}
		steps = arc_steps(s, fabs(sweep), half);
		for (long i = 1; i < steps; i++) {
			double angle = from + sweep * (double) i / (double) steps;

			put2(s, vx + half * cos(angle), vy + half * sin(angle));
		}
		put2(s, vx + bx, vy + by);
		seal(s, start);
		return;
	}
	if (join == RASTER_JOIN_BEVEL) {
		put2(s, vx, vy);
		put2(s, vx + ax, vy + ay);
		put2(s, vx + bx, vy + by);
		seal(s, start);
		return;
	}

	if (half == 0.0) {
		s->failed = 1;
		return;
	}
	/*
	 * Sum of the two unit outer normals: its length is 2cos(θ/2), and the
	 * miter tip sits 2·half / that length from the corner along it.
	 */
	mx = (ax + bx) / half;
	my = (ay + by) / half;
	squared = mx * mx + my * my;
	if (squared * miter_limit * miter_limit < 4.0) {
		put2(s, vx, vy);
		put2(s, vx + ax, vy + ay);
		put2(s, vx + bx, vy + by);
		seal(s, start);
		return;
	}
	if (squared == 0.0) {
		s->failed = 1;
		return;
	}
	reach = 2 * half / squared;
	put2(s, vx, vy);
	put2(s, vx + ax, vy + ay);
	put2(s, vx + mx * reach, vy + my * reach);
	put2(s, vx + bx, vy + by);
	seal(s, start);
}

/* The cap at an end of an open polyline; `outward` is the unit direction pointing away from the polyline, out of this end. */
static void cap_at(stroker *s, double x, double y, const double outward[2], double half, int cap)
{
	/* the normal on the left of the outward direction */
	double nx = -outward[1] * half;
	double ny = outward[0] * half;
	size_t start = s->out->length;

	if (cap == RASTER_CAP_SQUARE) {
		double ox = outward[0] * half;
		double oy = outward[1] * half;

		put2(s, x + nx, y + ny);
		put2(s, x + nx + ox, y + ny + oy);
		put2(s, x - nx + ox, y - ny + oy);
		put2(s, x - nx, y - ny);
	} else {
		/* a half circle from the left normal, through the outward direction, to the right normal */
		double from = atan2(ny, nx);
		/* an even count, so the point straight out from the end is one of the vertices */
		long steps = arc_steps(s, M_PI, half);

		steps += steps % 2;
		for (long i = 0; i <= steps; i++) {
			double angle = from - M_PI * (double) i / (double) steps;

			put2(s, x + half * cos(angle), y + half * sin(angle));
		}
	}
	seal(s, start);
}

int raster_stroke_outline(const raster_memory *memory, const double *points, size_t count, double stroke, int closed, int cap, int join, double miter_limit, const double *direction, raster_outline *out)
{
	stroker s = {memory, out, 0};
	double half;
	double *p;
	double *dirs;
	size_t n = 0, segments;

	memset(out, 0, sizeof(*out));
	if (!(stroke > 0)) {
		return RASTER_STROKE_OK;
	}
	half = stroke / 2;

	/* the points with each one that repeats the one before dropped */
	p = memory->grow(NULL, (count > 0 ? count : 1) * 2 * sizeof(double));
	for (size_t i = 0; i < count; i++) {
		double x = points[2 * i], y = points[2 * i + 1];

		if (n == 0 || !(p[2 * (n - 1)] == x && p[2 * (n - 1) + 1] == y)) {
			p[2 * n] = x;
			p[2 * n + 1] = y;
			n++;
		}
	}
	if (closed && n > 2 && p[0] == p[2 * (n - 1)] && p[1] == p[2 * (n - 1) + 1]) {
		n--;
	}
	if (n < 2) {
		/* a stroke that was drawn but has no length is a dot under a round cap and a square under a square one (SVG 2); none under a butt cap */
		if (n == 1 && count >= 2 && cap != RASTER_CAP_BUTT) {
			static const double along_x[2] = {1.0, 0.0};

			dot(&s, p[0], p[1], half, cap, direction != NULL ? direction : along_x);
		}
		memory->release(p);
		if (s.failed) {
			raster_outline_free(memory, out);
		}
		return s.failed ? RASTER_STROKE_DIVISION : RASTER_STROKE_OK;
	}
	closed = closed && n > 2;
	segments = closed ? n : n - 1;
	dirs = memory->grow(NULL, segments * 2 * sizeof(double));

	for (size_t i = 0; i < segments; i++) {
		double ax = p[2 * i], ay = p[2 * i + 1];
		double bx = p[2 * ((i + 1) % n)], by = p[2 * ((i + 1) % n) + 1];
		double length = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
		double ux, uy, nx, ny;
		size_t start = out->length;

		if (length == 0.0) {
			s.failed = 1;
			break;
		}
		ux = (bx - ax) / length;
		uy = (by - ay) / length;
		dirs[2 * i] = ux;
		dirs[2 * i + 1] = uy;
		nx = -uy * half;
		ny = ux * half;
		put2(&s, ax + nx, ay + ny);
		put2(&s, bx + nx, by + ny);
		put2(&s, bx - nx, by - ny);
		put2(&s, ax - nx, ay - ny);
		seal(&s, start);
	}

	if (!s.failed) {
		size_t joins = closed ? segments : segments - 1;

		for (size_t j = 0; j < joins && !s.failed; j++) {
			size_t at = (j + 1) % n, next = (j + 1) % segments;

			corner(&s, p[2 * at], p[2 * at + 1], dirs + 2 * j, dirs + 2 * next, half, join, miter_limit);
		}
	}

	if (!s.failed && !closed && cap != RASTER_CAP_BUTT) {
		double back[2] = {-dirs[0], -dirs[1]};

		cap_at(&s, p[0], p[1], back, half, cap);
		cap_at(&s, p[2 * (n - 1)], p[2 * (n - 1) + 1], dirs + 2 * (segments - 1), half, cap);
	}

	memory->release(dirs);
	memory->release(p);
	if (s.failed) {
		raster_outline_free(memory, out);
		return RASTER_STROKE_DIVISION;
	}
	return RASTER_STROKE_OK;
}

void raster_outline_free(const raster_memory *memory, raster_outline *out)
{
	memory->release(out->v);
	memory->release(out->ends);
	memset(out, 0, sizeof(*out));
}

/* ------------------------------------------------------------------- dashes */

typedef struct {
	const raster_memory *memory;
	raster_dashes *out;
	const double *pattern;
	size_t pattern_count;
	size_t at;
	double left;
	int active;           /* a piece is being made */
	raster_piece piece;
	double x, y;          /* where the walk is */
	double dx, dy;        /* the way it runs */
} dasher;

static void piece_add(const raster_memory *memory, raster_piece *piece, double x, double y)
{
	reserve(memory, (void **) &piece->v, &piece->capacity, piece->length + 2, sizeof(double));
	piece->v[piece->length++] = x;
	piece->v[piece->length++] = y;
}

static void push_piece(dasher *d, const raster_piece *piece)
{
	raster_dashes *o = d->out;

	reserve(d->memory, (void **) &o->pieces, &o->capacity, o->count + 1, sizeof(raster_piece));
	o->pieces[o->count++] = *piece;
}

/* Elements of no length: an on one is a dot. Then, on an on element, a piece begins here. */
static void settle(dasher *d)
{
	while (d->left <= 1e-12) {
		if (d->at % 2 == 0) {
			raster_piece dotted;

			memset(&dotted, 0, sizeof(dotted));
			piece_add(d->memory, &dotted, d->x, d->y);
			piece_add(d->memory, &dotted, d->x, d->y);
			dotted.dx = d->dx;
			dotted.dy = d->dy;
			push_piece(d, &dotted);
		}
		d->at = (d->at + 1) % d->pattern_count;
		d->left = d->pattern[d->at];
	}
	if (d->at % 2 == 0) {
		memset(&d->piece, 0, sizeof(d->piece));
		piece_add(d->memory, &d->piece, d->x, d->y);
		d->piece.dx = d->dx;
		d->piece.dy = d->dy;
		d->active = 1;
	}
}

int raster_stroke_dash(const raster_memory *memory, const double *points, size_t count, int closed, const double *dash, size_t dash_count, double offset, raster_dashes *out)
{
	dasher d;
	double *pattern;
	size_t pattern_count = dash_count % 2 == 1 ? dash_count * 2 : dash_count;
	double total = 0.0, longest = 0.0, phase;
	size_t walk_count = count, start_index;
	int ring, starts_on, turned_off = 0, ends_on;
	int have_direction = 0;
	double direction[2] = {0.0, 0.0};

	memset(out, 0, sizeof(*out));
	if (pattern_count == 0) {
		return RASTER_STROKE_OK;
	}
	pattern = memory->grow(NULL, pattern_count * sizeof(double));
	for (size_t i = 0; i < pattern_count; i++) {
		pattern[i] = dash[i % dash_count];
		total += pattern[i];
		if (pattern[i] > longest) {
			longest = pattern[i];
		}
	}
	/* a pattern with nothing in it longer than the walk can tell from none draws nothing */
	if (total <= 0.0 || longest <= 1e-12) {
		memory->release(pattern);
		return RASTER_STROKE_OK;
	}

	ring = closed && count > 2;
	if (ring) {
		walk_count++;
	}
#define WALK(i, k) (points[2 * ((i) == count ? 0 : (i)) + (k)])

	for (size_t i = 0; i + 1 < walk_count; i++) {
		double length = hypot(WALK(i + 1, 0) - WALK(i, 0), WALK(i + 1, 1) - WALK(i, 1));

		if (length > 0.0) {
			direction[0] = (WALK(i + 1, 0) - WALK(i, 0)) / length;
			direction[1] = (WALK(i + 1, 1) - WALK(i, 1)) / length;
			have_direction = 1;
			break;
		}
	}
	if (!have_direction) {
		memory->release(pattern);
		return RASTER_STROKE_OK;
	}

	/* the pattern's state at the start: which element, and how much of it is left */
	phase = fmod(offset, total);
	if (phase < 0) {
		phase += total;
	}
	memset(&d, 0, sizeof(d));
	d.memory = memory;
	d.out = out;
	d.pattern = pattern;
	d.pattern_count = pattern_count;
	while (phase > pattern[d.at] || (phase == pattern[d.at] && pattern[d.at] > 0.0)) {
		phase -= pattern[d.at];
		d.at = (d.at + 1) % pattern_count;
	}
	d.left = pattern[d.at] - phase;
	d.x = WALK(0, 0);
	d.y = WALK(0, 1);
	d.dx = direction[0];
	d.dy = direction[1];

	settle(&d);
	starts_on = d.active;
	start_index = out->count;

	for (size_t i = 0; i + 1 < walk_count; i++) {
		double ax = WALK(i, 0), ay = WALK(i, 1);
		double bx = WALK(i + 1, 0), by = WALK(i + 1, 1);
		double length = hypot(bx - ax, by - ay);
		double position;

		if (length == 0.0) {
			continue;
		}
		d.dx = (bx - ax) / length;
		d.dy = (by - ay) / length;
		position = 0.0;
		while (position < length) {
			double rest = length - position;
			double take = d.left > rest ? rest : d.left;

			position += take;
			d.left -= take;
			d.x = ax + (bx - ax) * position / length;
			d.y = ay + (by - ay) * position / length;
			if (d.active) {
				piece_add(memory, &d.piece, d.x, d.y);
				d.piece.dx = d.dx;
				d.piece.dy = d.dy;
			}
			if (d.left <= 1e-12) {
				if (d.active) {
					if (d.piece.length / 2 >= 2) {
						push_piece(&d, &d.piece);
					} else {
						memory->release(d.piece.v);
					}
					d.active = 0;
					turned_off = 1;
				}
				d.at = (d.at + 1) % pattern_count;
				d.left = pattern[d.at];
				settle(&d);
			}
		}
	}

	ends_on = d.active && d.piece.length / 2 >= 2;
	if (ring && starts_on && ends_on) {
		if (!turned_off) {
			/* never stopped: the whole polyline, closed */
			raster_piece whole;

			memory->release(d.piece.v);
			memset(&whole, 0, sizeof(whole));
			for (size_t i = 0; i < count; i++) {
				piece_add(memory, &whole, points[2 * i], points[2 * i + 1]);
			}
			whole.closed = 1;
			whole.dx = d.dx;
			whole.dy = d.dy;
			raster_dashes_free(memory, out);
			push_piece(&d, &whole);
			memory->release(pattern);
			return RASTER_STROKE_OK;
		}
		/* the last dash runs on into the first: one piece, joined at the start */
		if (start_index < out->count) {
			raster_piece *first = &out->pieces[start_index];

			for (size_t i = 2; i < first->length; i += 2) {
				piece_add(memory, &d.piece, first->v[i], first->v[i + 1]);
			}
			d.piece.dx = first->dx;
			d.piece.dy = first->dy;
			memory->release(first->v);
			*first = d.piece;
		} else {
			push_piece(&d, &d.piece);
		}
		d.active = 0;
	} else if (ends_on) {
		push_piece(&d, &d.piece);
		d.active = 0;
	}
	if (d.active) {
		memory->release(d.piece.v);
	}
	memory->release(pattern);
	return RASTER_STROKE_OK;
#undef WALK
}

void raster_dashes_free(const raster_memory *memory, raster_dashes *out)
{
	for (size_t i = 0; i < out->count; i++) {
		memory->release(out->pieces[i].v);
	}
	memory->release(out->pieces);
	memset(out, 0, sizeof(*out));
}
