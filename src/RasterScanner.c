#include "runtime.h"
#include "zend_exceptions.h"
#include "../stubs/RasterScanner_arginfo.h"
#include "sink.h"

#include <math.h>

zend_class_entry *phpraster_ce_RasterScanner;

static zend_object_handlers phpraster_scanner_handlers;

static void *phpraster_grow(void *pointer, size_t size)
{
	return erealloc(pointer, size);
}

static void phpraster_release(void *pointer)
{
	if (pointer != NULL) {
		efree(pointer);
	}
}

static zend_object *phpraster_scanner_create(zend_class_entry *ce)
{
	phpraster_scanner *self = zend_object_alloc(sizeof(phpraster_scanner), ce);

	zend_object_std_init(&self->std, ce);
	object_properties_init(&self->std, ce);
	self->scan.memory = (raster_memory) {phpraster_grow, phpraster_release};

	return &self->std;
}

void phpraster_register_RasterScanner(void)
{
	phpraster_ce_RasterScanner = register_class_RasterScanner();
	phpraster_ce_RasterScanner->create_object = phpraster_scanner_create;

	memcpy(&phpraster_scanner_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	phpraster_scanner_handlers.offset = XtOffsetOf(phpraster_scanner, std);
	phpraster_scanner_handlers.clone_obj = NULL;
	phpraster_scanner_handlers.compare = zend_objects_not_comparable;
	phpraster_ce_RasterScanner->default_object_handlers = &phpraster_scanner_handlers;
}

#define PHPRASTER_THIS(self) \
	phpraster_scanner *self = phpraster_scanner_from(Z_OBJ_P(ZEND_THIS)); \
	if (UNEXPECTED(!self->constructed)) { \
		zend_throw_error(NULL, "RasterScanner has not been constructed"); \
		RETURN_THROWS(); \
	}

/* A finite number within RASTER_LIMIT; otherwise a ValueError naming it. */
static bool phpraster_number(double value, const char *what)
{
	if (!isfinite(value)) {
		zend_value_error("%s is not a finite number.", what);
		return false;
	}
	if (fabs(value) > (double) RASTER_LIMIT) {
		zend_value_error("%s %.17g is past ±%d.", what, value, RASTER_LIMIT);
		return false;
	}

	return true;
}

/*
 * A flat x, y… list of numbers into `out` (emalloc'd, `*pairs` pairs); false
 * with a ValueError when it is not one. `what` names the list in the message.
 */
static bool phpraster_flat(zval *list, const char *what, double **out, size_t *pairs)
{
	zval *entry;
	size_t i = 0;

	if (Z_TYPE_P(list) != IS_ARRAY || !zend_array_is_list(Z_ARRVAL_P(list)) || zend_hash_num_elements(Z_ARRVAL_P(list)) % 2 != 0) {
		zend_value_error("%s is not a flat list of x, y pairs.", what);
		return false;
	}
	*pairs = zend_hash_num_elements(Z_ARRVAL_P(list)) / 2;
	*out = emalloc((*pairs > 0 ? *pairs : 1) * 2 * sizeof(double));
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(list), entry) {
		ZVAL_DEREF(entry);
		if (Z_TYPE_P(entry) == IS_LONG) {
			(*out)[i] = (double) Z_LVAL_P(entry);
		} else if (Z_TYPE_P(entry) == IS_DOUBLE) {
			(*out)[i] = Z_DVAL_P(entry);
		} else {
			efree(*out);
			zend_value_error("%s holds something that is not a number.", what);
			return false;
		}
		if (!phpraster_number((*out)[i], "A coordinate")) {
			efree(*out);
			return false;
		}
		i++;
	} ZEND_HASH_FOREACH_END();

	return true;
}

static void phpraster_free_contours(raster_contour *contours, uint32_t made)
{
	for (uint32_t i = 0; i < made; i++) {
		efree((void *) contours[i].points);
	}
	efree(contours);
}

/* A list of flat x, y… contours into `*out` (emalloc'd, `*made` of them); false with a ValueError, nothing left allocated. */
static bool phpraster_contours(HashTable *list, raster_contour **out, uint32_t *made)
{
	uint32_t count = zend_hash_num_elements(list);
	zval *entry;

	*out = emalloc((count > 0 ? count : 1) * sizeof(raster_contour));
	*made = 0;
	ZEND_HASH_FOREACH_VAL(list, entry) {
		char what[32];
		double *points;
		size_t pairs;

		ZVAL_DEREF(entry);
		snprintf(what, sizeof(what), "Contour %u", *made);
		if (!phpraster_flat(entry, what, &points, &pairs)) {
			phpraster_free_contours(*out, *made);
			return false;
		}
		(*out)[(*made)++] = (raster_contour) {points, pairs};
	} ZEND_HASH_FOREACH_END();

	return true;
}

static void phpraster_return(zval *return_value, raster_spans *spans)
{
	if (spans->length == 0) {
		phpraster_release(spans->bytes);
		RETURN_EMPTY_STRING();
	}
	zend_string *out = zend_string_init((const char *) spans->bytes, spans->length, 0);
	phpraster_release(spans->bytes);
	RETURN_NEW_STR(out);
}

ZEND_METHOD(RasterScanner, __construct)
{
	zend_long x, y, width, height;
	bool antialias;

	ZEND_PARSE_PARAMETERS_START(5, 5)
		Z_PARAM_LONG(x)
		Z_PARAM_LONG(y)
		Z_PARAM_LONG(width)
		Z_PARAM_LONG(height)
		Z_PARAM_BOOL(antialias)
	ZEND_PARSE_PARAMETERS_END();

	phpraster_scanner *self = phpraster_scanner_from(Z_OBJ_P(ZEND_THIS));
	if (self->constructed) {
		zend_throw_error(NULL, "RasterScanner is already constructed");
		RETURN_THROWS();
	}
	if (x < 0 || y < 0 || width < 1 || height < 1 || width > RASTER_MAX_SIDE - x || height > RASTER_MAX_SIDE - y) {
		zend_value_error("The clip " ZEND_LONG_FMT "x" ZEND_LONG_FMT " at (" ZEND_LONG_FMT ", " ZEND_LONG_FMT ") is empty or not inside 0..65535.", width, height, x, y);
		RETURN_THROWS();
	}

	self->scan.left = (int) x;
	self->scan.top = (int) y;
	self->scan.right = (int) (x + width);
	self->scan.bottom = (int) (y + height);
	self->scan.antialias = antialias;
	self->constructed = true;
}

ZEND_METHOD(RasterScanner, clip)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_THIS(self);

	array_init_size(return_value, 4);
	add_next_index_long(return_value, self->scan.left);
	add_next_index_long(return_value, self->scan.top);
	add_next_index_long(return_value, self->scan.right - self->scan.left);
	add_next_index_long(return_value, self->scan.bottom - self->scan.top);
}

ZEND_METHOD(RasterScanner, antialias)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_THIS(self);

	RETURN_BOOL(self->scan.antialias);
}

ZEND_METHOD(RasterScanner, path)
{
	HashTable *list;
	zend_long rule = RASTER_NON_ZERO;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(rule)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (rule != RASTER_NON_ZERO && rule != RASTER_EVEN_ODD) {
		zend_argument_value_error(2, "must be RASTER_NON_ZERO or RASTER_EVEN_ODD");
		RETURN_THROWS();
	}
	if (!zend_array_is_list(list)) {
		zend_argument_value_error(1, "must be a list of contours");
		RETURN_THROWS();
	}

	raster_contour *contours;
	uint32_t made;
	if (!phpraster_contours(list, &contours, &made)) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_path(&self->scan, contours, made, (int) rule, &spans);
	phpraster_free_contours(contours, made);

	phpraster_return(return_value, &spans);
}

ZEND_METHOD(RasterScanner, ellipse)
{
	double cx, cy, rx, ry;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_DOUBLE(cx)
		Z_PARAM_DOUBLE(cy)
		Z_PARAM_DOUBLE(rx)
		Z_PARAM_DOUBLE(ry)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!phpraster_number(cx, "cx") || !phpraster_number(cy, "cy") || !phpraster_number(rx, "rx") || !phpraster_number(ry, "ry")) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_ellipse(&self->scan, cx, cy, rx, ry, &spans);
	phpraster_return(return_value, &spans);
}

ZEND_METHOD(RasterScanner, ring)
{
	double cx, cy, rx, ry, stroke;

	ZEND_PARSE_PARAMETERS_START(5, 5)
		Z_PARAM_DOUBLE(cx)
		Z_PARAM_DOUBLE(cy)
		Z_PARAM_DOUBLE(rx)
		Z_PARAM_DOUBLE(ry)
		Z_PARAM_DOUBLE(stroke)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!phpraster_number(cx, "cx") || !phpraster_number(cy, "cy") || !phpraster_number(rx, "rx")
			|| !phpraster_number(ry, "ry") || !phpraster_number(stroke, "stroke")) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_ring(&self->scan, cx, cy, rx, ry, stroke, &spans);
	phpraster_return(return_value, &spans);
}

ZEND_METHOD(RasterScanner, polyline)
{
	zval *list;
	bool closed = false;

	ZEND_PARSE_PARAMETERS_START(1, 2)
		Z_PARAM_ARRAY(list)
		Z_PARAM_OPTIONAL
		Z_PARAM_BOOL(closed)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	double *points;
	size_t pairs;
	if (!phpraster_flat(list, "Points", &points, &pairs)) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_polyline(&self->scan, points, pairs, closed, &spans);
	efree(points);
	phpraster_return(return_value, &spans);
}

/* Four numbers, each finite within RASTER_LIMIT; otherwise a ValueError. */
static bool phpraster_radii(HashTable *list, double radii[4])
{
	zval *entry;
	int i = 0;

	if (!zend_array_is_list(list) || zend_hash_num_elements(list) != 4) {
		zend_value_error("Radii are a list of four numbers: [tl, tr, br, bl].");
		return false;
	}
	ZEND_HASH_FOREACH_VAL(list, entry) {
		ZVAL_DEREF(entry);
		if (Z_TYPE_P(entry) == IS_LONG) {
			radii[i] = (double) Z_LVAL_P(entry);
		} else if (Z_TYPE_P(entry) == IS_DOUBLE) {
			radii[i] = Z_DVAL_P(entry);
		} else {
			zend_value_error("Radii are a list of four numbers: [tl, tr, br, bl].");
			return false;
		}
		if (!phpraster_number(radii[i], "A radius")) {
			return false;
		}
		i++;
	} ZEND_HASH_FOREACH_END();

	return true;
}

static bool phpraster_box(double x0, double y0, double x1, double y1)
{
	return phpraster_number(x0, "x0") && phpraster_number(y0, "y0") && phpraster_number(x1, "x1") && phpraster_number(y1, "y1");
}

ZEND_METHOD(RasterScanner, roundRect)
{
	double x0, y0, x1, y1, radii[4];
	HashTable *list;

	ZEND_PARSE_PARAMETERS_START(5, 5)
		Z_PARAM_DOUBLE(x0)
		Z_PARAM_DOUBLE(y0)
		Z_PARAM_DOUBLE(x1)
		Z_PARAM_DOUBLE(y1)
		Z_PARAM_ARRAY_HT(list)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!phpraster_box(x0, y0, x1, y1) || !phpraster_radii(list, radii)) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_round_rect(&self->scan, x0, y0, x1, y1, radii, &spans);
	phpraster_return(return_value, &spans);
}

ZEND_METHOD(RasterScanner, roundRing)
{
	double x0, y0, x1, y1, inset, radii[4];
	HashTable *list;

	ZEND_PARSE_PARAMETERS_START(6, 6)
		Z_PARAM_DOUBLE(x0)
		Z_PARAM_DOUBLE(y0)
		Z_PARAM_DOUBLE(x1)
		Z_PARAM_DOUBLE(y1)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_DOUBLE(inset)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!phpraster_box(x0, y0, x1, y1) || !phpraster_number(inset, "inset") || !phpraster_radii(list, radii)) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_round_ring(&self->scan, x0, y0, x1, y1, radii, inset, &spans);
	phpraster_return(return_value, &spans);
}

ZEND_METHOD(RasterScanner, shadow)
{
	double x0, y0, x1, y1, radius, blur;

	ZEND_PARSE_PARAMETERS_START(6, 6)
		Z_PARAM_DOUBLE(x0)
		Z_PARAM_DOUBLE(y0)
		Z_PARAM_DOUBLE(x1)
		Z_PARAM_DOUBLE(y1)
		Z_PARAM_DOUBLE(radius)
		Z_PARAM_DOUBLE(blur)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!phpraster_box(x0, y0, x1, y1) || !phpraster_number(radius, "radius") || !phpraster_number(blur, "blur")) {
		RETURN_THROWS();
	}

	raster_spans spans = {NULL, 0, 0};
	raster_shadow(&self->scan, x0, y0, x1, y1, radius, blur, &spans);
	phpraster_return(return_value, &spans);
}

/* One op of batch(), read and checked: the numbers, the radii, the contours or points it owns. */
typedef struct {
	zend_long kind;
	double d[6];
	double radii[4];
	raster_contour *contours;
	uint32_t count;
	double *points;
	size_t pairs;
	int flag;                  /* the rule of a path, closed for a polyline */
} phpraster_op;

static void phpraster_op_free(phpraster_op *op)
{
	if (op->contours != NULL) {
		phpraster_free_contours(op->contours, op->count);
	}
	if (op->points != NULL) {
		efree(op->points);
	}
}

/* Entry `at` of an op as a checked number named `what`. */
static bool phpraster_op_number(HashTable *op, uint32_t index, uint32_t at, const char *what, double *out)
{
	zval *entry = zend_hash_index_find_deref(op, at);

	if (Z_TYPE_P(entry) == IS_LONG) {
		*out = (double) Z_LVAL_P(entry);
	} else if (Z_TYPE_P(entry) == IS_DOUBLE) {
		*out = Z_DVAL_P(entry);
	} else {
		zend_value_error("Op %u: %s is not a number.", index, what);
		return false;
	}

	return phpraster_number(*out, what);
}

/* Op `index` read into `out`; false with a ValueError and nothing left allocated. */
static bool phpraster_op_read(zval *zop, uint32_t index, phpraster_op *out)
{
	static const uint32_t sizes[] = {0, 3, 5, 6, 6, 7, 7, 3};
	static const char *names[][6] = {
		{0}, {0},
		{"cx", "cy", "rx", "ry"},
		{"cx", "cy", "rx", "ry", "stroke"},
		{"x0", "y0", "x1", "y1"},
		{"x0", "y0", "x1", "y1"},
		{"x0", "y0", "x1", "y1", "radius", "blur"},
		{0},
	};
	HashTable *op;
	zval *head, *entry;

	memset(out, 0, sizeof(*out));
	ZVAL_DEREF(zop);
	if (Z_TYPE_P(zop) != IS_ARRAY || !zend_array_is_list(Z_ARRVAL_P(zop)) || zend_hash_num_elements(Z_ARRVAL_P(zop)) == 0
			|| Z_TYPE_P(head = zend_hash_index_find_deref(Z_ARRVAL_P(zop), 0)) != IS_LONG
			|| Z_LVAL_P(head) < RASTER_OP_PATH || Z_LVAL_P(head) > RASTER_OP_POLYLINE) {
		zend_value_error("Op %u is not a list that starts with a RASTER_OP_* shape.", index);
		return false;
	}
	op = Z_ARRVAL_P(zop);
	out->kind = Z_LVAL_P(head);
	if (zend_hash_num_elements(op) != sizes[out->kind]) {
		zend_value_error("Op %u takes %u entries, the op and its arguments; it has %u.", index, sizes[out->kind], zend_hash_num_elements(op));
		return false;
	}

	switch (out->kind) {
		case RASTER_OP_PATH:
			entry = zend_hash_index_find_deref(op, 1);
			if (Z_TYPE_P(entry) != IS_ARRAY || !zend_array_is_list(Z_ARRVAL_P(entry))) {
				zend_value_error("Op %u: the contours are not a list.", index);
				return false;
			}
			head = zend_hash_index_find_deref(op, 2);
			if (Z_TYPE_P(head) != IS_LONG || (Z_LVAL_P(head) != RASTER_NON_ZERO && Z_LVAL_P(head) != RASTER_EVEN_ODD)) {
				zend_value_error("Op %u: the rule is not RASTER_NON_ZERO or RASTER_EVEN_ODD.", index);
				return false;
			}
			out->flag = (int) Z_LVAL_P(head);
			return phpraster_contours(Z_ARRVAL_P(entry), &out->contours, &out->count);
		case RASTER_OP_POLYLINE:
			head = zend_hash_index_find_deref(op, 2);
			if (Z_TYPE_P(head) != IS_TRUE && Z_TYPE_P(head) != IS_FALSE) {
				zend_value_error("Op %u: closed is not a bool.", index);
				return false;
			}
			out->flag = Z_TYPE_P(head) == IS_TRUE;
			return phpraster_flat(zend_hash_index_find_deref(op, 1), "Points", &out->points, &out->pairs);
		default: {
			uint32_t at = 1;
			int n = 0;
			for (; n < 6 && names[out->kind][n] != NULL; n++, at++) {
				if (!phpraster_op_number(op, index, at, names[out->kind][n], &out->d[n])) {
					return false;
				}
			}
			if (out->kind == RASTER_OP_ROUND_RECT || out->kind == RASTER_OP_ROUND_RING) {
				entry = zend_hash_index_find_deref(op, 5);
				if (Z_TYPE_P(entry) != IS_ARRAY) {
					zend_value_error("Radii are a list of four numbers: [tl, tr, br, bl].");
					return false;
				}
				if (!phpraster_radii(Z_ARRVAL_P(entry), out->radii)) {
					return false;
				}
				if (out->kind == RASTER_OP_ROUND_RING && !phpraster_op_number(op, index, 6, "inset", &out->d[4])) {
					return false;
				}
			}
			return true;
		}
	}
}

/* An op scanned into `spans`. */
static void phpraster_op_scan(const raster_scan *scan, const phpraster_op *op, raster_spans *spans)
{
	switch (op->kind) {
		case RASTER_OP_PATH: raster_path(scan, op->contours, op->count, op->flag, spans); break;
		case RASTER_OP_ELLIPSE: raster_ellipse(scan, op->d[0], op->d[1], op->d[2], op->d[3], spans); break;
		case RASTER_OP_RING: raster_ring(scan, op->d[0], op->d[1], op->d[2], op->d[3], op->d[4], spans); break;
		case RASTER_OP_ROUND_RECT: raster_round_rect(scan, op->d[0], op->d[1], op->d[2], op->d[3], op->radii, spans); break;
		case RASTER_OP_ROUND_RING: raster_round_ring(scan, op->d[0], op->d[1], op->d[2], op->d[3], op->radii, op->d[4], spans); break;
		case RASTER_OP_SHADOW: raster_shadow(scan, op->d[0], op->d[1], op->d[2], op->d[3], op->d[4], op->d[5], spans); break;
		case RASTER_OP_POLYLINE: raster_polyline(scan, op->points, op->pairs, op->flag, spans); break;
	}
}

/* Every op of a list read and checked into `ops` (room for all); false with a ValueError, `*made` of them to free. */
static bool phpraster_ops_read(HashTable *list, phpraster_op *ops, uint32_t *made)
{
	zval *entry;

	*made = 0;
	ZEND_HASH_FOREACH_VAL(list, entry) {
		if (!phpraster_op_read(entry, *made, &ops[*made])) {
			return false;
		}
		(*made)++;
	} ZEND_HASH_FOREACH_END();

	return true;
}

ZEND_METHOD(RasterScanner, batch)
{
	HashTable *list;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY_HT(list)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	if (!zend_array_is_list(list)) {
		zend_argument_value_error(1, "must be a list of ops");
		RETURN_THROWS();
	}

	uint32_t count = zend_hash_num_elements(list), made = 0;
	phpraster_op *ops = emalloc((count > 0 ? count : 1) * sizeof(phpraster_op));
	bool ok = phpraster_ops_read(list, ops, &made);

	if (ok) {
		array_init_size(return_value, count);
	}
	for (uint32_t i = 0; i < made; i++) {
		phpraster_op *op = &ops[i];
		if (ok) {
			raster_spans spans = {NULL, 0, 0};
			zval out;
			phpraster_op_scan(&self->scan, op, &spans);
			phpraster_return(&out, &spans);
			add_next_index_zval(return_value, &out);
		}
		phpraster_op_free(op);
	}
	efree(ops);
	if (!ok) {
		RETURN_THROWS();
	}
}

ZEND_METHOD(RasterScanner, fills)
{
	ZEND_PARSE_PARAMETERS_NONE();

	RETURN_BOOL(phpraster_sink != NULL);
}

ZEND_METHOD(RasterScanner, fill)
{
	HashTable *list, *colours;
	zend_object *target;

	ZEND_PARSE_PARAMETERS_START(3, 3)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_ARRAY_HT(colours)
		Z_PARAM_OBJ(target)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_THIS(self);

	int width, height;
	if (phpraster_sink == NULL) {
		zend_throw_error(NULL, "fill() paints into ext-fb's FbBuffer, and ext-fb is not loaded.");
		RETURN_THROWS();
	}
	if (!phpraster_sink->size(target, &width, &height)) {
		zend_argument_type_error(3, "must be a constructed FbBuffer, %s given", ZSTR_VAL(target->ce->name));
		RETURN_THROWS();
	}
	if (self->scan.right > width || self->scan.bottom > height) {
		zend_value_error("The clip reaches past the %dx%d framebuffer.", width, height);
		RETURN_THROWS();
	}
	if (!zend_array_is_list(list) || !zend_array_is_list(colours) || zend_hash_num_elements(list) != zend_hash_num_elements(colours)) {
		zend_value_error("fill() takes a list of ops and a list of as many colours.");
		RETURN_THROWS();
	}

	uint32_t count = zend_hash_num_elements(list), made = 0;
	phpraster_op *ops = emalloc((count > 0 ? count : 1) * sizeof(phpraster_op));
	uint32_t *rgba = emalloc((count > 0 ? count : 1) * sizeof(uint32_t));
	uint32_t i = 0;
	zval *entry;
	bool ok = true;

	ZEND_HASH_FOREACH_VAL(colours, entry) {
		ZVAL_DEREF(entry);
		if (Z_TYPE_P(entry) != IS_LONG || Z_LVAL_P(entry) < 0 || Z_LVAL_P(entry) > (zend_long) UINT32_MAX) {
			zend_value_error("Colour %u is not 0xRRGGBBAA.", i);
			ok = false;
			break;
		}
		rgba[i++] = (uint32_t) Z_LVAL_P(entry);
	} ZEND_HASH_FOREACH_END();

	/* every op read and checked before any is painted */
	ok = ok && phpraster_ops_read(list, ops, &made);
	if (ok) {
		array_init_size(return_value, count);
	}
	for (i = 0; i < made; i++) {
		if (ok) {
			raster_spans spans = {NULL, 0, 0};
			int bounds[4];
			int painted;

			phpraster_op_scan(&self->scan, &ops[i], &spans);
			painted = spans.length == 0 ? 0 : phpraster_sink->paint_spans(target, spans.bytes, spans.length / RASTER_SPAN_BYTES, rgba[i], bounds);
			phpraster_release(spans.bytes);
			if (painted < 0) {
				/* cannot happen inside a checked clip; the sink has thrown */
				zval_ptr_dtor(return_value);
				ZVAL_NULL(return_value);
				ok = false;
			} else if (painted == 0) {
				add_next_index_null(return_value);
			} else {
				zval rect;
				array_init_size(&rect, 4);
				for (int k = 0; k < 4; k++) {
					add_next_index_long(&rect, bounds[k]);
				}
				add_next_index_zval(return_value, &rect);
			}
		}
		phpraster_op_free(&ops[i]);
	}
	efree(ops);
	efree(rgba);
	if (!ok) {
		RETURN_THROWS();
	}
}
