/*
 * rasterize: shapes into coverage spans in C. RasterScanner answers the span
 * bytes of paths, ellipses, rings and one-pixel polylines inside a clip.
 */

#include "runtime.h"
#include "ext/standard/info.h"
#include "../stubs/rasterize_arginfo.h"
#include "sink.h"

const phpraster_span_sink *phpraster_sink = NULL;

/* ext-fb's span sink through its own handle, when it is loaded: no link to it, and none needed. */
static void phpraster_find_sink(void)
{
	zend_module_entry *fb = zend_hash_str_find_ptr(&module_registry, "fb", sizeof("fb") - 1);
	const phpraster_span_sink *sink = NULL;

	if (fb != NULL && fb->handle != NULL) {
		sink = (const phpraster_span_sink *) DL_FETCH_SYMBOL(fb->handle, PHPRASTER_SINK_SYMBOL);
	}
	phpraster_sink = sink != NULL && sink->version == PHPRASTER_SINK_VERSION ? sink : NULL;
}

PHP_MINIT_FUNCTION(rasterize)
{
	(void) type;

	register_rasterize_symbols(module_number);

	phpraster_register_RasterScanner();
	phpraster_find_sink();

	return SUCCESS;
}

PHP_RINIT_FUNCTION(rasterize)
{
	(void) type;
	(void) module_number;
#if defined(COMPILE_DL_RASTERIZE) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

PHP_MINFO_FUNCTION(rasterize)
{
	(void) zend_module;

	php_info_print_table_start();
	php_info_print_table_row(2, "rasterize support", "enabled");
	php_info_print_table_row(2, "Version", PHP_RASTERIZE_VERSION);
	php_info_print_table_row(2, "Paints into ext-fb", phpraster_sink != NULL ? "yes" : "no (ext-fb not loaded)");
	php_info_print_table_end();
}

/* ext-fb, when there is one, starts first, so its sink is there to find */
static const zend_module_dep rasterize_deps[] = {
	ZEND_MOD_OPTIONAL("fb")
	ZEND_MOD_END
};

zend_module_entry rasterize_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	rasterize_deps,
	"rasterize",
	NULL,
	PHP_MINIT(rasterize),
	NULL,
	PHP_RINIT(rasterize),
	NULL,
	PHP_MINFO(rasterize),
	PHP_RASTERIZE_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_RASTERIZE
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(rasterize)
#endif
