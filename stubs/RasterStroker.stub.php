<?php

/** @generate-class-entries */

/**
 * A polyline turned into the contours of its stroke, and cut into the pieces a dash pattern leaves.
 * Points are lists of [x, y] numbers, finite with magnitude at most RASTER_LIMIT; anything else is
 * ValueError. Answers are plain PHP arrays of floats.
 *
 * @strict-properties
 * @not-serializable
 */
final class RasterStroker
{
    private function __construct() {}

    /**
     * The contours that, filled with the non-zero rule, are the stroke of $points $stroke wide: a quad
     * a segment; at each corner the join (a miter, cut to a bevel where its tip would sit more than
     * $miterLimit half-strokes from the corner; a bevel; or a round arc); at the ends of an open
     * polyline the cap (butt, square or round). Every contour is turned to positive signed area, so
     * pieces that overlap unite instead of cancelling; one of no area is dropped. A stroke of 0 or
     * less is nothing.
     *
     * A polyline whose points all repeat, with at least two given, has no length: it is a dot under a
     * round cap and a square under a square one, turned to $direction (a unit [dx, dy]; none is along
     * x), and nothing under a butt cap.
     *
     * @param array $points A list of [x, y].
     * @param array|null $direction [dx, dy]
     * @return array A list of contours, each a flat list x, y, x, y…
     *
     * @throws DivisionByZeroError When a segment is so short that its length is 0, or the stroke so thin that half of it is.
     */
    public static function outline(array $points, float $stroke, bool $closed, int $cap = RASTER_CAP_BUTT, int $join = RASTER_JOIN_MITER, float $miterLimit = RASTER_MITER_LIMIT, ?array $direction = null): array {}

    /**
     * $points cut into the pieces a dash pattern leaves: the pattern is on, off, on, off… (an odd count
     * is repeated), starting $offset into it. A piece that runs round a corner keeps the corner. An on
     * element of length 0 leaves a piece of no length. A closed polyline is walked once round; a dash
     * that runs across its start is one piece, and one that never stops is the whole polyline, closed.
     * A pattern with nothing longer than 1e-12 in it is nothing.
     *
     * @param array $points A list of [x, y].
     * @param array $dash Lengths: finite numbers, 0 or more.
     * @return array A list of ['points' => list of [x, y], 'closed' => bool, 'direction' => [dx, dy]], the way a piece runs where it ends.
     */
    public static function dashed(array $points, bool $closed, array $dash, float $offset): array {}
}
