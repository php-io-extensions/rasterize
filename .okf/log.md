# Log

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
