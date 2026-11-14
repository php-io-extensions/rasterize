# Log

## 2026-11-13

* `RasterStroker::outline()` and `::dashed()`: a polyline into the contours of its stroke (a quad a segment, miter, round or bevel joins, butt, round or square caps, every contour turned to positive area) and into the pieces a dash pattern leaves. `src/stroke.{h,c}` plain C, `src/RasterStroker.c` the class, seven constants (`RASTER_CAP_*`, `RASTER_JOIN_*`, `RASTER_MITER_LIMIT`). The steps and their order are those of the PHP stroker it was ported from, and it answers the same bits (16,000 random polylines and dash patterns, 241 of them degenerate ones that threw `DivisionByZeroError` in both; NTS, also under guard malloc). `config.m4` gained `-fno-builtin-sin -fno-builtin-cos`: clang joined a `sin` and a `cos` of one angle into `__sincos_stret` and the arcs differed in the last bit. The stroker calls `hypot`, `fmod`, `atan2`, `acos`, `sin` and `cos`: bits equal PHP's on one machine, not promised across C libraries. Version unchanged. [stroker](api/stroker.md)

## 2026-11-08

* **Version**: 0.11.1 in `PHP_RASTERIZE_VERSION` (`php_rasterize.h`) and the version test; nothing else changed.

## 2026-11-07

* `RasterScanner::fills()` and `fill()`: direct fill into ext-fb's `FbBuffer` through its exported span sink (`src/sink.h`, looked up in `module_registry` and with `DL_FETCH_SYMBOL` on ext-fb's handle at MINIT; `ZEND_MOD_OPTIONAL("fb")`). Loads and works without ext-fb. [scanner](api/scanner.md)
* `RasterScanner::batch(array $ops): array` and `RASTER_OP_*` (1..7): many shapes in one call, all checked first; `path()` and batch share `phpraster_contours()`. Answers the single calls' bytes (`tests/ScannerTest.php`); no leak on the refusal paths (`leaks --atExit`). [scanner](api/scanner.md)
* Anti-aliased rows are emitted run by run, not pixel by pixel: `raster_scan_rows()` keeps the columns a row wrote in its coverage and delta arrays, and between two of them, where both are 0, every pixel has the running value, so the stretch is one span (and only those columns are cleared). A large filled shape cost a coverage step for every pixel inside it. Spans unchanged (ScannerTest, Surface's parity and golden frames). Version unchanged.
* Paths, ellipses, rings, rounded rects and rounded rings scan only the rows and columns of the shape's extent (a pixel more each side, cut to the clip; `raster_narrow()` in `core.c`), not every row of the clip with sixteen sample lines each and clip-wide coverage arrays. A small shape in a window-sized clip cost hundreds of µs; the spans are unchanged (outside the extent no sample line has an interval), held by `tests/ScannerTest.php` and Surface's native/extended parity. Version unchanged.

## 2026-11-05

* `RasterScanner::roundRect()`, `roundRing()` and `shadow()` (`raster_round_rect`, `raster_round_ring`, `raster_shadow` in `core.c`), the steps of Surface's `NativeScanner` in its order; malformed radii are a `ValueError`. [scanner](api/scanner.md)
* [build](runbooks/build.md): the gaming PC loop (a tar file redirected in, built from `git ls-files`, `COPYFILE_DISABLE=1` with `--no-xattrs`).

## 2026-10-28

* **Version**: 0.11.0 in `PHP_RASTERIZE_VERSION` (`php_rasterize.h`); `branch-alias` `dev-main` is `0.11.x-dev`. Promoted with the Venusian 0.11 ecosystem. No API or behaviour change. Built and loaded on Homebrew PHP 8.4 NTS and ZTS, the Pi on 8.4 ZTS and an x86_64 8.4 NTS box: 34 passed on each.

## 2026-10-03

* Bundle created with ext-rasterize 0.10.0: [scanner](api/scanner.md) (`RasterScanner`, 5 constants), [build](runbooks/build.md).

## 2026-10-13

* Version 0.10.5: `PHP_RASTERIZE_VERSION` and the version test move from 0.10.0. README rewritten to the php-io-extensions template.
