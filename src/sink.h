/*
 * The span sink ext-fb exports for painting spans straight into an FbBuffer, found at runtime: ext-rasterize
 * neither links to ext-fb nor needs it loaded. Kept in step with ext-fb's src/sink.h by its version.
 */

#ifndef PHPRASTER_SINK_H
#define PHPRASTER_SINK_H

#include "php.h"

#define PHPRASTER_SINK_SYMBOL "phpfb_span_sink"
#define PHPRASTER_SINK_VERSION 1

typedef struct {
	unsigned version;
	/* An FbBuffer's size: true with *width and *height set, false (nothing thrown) for any other object. */
	bool (*size)(zend_object *buffer, int *width, int *height);
	/*
	 * `count` spans painted at `rgba` as FbBuffer::paintSpans() paints them: 1 with bounds [x, y, width, height]
	 * set, 0 when there were none, -1 with a ValueError thrown and nothing painted.
	 */
	int (*paint_spans)(zend_object *buffer, const uint8_t *spans, size_t count, uint32_t rgba, int bounds[4]);
} phpraster_span_sink;

/* ext-fb's sink, or NULL when ext-fb is not loaded or has none of this version; set once at startup. */
extern const phpraster_span_sink *phpraster_sink;

#endif
