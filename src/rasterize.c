/*
 * rasterize: shapes into coverage spans in C. RasterScanner answers the span
 * bytes of paths, ellipses, rings and one-pixel polylines inside a clip.
 */

#include "runtime.h"
#include "ext/standard/info.h"
#include "../stubs/rasterize_arginfo.h"

PHP_MINIT_FUNCTION(rasterize)
{
	(void) type;

	register_rasterize_symbols(module_number);

	phpraster_register_RasterScanner();

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
	php_info_print_table_end();
}

zend_module_entry rasterize_module_entry = {
	STANDARD_MODULE_HEADER,
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
