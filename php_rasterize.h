#ifndef PHP_RASTERIZE_H
#define PHP_RASTERIZE_H

extern zend_module_entry rasterize_module_entry;
#define phpext_rasterize_ptr &rasterize_module_entry

#define PHP_RASTERIZE_VERSION "0.10.0"

#if defined(ZTS) && defined(COMPILE_DL_RASTERIZE)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif
