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

/* The points from `start` to the end, turned to run the other way. */
static void turn_back(stroker *s, size_t start)
{
	raster_outline *o = s->out;
	size_t n = (o->length - start) / 2;
	double *w = o->v + start;

	for (size_t i = 0; i < n / 2; i++) {
		size_t k = n - 1 - i;
		double x = w[2 * i], y = w[2 * i + 1];

		w[2 * i] = w[2 * k];
		w[2 * i + 1] = w[2 * k + 1];
		w[2 * k] = x;
		w[2 * k + 1] = y;
	}
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
	if (cap == RASTER_CAP_SQUARE) {
		double ux = direction[0] * half;
		double uy = direction[1] * half;
		double nx = -uy;
		double ny = ux;

		/* the same way round as every outline, so a dot that overlaps another piece unites with it */
		put2(s, x - ux - nx, y - uy - ny);
		put2(s, x + ux - nx, y + uy - ny);
		put2(s, x + ux + nx, y + uy + ny);
		put2(s, x - ux + nx, y - uy + ny);
	} else {
		/* a multiple of four, so the four points straight out are vertices */
		long steps = (long) (ceil((double) arc_steps(s, 2 * M_PI, half) / 4) * 4);

		for (long i = 0; i < steps; i++) {
			double angle = 2 * M_PI * (double) i / (double) steps;

			put2(s, x + half * cos(angle), y + half * sin(angle));
		}
	}
	record(s);
}

/*
 * What one border does at a corner, from the edge coming in to the edge going out. On the outside of the turn that
 * is the join. On the inside the two edges cross: where both segments are long enough to reach the crossing it is
 * that one point, and where one is not, the border runs to the corner itself and out again.
 *
 * A point that lies on the straight line between its neighbours is left out: a miter is its tip alone.
 * `side` is 1.0 for the side of the left normal (-y, x) and -1.0 for the other; `shorter` is the length of the
 * shorter of the two segments.
 */
static void corner(stroker *s, double vx, double vy, const double in[2], const double out[2], double shorter, double side, double half, int join, double miter_limit)
{
	double ax = side * -in[1] * half;
	double ay = side * in[0] * half;
	double bx = side * -out[1] * half;
	double by = side * out[0] * half;
	double cross = in[0] * out[1] - in[1] * out[0];
	double mx, my, squared, reach;

	if (cross == 0.0) {
		/* straight on: one point. Straight back: across the end, from this side of the way in to this side of the way out. */
		put2(s, vx + ax, vy + ay);
		if (!(in[0] * out[0] + in[1] * out[1] > 0)) {
			put2(s, vx + bx, vy + by);
		}
		return;
	}
	/* The turn is toward the left normal when the cross product is positive: that side is the inside. */
	if (side * cross > 0) {
		if (half == 0.0) {
			s->failed = 1;
			return;
		}
		/*
		 * Sum of the two unit normals on this side: its length is 2cos(θ/2), and the two edges' lines
		 * cross 2·half / that length from the corner along it.
		 */
		mx = (ax + bx) / half;
		my = (ay + by) / half;
		squared = mx * mx + my * my;
		if (squared > 0.0) {
			/*
			 * the crossing is half·tan(θ/2) back along each segment: within half of each, so two corners
			 * on one segment never claim the same stretch of it
			 */
			double turn = 1 + in[0] * out[0] + in[1] * out[1];

			if (turn == 0.0) {
				s->failed = 1;
				return;
			}
			if (half * fabs(cross) / turn <= shorter / 2) {
				reach = 2 * half / squared;
				put2(s, vx + mx * reach, vy + my * reach);
				return;
			}
		}
		put2(s, vx + ax, vy + ay);
		put2(s, vx, vy);
		put2(s, vx + bx, vy + by);
		return;
	}

	if (join == RASTER_JOIN_ROUND) {
		double from = atan2(ay, ax);
		double sweep = atan2(by, bx) - from;
		long steps;

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
		return;
	}
	if (join == RASTER_JOIN_BEVEL) {
		put2(s, vx + ax, vy + ay);
		put2(s, vx + bx, vy + by);
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
		put2(s, vx + ax, vy + ay);
		put2(s, vx + bx, vy + by);
		return;
	}
	if (squared == 0.0) {
		s->failed = 1;
		return;
	}
	reach = 2 * half / squared;
	/* the tip alone: the ends of the two edges lie on the lines into it */
	put2(s, vx + mx * reach, vy + my * reach);
}

/*
 * One border of the stroke: the polyline moved `half` to one side, in the order the polyline runs, with what each
 * corner takes on that side.
 */
static void border(stroker *s, const double *p, size_t n, const double *dirs, const double *lengths, size_t segments, int closed, double side, double half, int join, double miter_limit)
{
	const double *last;

	if (closed) {
		for (size_t j = 0; j < n && !s->failed; j++) {
			size_t before = (j + segments - 1) % segments;
			double shorter = lengths[before] < lengths[j] ? lengths[before] : lengths[j];

			corner(s, p[2 * j], p[2 * j + 1], dirs + 2 * before, dirs + 2 * j, shorter, side, half, join, miter_limit);
		}
		return;
	}

	put2(s, p[0] + side * -dirs[1] * half, p[1] + side * dirs[0] * half);
	for (size_t j = 1; j + 1 < n && !s->failed; j++) {
		double shorter = lengths[j - 1] < lengths[j] ? lengths[j - 1] : lengths[j];

		corner(s, p[2 * j], p[2 * j + 1], dirs + 2 * (j - 1), dirs + 2 * j, shorter, side, half, join, miter_limit);
	}
	last = dirs + 2 * (segments - 1);
	put2(s, p[2 * (n - 1)] + side * -last[1] * half, p[2 * (n - 1) + 1] + side * last[0] * half);
}

/*
 * What the outline takes round an end of an open polyline, between its two borders: from the border on the right
 * of the outward direction to the one on its left. The two border points themselves are the borders'. A butt cap
 * is the straight line between them: nothing. `outward` is the unit direction pointing away from the polyline.
 */
static void cap_at(stroker *s, double x, double y, const double outward[2], double half, int cap)
{
	/* the normal on the left of the outward direction */
	double nx, ny;

	if (cap == RASTER_CAP_BUTT) {
		return;
	}
	nx = -outward[1] * half;
	ny = outward[0] * half;

	if (cap == RASTER_CAP_SQUARE) {
		double ox = outward[0] * half;
		double oy = outward[1] * half;

		put2(s, x - nx + ox, y - ny + oy);
		put2(s, x + nx + ox, y + ny + oy);
	} else {
		/* a half circle from the right normal, through the outward direction, to the left normal */
		double from = atan2(-ny, -nx);
		/* an even count, so the point straight out from the end is one of the vertices */
		long steps = arc_steps(s, M_PI, half);

		steps += steps % 2;
		for (long i = 1; i < steps; i++) {
			double angle = from + M_PI * (double) i / (double) steps;

			put2(s, x + half * cos(angle), y + half * sin(angle));
		}
	}
}

int raster_stroke_outline(const raster_memory *memory, const double *points, size_t count, double stroke, int closed, int cap, int join, double miter_limit, const double *direction, raster_outline *out)
{
	stroker s = {memory, out, 0};
	double half;
	double *p;
	double *dirs;
	double *lengths;
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
	lengths = memory->grow(NULL, segments * sizeof(double));

	for (size_t i = 0; i < segments; i++) {
		double ax = p[2 * i], ay = p[2 * i + 1];
		double bx = p[2 * ((i + 1) % n)], by = p[2 * ((i + 1) % n) + 1];
		double length = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));

		if (length == 0.0) {
			s.failed = 1;
			break;
		}
		dirs[2 * i] = (bx - ax) / length;
		dirs[2 * i + 1] = (by - ay) / length;
		lengths[i] = length;
	}

	if (!s.failed && closed) {
		/* the two borders, one each way round: what lies between them is filled */
		size_t left;

		border(&s, p, n, dirs, lengths, segments, 1, -1.0, half, join, miter_limit);
		record(&s);
		left = out->length;
		border(&s, p, n, dirs, lengths, segments, 1, 1.0, half, join, miter_limit);
		turn_back(&s, left);
		record(&s);
	} else if (!s.failed) {
		/* out along the right, round the far end, back along the left, round the start */
		double back[2] = {-dirs[0], -dirs[1]};
		size_t left;

		border(&s, p, n, dirs, lengths, segments, 0, -1.0, half, join, miter_limit);
		cap_at(&s, p[2 * (n - 1)], p[2 * (n - 1) + 1], dirs + 2 * (segments - 1), half, cap);
		left = out->length;
		border(&s, p, n, dirs, lengths, segments, 0, 1.0, half, join, miter_limit);
		turn_back(&s, left);
		cap_at(&s, p[0], p[1], back, half, cap);
		record(&s);
	}

	memory->release(lengths);
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
