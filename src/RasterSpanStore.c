#include "runtime.h"
#include "zend_exceptions.h"
#include "../stubs/RasterSpanStore_arginfo.h"
#include "sink.h"

/*
 * RasterSpanStore: span lists scanned once and kept in C, painted many times at whole-pixel offsets through ext-fb's
 * span sink. What a glyph cache keeps: the coverage never leaves C.
 */

zend_class_entry *phpraster_ce_RasterSpanStore;

static zend_object_handlers phpraster_store_handlers;

/* A placement's offset stays within this, so a moved span's edges stay in 64-bit arithmetic with room. */
#define PHPRASTER_STORE_REACH 1073741824

typedef struct {
	uint8_t *bytes;    /* RASTER_SPAN_BYTES a span, inside 0..width x 0..height; NULL for a number not in use */
	uint32_t count;    /* spans */
} phpraster_kept;

typedef struct {
	phpraster_kept *kept;
	uint32_t used, cap;     /* numbers handed out so far, and room */
	uint32_t *spare;        /* numbers removed, to hand out again */
	uint32_t spares;
	uint32_t live;          /* lists kept */
	size_t size;            /* their bytes */
	bool antialias;
	bool constructed;
	zend_object std;
} phpraster_store;

static zend_always_inline phpraster_store *phpraster_store_from(zend_object *obj)
{
	return (phpraster_store *) ((char *) obj - XtOffsetOf(phpraster_store, std));
}

static void *phpraster_store_grow(void *pointer, size_t size)
{
	return erealloc(pointer, size);
}

static void phpraster_store_release(void *pointer)
{
	if (pointer != NULL) {
		efree(pointer);
	}
}

static zend_object *phpraster_store_create(zend_class_entry *ce)
{
	phpraster_store *self = zend_object_alloc(sizeof(phpraster_store), ce);

	zend_object_std_init(&self->std, ce);
	object_properties_init(&self->std, ce);

	return &self->std;
}

static void phpraster_store_free(zend_object *object)
{
	phpraster_store *self = phpraster_store_from(object);

	for (uint32_t i = 0; i < self->used; i++) {
		phpraster_store_release(self->kept[i].bytes);
	}
	phpraster_store_release(self->kept);
	phpraster_store_release(self->spare);
	zend_object_std_dtor(object);
}

void phpraster_register_RasterSpanStore(void)
{
	phpraster_ce_RasterSpanStore = register_class_RasterSpanStore();
	phpraster_ce_RasterSpanStore->create_object = phpraster_store_create;

	memcpy(&phpraster_store_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	phpraster_store_handlers.offset = XtOffsetOf(phpraster_store, std);
	phpraster_store_handlers.free_obj = phpraster_store_free;
	phpraster_store_handlers.clone_obj = NULL;
	phpraster_store_handlers.compare = zend_objects_not_comparable;
	phpraster_ce_RasterSpanStore->default_object_handlers = &phpraster_store_handlers;
}

#define PHPRASTER_STORE_THIS(self) \
	phpraster_store *self = phpraster_store_from(Z_OBJ_P(ZEND_THIS)); \
	if (UNEXPECTED(!self->constructed)) { \
		zend_throw_error(NULL, "RasterSpanStore has not been constructed"); \
		RETURN_THROWS(); \
	}

ZEND_METHOD(RasterSpanStore, __construct)
{
	bool antialias;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_BOOL(antialias)
	ZEND_PARSE_PARAMETERS_END();

	phpraster_store *self = phpraster_store_from(Z_OBJ_P(ZEND_THIS));
	if (self->constructed) {
		zend_throw_error(NULL, "RasterSpanStore is already constructed");
		RETURN_THROWS();
	}
	self->antialias = antialias;
	self->constructed = true;
}

ZEND_METHOD(RasterSpanStore, antialias)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_STORE_THIS(self);

	RETURN_BOOL(self->antialias);
}

ZEND_METHOD(RasterSpanStore, add)
{
	HashTable *list;
	zend_long width, height, rule = RASTER_NON_ZERO;

	ZEND_PARSE_PARAMETERS_START(3, 4)
		Z_PARAM_ARRAY_HT(list)
		Z_PARAM_LONG(width)
		Z_PARAM_LONG(height)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(rule)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_STORE_THIS(self);

	if (width < 1 || height < 1 || width > RASTER_MAX_SIDE || height > RASTER_MAX_SIDE) {
		zend_value_error("A span list is scanned in a box of 1..%d by 1..%d, got " ZEND_LONG_FMT "x" ZEND_LONG_FMT ".", RASTER_MAX_SIDE, RASTER_MAX_SIDE, width, height);
		RETURN_THROWS();
	}
	if (rule != RASTER_NON_ZERO && rule != RASTER_EVEN_ODD) {
		zend_value_error("The fill rule is RASTER_NON_ZERO or RASTER_EVEN_ODD, got " ZEND_LONG_FMT ".", rule);
		RETURN_THROWS();
	}
	if (!zend_array_is_list(list)) {
		zend_value_error("The contours are a list.");
		RETURN_THROWS();
	}

	raster_contour *contours;
	uint32_t made;
	if (!phpraster_contours(list, &contours, &made)) {
		RETURN_THROWS();
	}

	raster_scan scan = {0, 0, (int) width, (int) height, self->antialias, {phpraster_store_grow, phpraster_store_release}};
	raster_spans spans = {NULL, 0, 0};
	raster_path(&scan, contours, made, (int) rule, &spans);
	phpraster_free_contours(contours, made);

	if (spans.length == 0) {
		phpraster_store_release(spans.bytes);
		RETURN_LONG(-1);
	}

	uint32_t id;
	if (self->spares > 0) {
		id = self->spare[--self->spares];
	} else {
		if (self->used == UINT32_MAX - 1) {
			phpraster_store_release(spans.bytes);
			zend_throw_error(NULL, "RasterSpanStore has handed out every number it has.");
			RETURN_THROWS();
		}
		if (self->used == self->cap) {
			self->cap = self->cap == 0 ? 64 : self->cap * 2;
			self->kept = safe_erealloc(self->kept, self->cap, sizeof(phpraster_kept), 0);
		}
		id = self->used++;
	}
	/* the spans are kept at their own length, not the scanner's spare room */
	self->kept[id].bytes = erealloc(spans.bytes, spans.length);
	self->kept[id].count = (uint32_t) (spans.length / RASTER_SPAN_BYTES);
	self->live++;
	self->size += spans.length;

	RETURN_LONG(id);
}

/* The kept list numbered `id`; NULL with a ValueError naming `what` when no list has that number. */
static phpraster_kept *phpraster_store_find(phpraster_store *self, zend_long id, const char *what)
{
	if (id < 0 || (zend_ulong) id >= self->used || self->kept[id].bytes == NULL) {
		zend_value_error("%s " ZEND_LONG_FMT " is not a span list this store keeps.", what, id);
		return NULL;
	}

	return &self->kept[id];
}

ZEND_METHOD(RasterSpanStore, remove)
{
	zend_long id;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(id)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_STORE_THIS(self);

	phpraster_kept *kept = phpraster_store_find(self, id, "Number");
	if (kept == NULL) {
		RETURN_THROWS();
	}
	self->size -= (size_t) kept->count * RASTER_SPAN_BYTES;
	self->live--;
	efree(kept->bytes);
	kept->bytes = NULL;
	kept->count = 0;
	if (self->spares == 0 || (self->spares & (self->spares - 1)) == 0) {
		/* room grows by doubling: at 0, 1, 2, 4… spares */
		self->spare = safe_erealloc(self->spare, self->spares == 0 ? 1 : self->spares * 2, sizeof(uint32_t), 0);
	}
	self->spare[self->spares++] = (uint32_t) id;
}

ZEND_METHOD(RasterSpanStore, count)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_STORE_THIS(self);

	RETURN_LONG(self->live);
}

ZEND_METHOD(RasterSpanStore, size)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_STORE_THIS(self);

	RETURN_LONG((zend_long) self->size);
}

ZEND_METHOD(RasterSpanStore, clear)
{
	ZEND_PARSE_PARAMETERS_NONE();
	PHPRASTER_STORE_THIS(self);

	for (uint32_t i = 0; i < self->used; i++) {
		phpraster_store_release(self->kept[i].bytes);
		self->kept[i].bytes = NULL;
	}
	self->used = 0;
	self->spares = 0;
	self->live = 0;
	self->size = 0;
}

ZEND_METHOD(RasterSpanStore, fill)
{
	HashTable *placements;
	zend_long rgba, cx, cy, cw, ch;
	zend_object *target;

	ZEND_PARSE_PARAMETERS_START(7, 7)
		Z_PARAM_ARRAY_HT(placements)
		Z_PARAM_LONG(rgba)
		Z_PARAM_OBJ(target)
		Z_PARAM_LONG(cx)
		Z_PARAM_LONG(cy)
		Z_PARAM_LONG(cw)
		Z_PARAM_LONG(ch)
	ZEND_PARSE_PARAMETERS_END();
	PHPRASTER_STORE_THIS(self);

	int width, height;
	if (phpraster_sink == NULL) {
		zend_throw_error(NULL, "fill() paints into ext-fb's FbBuffer, and ext-fb is not loaded.");
		RETURN_THROWS();
	}
	if (!phpraster_sink->size(target, &width, &height)) {
		zend_argument_type_error(3, "must be a constructed FbBuffer, %s given", ZSTR_VAL(target->ce->name));
		RETURN_THROWS();
	}
	if (rgba < 0 || rgba > (zend_long) UINT32_MAX) {
		zend_value_error("fill() takes a colour 0xRRGGBBAA, got " ZEND_LONG_FMT ".", rgba);
		RETURN_THROWS();
	}
	if (cw < 1 || ch < 1 || cx < 0 || cy < 0 || cx > width - cw || cy > height - ch) {
		zend_value_error("The clip is empty or not inside the %dx%d framebuffer.", width, height);
		RETURN_THROWS();
	}
	uint32_t n = zend_hash_num_elements(placements);
	if (!zend_array_is_list(placements) || n % 3 != 0) {
		zend_value_error("The placements are a flat list of number, x, y.");
		RETURN_THROWS();
	}

	/* every placement checked, and the room the moved spans take counted, before any is painted */
	zend_long *place = emalloc((n > 0 ? n : 1) * sizeof(zend_long));
	size_t total = 0;
	uint32_t i = 0;
	zval *entry;
	ZEND_HASH_FOREACH_VAL(placements, entry) {
		ZVAL_DEREF(entry);
		if (Z_TYPE_P(entry) != IS_LONG) {
			efree(place);
			zend_value_error("Placement %u holds something that is not a whole number.", i / 3);
			RETURN_THROWS();
		}
		place[i] = Z_LVAL_P(entry);
		if (i % 3 == 0) {
			phpraster_kept *kept = phpraster_store_find(self, place[i], "Placement's number");
			if (kept == NULL) {
				efree(place);
				RETURN_THROWS();
			}
			total += kept->count;
		} else if (place[i] < -PHPRASTER_STORE_REACH || place[i] > PHPRASTER_STORE_REACH) {
			efree(place);
			zend_value_error("Placement %u is moved past +/-2^30.", i / 3);
			RETURN_THROWS();
		}
		i++;
	} ZEND_HASH_FOREACH_END();

	/* each list moved and cut to the clip, into one list for the sink */
	uint8_t *out = emalloc((total > 0 ? total : 1) * RASTER_SPAN_BYTES);
	size_t written = 0;
	const int64_t left = cx, top = cy, right = cx + cw, bottom = cy + ch;
	for (i = 0; i < n; i += 3) {
		const phpraster_kept *kept = &self->kept[place[i]];
		const int64_t dx = place[i + 1], dy = place[i + 2];

		for (uint32_t s = 0; s < kept->count; s++) {
			const uint8_t *p = kept->bytes + (size_t) s * RASTER_SPAN_BYTES;
			int64_t y = dy + (p[0] | (p[1] << 8));
			int64_t x0 = dx + (p[2] | (p[3] << 8));
			int64_t x1 = x0 + (p[4] | (p[5] << 8));

			if (y < top || y >= bottom) {
				continue;
			}
			if (x0 < left) {
				x0 = left;
			}
			if (x1 > right) {
				x1 = right;
			}
			if (x1 <= x0) {
				continue;
			}
			uint8_t *q = out + written * RASTER_SPAN_BYTES;
			q[0] = (uint8_t) (y & 0xFF);
			q[1] = (uint8_t) (y >> 8);
			q[2] = (uint8_t) (x0 & 0xFF);
			q[3] = (uint8_t) (x0 >> 8);
			q[4] = (uint8_t) ((x1 - x0) & 0xFF);
			q[5] = (uint8_t) ((x1 - x0) >> 8);
			q[6] = p[6];
			written++;
		}
	}
	efree(place);

	int bounds[4];
	int painted = written == 0 ? 0 : phpraster_sink->paint_spans(target, out, written, (uint32_t) rgba, bounds);
	efree(out);
	if (painted < 0) {
		/* cannot happen inside a checked clip; the sink has thrown */
		RETURN_THROWS();
	}
	if (painted == 0) {
		RETURN_NULL();
	}
	array_init_size(return_value, 4);
	for (int k = 0; k < 4; k++) {
		add_next_index_long(return_value, bounds[k]);
	}
}
