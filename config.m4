PHP_ARG_ENABLE([rasterize],
  [whether to enable rasterize support],
  [AS_HELP_STRING([--enable-rasterize], [Enable the rasterize scanning extension])],
  [no])

if test "$PHP_RASTERIZE" != "no"; then
  dnl -ffp-contract=off: no fused multiply-add, so every platform rounds each step the same way.
  PHP_NEW_EXTENSION([rasterize],
    [src/rasterize.c src/core.c src/RasterScanner.c],
    [$ext_shared],, [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -ffp-contract=off])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
fi
