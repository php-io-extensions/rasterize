/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 22ab684db0c2675231f9ca9a837af8862e952db2 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_RasterSpanStore___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, antialias, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_antialias, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_add, 0, 3, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, contours, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, width, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, height, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rule, IS_LONG, 0, "RASTER_NON_ZERO")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_remove, 0, 1, IS_VOID, 0)
	ZEND_ARG_TYPE_INFO(0, id, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_clear, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_RasterSpanStore_size arginfo_class_RasterSpanStore_count

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterSpanStore_fill, 0, 7, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO(0, placements, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, rgba, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, target, IS_OBJECT, 0)
	ZEND_ARG_TYPE_INFO(0, clipX, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, clipY, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, clipWidth, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, clipHeight, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(RasterSpanStore, __construct);
ZEND_METHOD(RasterSpanStore, antialias);
ZEND_METHOD(RasterSpanStore, add);
ZEND_METHOD(RasterSpanStore, remove);
ZEND_METHOD(RasterSpanStore, clear);
ZEND_METHOD(RasterSpanStore, count);
ZEND_METHOD(RasterSpanStore, size);
ZEND_METHOD(RasterSpanStore, fill);

static const zend_function_entry class_RasterSpanStore_methods[] = {
	ZEND_ME(RasterSpanStore, __construct, arginfo_class_RasterSpanStore___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, antialias, arginfo_class_RasterSpanStore_antialias, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, add, arginfo_class_RasterSpanStore_add, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, remove, arginfo_class_RasterSpanStore_remove, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, clear, arginfo_class_RasterSpanStore_clear, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, count, arginfo_class_RasterSpanStore_count, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, size, arginfo_class_RasterSpanStore_size, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterSpanStore, fill, arginfo_class_RasterSpanStore_fill, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_RasterSpanStore(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "RasterSpanStore", class_RasterSpanStore_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL|ZEND_ACC_NOT_SERIALIZABLE);

	return class_entry;
}
