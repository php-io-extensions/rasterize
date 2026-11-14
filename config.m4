PHP_ARG_ENABLE([rasterize],
  [whether to enable rasterize support],
  [AS_HELP_STRING([--enable-rasterize], [Enable the rasterize scanning extension])],
  [no])

if test "$PHP_RASTERIZE" != "no"; then
  dnl -ffp-contract=off: no fused multiply-add, so every platform rounds each step the same way.
  dnl -fno-builtin-sin -fno-builtin-cos: the compiler otherwise joins a sin and a cos of one angle
  dnl into one sincos call, whose last bit is not always the one sin() and cos() give alone: the
  dnl stroker's arcs would then differ from the same steps in PHP.
  PHP_NEW_EXTENSION([rasterize],
    [src/rasterize.c src/core.c src/RasterScanner.c src/stroke.c src/RasterStroker.c],
    [$ext_shared],, [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -ffp-contract=off -fno-builtin-sin -fno-builtin-cos])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
fi
