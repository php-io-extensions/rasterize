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

#endif
