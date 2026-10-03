#include "runtime.h"
#include "zend_exceptions.h"
#include "../stubs/RasterScanner_arginfo.h"

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

	uint32_t count = zend_hash_num_elements(list);
	raster_contour *contours = emalloc((count > 0 ? count : 1) * sizeof(raster_contour));
	uint32_t made = 0;
	zval *entry;
	bool ok = true;

	ZEND_HASH_FOREACH_VAL(list, entry) {
		char what[32];
		double *points;
		size_t pairs;

		ZVAL_DEREF(entry);
		snprintf(what, sizeof(what), "Contour %u", made);
		if (!phpraster_flat(entry, what, &points, &pairs)) {
			ok = false;
			break;
		}
		contours[made++] = (raster_contour) {points, pairs};
	} ZEND_HASH_FOREACH_END();

	raster_spans spans = {NULL, 0, 0};
	if (ok) {
		raster_path(&self->scan, contours, made, (int) rule, &spans);
	}
	for (uint32_t i = 0; i < made; i++) {
		efree((void *) contours[i].points);
	}
	efree(contours);
	if (!ok) {
		RETURN_THROWS();
	}

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
