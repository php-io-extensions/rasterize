# Log

## 2026-11-05

* `RasterScanner::roundRect()`, `roundRing()` and `shadow()` (`raster_round_rect`, `raster_round_ring`, `raster_shadow` in `core.c`), the steps of Surface's `NativeScanner` in its order; malformed radii are a `ValueError`. [scanner](api/scanner.md)
* [build](runbooks/build.md): the gaming PC loop (a tar file redirected in, built from `git ls-files`, `COPYFILE_DISABLE=1` with `--no-xattrs`).

## 2026-10-28

* **Version**: 0.11.0 in `PHP_RASTERIZE_VERSION` (`php_rasterize.h`); `branch-alias` `dev-main` is `0.11.x-dev`. Promoted with the Venusian 0.11 ecosystem. No API or behaviour change. Built and loaded on Homebrew PHP 8.4 NTS and ZTS, the Pi on 8.4 ZTS and an x86_64 8.4 NTS box: 34 passed on each.

## 2026-10-03

* Bundle created with ext-rasterize 0.10.0: [scanner](api/scanner.md) (`RasterScanner`, 5 constants), [build](runbooks/build.md).

## 2026-10-13

* Version 0.10.5: `PHP_RASTERIZE_VERSION` and the version test move from 0.10.0. README rewritten to the php-io-extensions template.
