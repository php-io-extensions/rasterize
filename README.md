# ext-rasterize

Shapes into coverage spans in C. `RasterScanner` holds a clip and an edge mode
and answers paths, ellipses, rings and one-pixel polylines as span bytes, ready
for a pixel store to paint. Geometry only: it never touches a pixel. Written
directly in C against the Zend API, with no dependency: it knows nothing of any
framework or pixel store. PHP 8.4+, NTS and ZTS, Linux and macOS.

## Spans

7 bytes each, little-endian: `y` (uint16), `x` (uint16), `length` (uint16),
`coverage` (uint8). Rows ascending, `x` ascending within a row, no two spans of
one answer overlapping, coverage 1..255.

## Edges

Pixel `(x, y)` covers `[x, x + 1) × [y, y + 1)`; integer coordinates are pixel
edges.

* Hard (`antialias` false): each row is sampled at its pixel centres; a pixel is
  in when its centre is inside, a centre on a left or top edge in, on a right
  or bottom edge out. Coverage 255.
* Anti-aliased: each row is sampled on `RASTER_SAMPLES` (16) lines, each
  crossing adds its exact horizontal overlap with each pixel, and the sum
  becomes coverage 1..255.

Paths are one or more closed contours under `RASTER_NON_ZERO` or
`RASTER_EVEN_ODD`, self-intersecting ones included. Ellipses are axis-aligned;
a ring is the band between radii `r ± stroke / 2`. `polyline()` draws
one-pixel lines between the floored points, every pixel once, coverage 255 in
either mode.

The arithmetic is IEEE double with `+ − × ÷`, `sqrt`, `floor` and `ceil` in a
fixed order, built with `-ffp-contract=off`, so the same input answers the same
bytes on every platform.

## Example

```php
$scanner = new RasterScanner(0, 0, 320, 240, true);          // clip, anti-aliased

$disc = $scanner->ellipse(160, 120, 80, 50);
$ring = $scanner->ring(160, 120, 80, 50, 4);
$star = $scanner->path([[160, 20, 190, 110, 280, 110, 205, 160, 235, 230, 160, 185, 85, 230, 115, 160, 40, 110, 130, 110]]);
$edge = $scanner->polyline([0, 0, 319, 239]);

foreach (str_split($disc, RASTER_SPAN_BYTES) as $span) {
    ['y' => $y, 'x' => $x, 'length' => $length, 'coverage' => $coverage] = unpack('vy/vx/vlength/Ccoverage', $span);
}
```

Coordinates are finite with magnitude at most `RASTER_LIMIT` (2³⁰); anything
else, a malformed list, or a clip outside 0..65535 is a `ValueError`. The stubs
in `stubs/` are the full declaration.

## Install

```bash
./install-macos.sh            # Homebrew php@8.4 and php@8.4-zts, or pass PHP binaries
./install-debian-trixie.sh    # Debian trixie, Raspberry Pi OS
```

## Test

```bash
composer install && php vendor/bin/pest
```
