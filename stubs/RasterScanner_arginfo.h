/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: b41fcaf19f00cb460accfb119047c733027be2a1 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_RasterScanner___construct, 0, 0, 5)
	ZEND_ARG_TYPE_INFO(0, x, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, y, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, width, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, height, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, antialias, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_clip, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_antialias, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_path, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, contours, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, rule, IS_LONG, 0, "RASTER_NON_ZERO")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_ellipse, 0, 4, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, cx, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, cy, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, rx, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, ry, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_ring, 0, 5, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, cx, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, cy, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, rx, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, ry, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, stroke, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_roundRect, 0, 5, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, x0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, x1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, radii, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_roundRing, 0, 6, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, x0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, x1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, radii, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, inset, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_shadow, 0, 6, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, x0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y0, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, x1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, y1, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, radius, IS_DOUBLE, 0)
	ZEND_ARG_TYPE_INFO(0, blur, IS_DOUBLE, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_polyline, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, points, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, closed, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_batch, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, ops, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_RasterScanner_fills arginfo_class_RasterScanner_antialias

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_RasterScanner_fill, 0, 3, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, ops, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, colours, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, target, IS_OBJECT, 0)
ZEND_END_ARG_INFO()

ZEND_METHOD(RasterScanner, __construct);
ZEND_METHOD(RasterScanner, clip);
ZEND_METHOD(RasterScanner, antialias);
ZEND_METHOD(RasterScanner, path);
ZEND_METHOD(RasterScanner, ellipse);
ZEND_METHOD(RasterScanner, ring);
ZEND_METHOD(RasterScanner, roundRect);
ZEND_METHOD(RasterScanner, roundRing);
ZEND_METHOD(RasterScanner, shadow);
ZEND_METHOD(RasterScanner, polyline);
ZEND_METHOD(RasterScanner, batch);
ZEND_METHOD(RasterScanner, fills);
ZEND_METHOD(RasterScanner, fill);

static const zend_function_entry class_RasterScanner_methods[] = {
	ZEND_ME(RasterScanner, __construct, arginfo_class_RasterScanner___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, clip, arginfo_class_RasterScanner_clip, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, antialias, arginfo_class_RasterScanner_antialias, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, path, arginfo_class_RasterScanner_path, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, ellipse, arginfo_class_RasterScanner_ellipse, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, ring, arginfo_class_RasterScanner_ring, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, roundRect, arginfo_class_RasterScanner_roundRect, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, roundRing, arginfo_class_RasterScanner_roundRing, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, shadow, arginfo_class_RasterScanner_shadow, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, polyline, arginfo_class_RasterScanner_polyline, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, batch, arginfo_class_RasterScanner_batch, ZEND_ACC_PUBLIC)
	ZEND_ME(RasterScanner, fills, arginfo_class_RasterScanner_fills, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	ZEND_ME(RasterScanner, fill, arginfo_class_RasterScanner_fill, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_RasterScanner(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "RasterScanner", class_RasterScanner_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, NULL, ZEND_ACC_FINAL|ZEND_ACC_NOT_SERIALIZABLE);

	return class_entry;
}
