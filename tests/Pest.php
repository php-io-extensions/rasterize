<?php

declare(strict_types=1);

if (! extension_loaded('rasterize')) {
    throw new RuntimeException('The rasterize extension is not loaded; run pest with -d extension=/path/to/rasterize.so');
}

/**
 * Span bytes as [y, x, length, coverage] lists.
 *
 * @return list<array{int, int, int, int}>
 */
function spans(string $bytes): array
{
    expect(strlen($bytes) % RASTER_SPAN_BYTES)->toBe(0);

    return array_map(fn (string $span): array => array_values(unpack('vy/vx/vlength/Ccoverage', $span)), $bytes === '' ? [] : str_split($bytes, RASTER_SPAN_BYTES));
}

/**
 * Span bytes over a clip as rows: '#' / '.' for coverage 255 / none, '?' for anything between.
 *
 * @return list<string>
 */
function grid(string $bytes, int $x, int $y, int $width, int $height): array
{
    $rows = array_fill(0, $height, str_repeat('.', $width));
    foreach (spans($bytes) as [$sy, $sx, $length, $coverage]) {
        for ($i = 0; $i < $length; $i++) {
            $rows[$sy - $y][$sx - $x + $i] = $coverage === 255 ? '#' : '?';
        }
    }

    return $rows;
}
