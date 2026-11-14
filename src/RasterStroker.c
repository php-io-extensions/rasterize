#include "runtime.h"
#include "stroke.h"
#include "zend_exceptions.h"
#include "../stubs/RasterStroker_arginfo.h"

#include <math.h>

zend_class_entry *phpraster_ce_RasterStroker;

void phpraster_register_RasterStroker(void)
{
	phpraster_ce_RasterStroker = register_class_RasterStroker();
}

static void *phpraster_stroke_grow(void *pointer, size_t size)
{
	return erealloc(pointer, size);
}

static void phpraster_stroke_release(void *pointer)
{
	if (pointer != NULL) {
		efree(pointer);
	}
}

static const raster_memory phpraster_stroke_memory = {phpraster_stroke_grow, phpraster_stroke_release};

/* A finite number within RASTER_LIMIT; otherwise a ValueError naming it. */
static bool phpraster_stroke_number(double value, const char *what)
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

/* A number out of an int or a float; false when it is neither. */
static bool phpraster_stroke_read(zval *entry, double *out)
{
	ZVAL_DEREF(entry);
	if (Z_TYPE_P(entry) == IS_LONG) {
		*out = (double) Z_LVAL_P(entry);
		return true;
	}
	if (Z_TYPE_P(entry) == IS_DOUBLE) {
		*out = Z_DVAL_P(entry);
		return true;
	}
	return false;
}

/* [x, y]: two numbers, the first two entries of a list. */
static bool phpraster_stroke_pair(zval *entry, const char *what, double out[2])
{
	zval *x, *y;

	ZVAL_DEREF(entry);
	if (Z_TYPE_P(entry) != IS_ARRAY
		|| (x = zend_hash_index_find(Z_ARRVAL_P(entry), 0)) == NULL || (y = zend_hash_index_find(Z_ARRVAL_P(entry), 1)) == NULL
		|| !phpraster_stroke_read(x, &out[0]) || !phpraster_stroke_read(y, &out[1])) {
		zend_value_error("%s is not [x, y]: two numbers.", what);
		return false;
	}
	return phpraster_stroke_number(out[0], "A coordinate") && phpraster_stroke_number(out[1], "A coordinate");
}

/* A list of [x, y] points into `*out` (emalloc'd, `*count` of them); false with a ValueError, nothing left allocated. */
static bool phpraster_stroke_points(HashTable *list, double **out, size_t *count)
{
	zval *entry;
	size_t i = 0;

	*count = zend_hash_num_elements(list);
	*out = emalloc((*count > 0 ? *count : 1) * 2 * sizeof(double));
	ZEND_HASH_FOREACH_VAL(list, entry) {
		char what[32];

		snprintf(what, sizeof(what), "Point %zu", i);
		if (!phpraster_stroke_pair(entry, what, *out + 2 * i)) {
			efree(*out);
			return false;
		}
		i++;
	} ZEND_HASH_FOREACH_END();

	return true;
}

static void phpraster_stroke_division(void)
{
	zend_throw_exception(zend_ce_division_by_zero_error, "Division by zero", 0);
}

/* Private: the class is its static methods; nothing is made of it. */
PHP_METHOD(RasterStroker, __construct)
{
	(void) return_value;
	ZEND_PARSE_PARAMETERS_NONE();
}

PHP_METHOD(RasterStroker, outline)
{
	HashTable *list, *direction_list = NULL;
	double stroke, miter_limit = RASTER_MITER_LIMIT, *points, direction[2];
	bool closed;
	zend_long cap = RASTER_CAP_BUTT, join = RASTER_JOIN_MITER;
	size_t count;
	raster_outline outline;
	int result;

	ZEND_PARSE_PARAMETERS_START(3, 7)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_DOUBLE(stroke)
		Z_PARAM_BOOL(closed)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(cap)
		Z_PARAM_LONG(join)
		Z_PARAM_DOUBLE(miter_limit)
		Z_PARAM_ARRAY_HT_OR_NULL(direction_list)
	ZEND_PARSE_PARAMETERS_END();

	if (cap < RASTER_CAP_BUTT || cap > RASTER_CAP_SQUARE) {
		zend_argument_value_error(4, "must be RASTER_CAP_BUTT, RASTER_CAP_ROUND or RASTER_CAP_SQUARE");
		RETURN_THROWS();
	}
	if (join < RASTER_JOIN_MITER || join > RASTER_JOIN_BEVEL) {
		zend_argument_value_error(5, "must be RASTER_JOIN_MITER, RASTER_JOIN_ROUND or RASTER_JOIN_BEVEL");
		RETURN_THROWS();
	}
	/* a stroke that is not above 0, a number or not, is nothing */
	if (!(stroke > 0)) {
		RETURN_EMPTY_ARRAY();
	}
	if (!phpraster_stroke_number(stroke, "The stroke")) {
		RETURN_THROWS();
	}
	if (direction_list != NULL) {
		zval pair;

		ZVAL_ARR(&pair, direction_list);
		if (!phpraster_stroke_pair(&pair, "The direction", direction)) {
			RETURN_THROWS();
		}
	}
	if (!phpraster_stroke_points(list, &points, &count)) {
		RETURN_THROWS();
	}

	result = raster_stroke_outline(&phpraster_stroke_memory, points, count, stroke, closed, (int) cap, (int) join, miter_limit, direction_list != NULL ? direction : NULL, &outline);
	efree(points);
	if (result != RASTER_STROKE_OK) {
		phpraster_stroke_division();
		RETURN_THROWS();
	}

	array_init_size(return_value, (uint32_t) outline.count);
	for (size_t c = 0, at = 0; c < outline.count; c++) {
		zval contour;
		size_t end = outline.ends[c];

		array_init_size(&contour, (uint32_t) (end - at));
		for (; at < end; at++) {
			add_next_index_double(&contour, outline.v[at]);
		}
		add_next_index_zval(return_value, &contour);
	}
	raster_outline_free(&phpraster_stroke_memory, &outline);
}

PHP_METHOD(RasterStroker, dashed)
{
	HashTable *list, *dash_list;
	bool closed;
	double offset, *points, *dash;
	size_t count, dash_count = 0, i = 0;
	raster_dashes dashes;
	zval *entry;

	ZEND_PARSE_PARAMETERS_START(4, 4)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_BOOL(closed)
		Z_PARAM_ARRAY_HT(dash_list)
		Z_PARAM_DOUBLE(offset)
	ZEND_PARSE_PARAMETERS_END();

	if (!phpraster_stroke_number(offset, "The offset")) {
		RETURN_THROWS();
	}
	dash_count = zend_hash_num_elements(dash_list);
	dash = emalloc((dash_count > 0 ? dash_count : 1) * sizeof(double));
	ZEND_HASH_FOREACH_VAL(dash_list, entry) {
		if (!phpraster_stroke_read(entry, &dash[i])) {
			efree(dash);
			zend_argument_value_error(3, "holds something that is not a number");
			RETURN_THROWS();
		}
		if (!isfinite(dash[i]) || dash[i] < 0.0) {
			efree(dash);
			zend_argument_value_error(3, "holds a length that is not a finite number, 0 or more");
			RETURN_THROWS();
		}
		i++;
	} ZEND_HASH_FOREACH_END();
	if (!phpraster_stroke_points(list, &points, &count)) {
		efree(dash);
		RETURN_THROWS();
	}

	raster_stroke_dash(&phpraster_stroke_memory, points, count, closed, dash, dash_count, offset, &dashes);
	efree(points);
	efree(dash);

	array_init_size(return_value, (uint32_t) dashes.count);
	for (size_t p = 0; p < dashes.count; p++) {
		const raster_piece *piece = &dashes.pieces[p];
		zval entry_piece, pts, dir, xy;

		array_init_size(&pts, (uint32_t) (piece->length / 2));
		for (size_t k = 0; k < piece->length; k += 2) {
			array_init_size(&xy, 2);
			add_next_index_double(&xy, piece->v[k]);
			add_next_index_double(&xy, piece->v[k + 1]);
			add_next_index_zval(&pts, &xy);
		}
		array_init_size(&dir, 2);
		add_next_index_double(&dir, piece->dx);
		add_next_index_double(&dir, piece->dy);

		array_init_size(&entry_piece, 3);
		add_assoc_zval_ex(&entry_piece, "points", sizeof("points") - 1, &pts);
		add_assoc_bool_ex(&entry_piece, "closed", sizeof("closed") - 1, piece->closed);
		add_assoc_zval_ex(&entry_piece, "direction", sizeof("direction") - 1, &dir);
		add_next_index_zval(return_value, &entry_piece);
	}
	raster_dashes_free(&phpraster_stroke_memory, &dashes);
}
