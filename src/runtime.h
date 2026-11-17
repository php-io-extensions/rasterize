/*
 * What the class source shares with the module: the Zend object that holds a
 * scanner's clip and edge mode, and its class entry.
 */

#ifndef PHPRASTER_RUNTIME_H
#define PHPRASTER_RUNTIME_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_rasterize.h"
#include "core.h"
#include "stroke.h"

typedef struct {
	raster_scan scan;
	bool constructed;          /* false until __construct has run */
	zend_object std;
} phpraster_scanner;

static zend_always_inline phpraster_scanner *phpraster_scanner_from(zend_object *obj)
{
	return (phpraster_scanner *) ((char *) obj - XtOffsetOf(phpraster_scanner, std));
}

extern zend_class_entry *phpraster_ce_RasterScanner;

void phpraster_register_RasterScanner(void);

extern zend_class_entry *phpraster_ce_RasterStroker;

void phpraster_register_RasterStroker(void);

extern zend_class_entry *phpraster_ce_RasterSpanStore;

void phpraster_register_RasterSpanStore(void);

/* A list of flat x, y… contours into `*out` (emalloc'd, `*made` of them); false with a ValueError, nothing left allocated. */
bool phpraster_contours(HashTable *list, raster_contour **out, uint32_t *made);

void phpraster_free_contours(raster_contour *contours, uint32_t made);

#endif
