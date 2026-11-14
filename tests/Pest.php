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

/**
 * A flat x, y… list as a list of [x, y].
 *
 * @return list<array{float, float}>
 */
function pairs(array $flat): array
{
    return array_chunk($flat, 2);
}

/** Twice the signed area of a flat x, y… contour: positive turns the way the stroker keeps. */
function area(array $flat): float
{
    $n = intdiv(count($flat), 2);
    $sum = 0.0;
    for ($i = 0; $i < $n; $i++) {
        $j = ($i + 1) % $n;
        $sum += $flat[2 * $i] * $flat[2 * $j + 1] - $flat[2 * $j] * $flat[2 * $i + 1];
    }

    return $sum;
}
