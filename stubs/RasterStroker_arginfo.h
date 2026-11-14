/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 7c445d2a8875c647c21cabdcae8a9800e755b672 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_RasterStroker___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterStroker_outline, 0, 3, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, points, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, stroke, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, closed, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, cap, IS_LONG, 0, "RASTER_CAP_BUTT")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, join, IS_LONG, 0, "RASTER_JOIN_MITER")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, miterLimit, IS_DOUBLE, 0, "RASTER_MITER_LIMIT")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, direction, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterStroker_dashed, 0, 4, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, points, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, closed, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, dash, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, offset, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(RasterStroker, __construct);
ZEND_METHOD(RasterStroker, outline);
ZEND_METHOD(RasterStroker, dashed);

static const zend_function_entry class_RasterStroker_methods[] = {
	ZEND_ME(RasterStroker, __construct, arginfo_class_RasterStroker___construct, ZEND_ACC_PRIVATE)
	ZEND_ME(RasterStroker, outline, arginfo_class_RasterStroker_outline, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(RasterStroker, dashed, arginfo_class_RasterStroker_dashed, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_RasterStroker(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "RasterStroker", class_RasterStroker_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL|ZEND_ACC_NO_DYNAMIC_PROPERTIES|ZEND_ACC_NOT_SERIALIZABLE);

	return class_entry;
}
