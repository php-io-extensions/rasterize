<?php

declare(strict_types=1);

it('answers a segment as a quad round it, turned to positive area', function (): void {
    $contours = RasterStroker::outline([[0, 0], [10, 0]], 2.0, false);

    // the quad is (0, 1) (10, 1) (10, -1) (0, -1), which turns the wrong way, so it comes back reversed
    expect($contours)->toBe([[0.0, -1.0, 10.0, -1.0, 10.0, 1.0, 0.0, 1.0]])
        ->and(area($contours[0]))->toBe(40.0);
});

it('takes ints and floats alike and answers floats', function (): void {
    expect(RasterStroker::outline([[0, 0], [10, 0]], 2, false))->toBe(RasterStroker::outline([[0.0, 0.0], [10.0, 0.0]], 2.0, false));
});

it('answers nothing for a stroke of 0 or less, or for fewer than two distinct points under a butt cap', function (array $points, float $stroke): void {
    expect(RasterStroker::outline($points, $stroke, false))->toBe([]);
})->with([
    'a stroke of 0' => [[[0, 0], [10, 0]], 0.0],
    'a stroke under 0' => [[[0, 0], [10, 0]], -2.0],
    'a stroke that is not a number' => [[[0, 0], [10, 0]], NAN],
    'no points' => [[], 2.0],
    'one point' => [[[3, 3]], 2.0],
    'one point twice' => [[[3, 3], [3, 3]], 2.0],
]);

it('drops a point that repeats the one before it, and the last of a closed polyline that repeats the first', function (): void {
    $plain = RasterStroker::outline([[0, 0], [10, 0], [10, 10]], 2.0, false);

    expect(RasterStroker::outline([[0, 0], [0, 0], [10, 0], [10, 0], [10, 10]], 2.0, false))->toBe($plain)
        ->and(RasterStroker::outline([[0, 0], [10, 0], [10, 10], [0, 0]], 2.0, true))->toBe(RasterStroker::outline([[0, 0], [10, 0], [10, 10]], 2.0, true));
});

it('fills the outside of a corner with a miter, its tip half a width past the corner\'s diagonal', function (): void {
    // a right angle at (10, 0), half-width 1: the outer normals are (0, -1) and (1, 0), the tip (11, -1)
    $contours = RasterStroker::outline([[0, 0], [10, 0], [10, 10]], 2.0, false);

    expect(count($contours))->toBe(3)
        ->and(pairs($contours[2]))->toBe([[10.0, 0.0], [10.0, -1.0], [11.0, -1.0], [11.0, 0.0]])
        ->and(area($contours[2]))->toBe(2.0);
});

it('cuts a miter to a bevel where its tip would be past the miter limit of half-strokes', function (): void {
    // a turn back on itself at 150 degrees: the tip is 1 / sin(15°) = 3.86 half-widths out
    $points = [[0, 0], [10, 0], [10 - 10 * cos(M_PI / 6), 10 * sin(M_PI / 6)]];

    $mitered = RasterStroker::outline($points, 2.0, false, RASTER_CAP_BUTT, RASTER_JOIN_MITER, 4.0);
    $cut = RasterStroker::outline($points, 2.0, false, RASTER_CAP_BUTT, RASTER_JOIN_MITER, 3.0);

    expect(count($mitered[2]))->toBe(8)
        ->and(count($cut[2]))->toBe(6)
        ->and(RasterStroker::outline($points, 2.0, false, RASTER_CAP_BUTT, RASTER_JOIN_BEVEL)[2])->toBe($cut[2]);
});

it('cuts the corner flat with a bevel, and rounds it with an arc whose pieces keep it within a tenth of a pixel', function (): void {
    $points = [[0, 0], [10, 0], [10, 10]];

    $bevel = RasterStroker::outline($points, 2.0, false, RASTER_CAP_BUTT, RASTER_JOIN_BEVEL);
    $round = RasterStroker::outline($points, 2.0, false, RASTER_CAP_BUTT, RASTER_JOIN_ROUND);

    // the corner, then the end of each outer normal
    expect(pairs($bevel[2]))->toBe([[10.0, 0.0], [10.0, -1.0], [11.0, 0.0]])
        // a quarter turn at radius 1: ceil((π / 2) / (2 acos(1 - 0.1))) = 2 pieces: the corner, both ends and one point between
        ->and(count($round[2]))->toBe(8)
        ->and(pairs($round[2])[0])->toBe([10.0, 0.0])
        ->and(pairs($round[2])[3])->toBe([11.0, -0.0]);
});

it('adds no piece where the polyline runs straight on or straight back', function (array $points): void {
    expect(count(RasterStroker::outline($points, 2.0, false)))->toBe(2);
})->with([
    'straight on' => [[[0, 0], [5, 0], [10, 0]]],
    'straight back' => [[[0, 0], [10, 0], [5, 0]]],
]);

it('closes a polyline with a join where it meets itself, and caps nothing', function (): void {
    $triangle = [[0, 0], [10, 0], [5, 8]];

    // closed: three quads and three joins, whatever the cap; open: two quads and the one join between them
    expect(count(RasterStroker::outline($triangle, 2.0, true)))->toBe(6)
        ->and(count(RasterStroker::outline($triangle, 2.0, true, RASTER_CAP_ROUND)))->toBe(6)
        ->and(count(RasterStroker::outline($triangle, 2.0, false)))->toBe(3);
});

it('caps each end of an open polyline: nothing for a butt, a box for a square, a half circle for a round one', function (): void {
    $line = [[0, 0], [10, 0]];

    $butt = RasterStroker::outline($line, 2.0, false, RASTER_CAP_BUTT);
    $square = RasterStroker::outline($line, 2.0, false, RASTER_CAP_SQUARE);
    $round = RasterStroker::outline($line, 2.0, false, RASTER_CAP_ROUND);

    // a square cap is half the width long: the two boxes and the quad make up 12 by 2
    expect(count($butt))->toBe(1)
        ->and(count($square))->toBe(3)
        ->and(array_sum(array_map('area', $square)))->toBe(2 * (12.0 * 2.0))
        ->and(count($round))->toBe(3)
        // a half circle of radius 1: ceil(π / 0.902) = 4 pieces, 5 points
        ->and(count($round[1]))->toBe(10);
});

it('draws a stroke with no length as a dot under a round cap and a square under a square one, none under a butt cap', function (): void {
    $point = [[3, 4], [3, 4]];

    $round = RasterStroker::outline($point, 2.0, false, RASTER_CAP_ROUND);
    $square = RasterStroker::outline($point, 2.0, false, RASTER_CAP_SQUARE);
    $turned = RasterStroker::outline($point, 2.0, false, RASTER_CAP_SQUARE, direction: [0.0, 1.0]);

    // a full turn at radius 1 is 7 pieces: a multiple of four, so 8, the first straight out at (4, 4)
    expect(RasterStroker::outline($point, 2.0, false, RASTER_CAP_BUTT))->toBe([])
        ->and(count($round))->toBe(1)
        ->and(count($round[0]))->toBe(16)
        ->and(pairs($round[0])[0])->toBe([4.0, 4.0])
        // a 2 by 2 square on (3, 4), as it is made: not turned to positive area
        ->and(pairs($square[0]))->toBe([[2.0, 5.0], [4.0, 5.0], [4.0, 3.0], [2.0, 3.0]])
        // run along y instead of x: the same square, its corners in another order
        ->and(pairs($turned[0]))->toBe([[2.0, 3.0], [2.0, 5.0], [4.0, 5.0], [4.0, 3.0]])
        // one point given once has no length to draw either
        ->and(RasterStroker::outline([[3, 4]], 2.0, false, RASTER_CAP_ROUND))->toBe([]);
});

it('turns every contour to positive area', function (int $seed): void {
    mt_srand($seed);
    $points = array_map(fn (): array => [mt_rand(-500, 5000) / 100, mt_rand(-500, 5000) / 100], range(1, mt_rand(2, 12)));
    foreach ([RASTER_JOIN_MITER, RASTER_JOIN_ROUND, RASTER_JOIN_BEVEL] as $join) {
        foreach ([RASTER_CAP_BUTT, RASTER_CAP_ROUND, RASTER_CAP_SQUARE] as $cap) {
            foreach (RasterStroker::outline($points, mt_rand(1, 80) / 10, (bool) mt_rand(0, 1), $cap, $join) as $contour) {
                expect(area($contour))->toBeGreaterThan(0.0);
            }
        }
    }
})->with(range(1, 25));

it('cuts a polyline by a dash pattern: on, then off', function (): void {
    expect(RasterStroker::dashed([[0, 0], [10, 0]], false, [4, 3], 0.0))->toBe([
        ['points' => [[0.0, 0.0], [4.0, 0.0]], 'closed' => false, 'direction' => [1.0, 0.0]],
        ['points' => [[7.0, 0.0], [10.0, 0.0]], 'closed' => false, 'direction' => [1.0, 0.0]],
    ]);
});

it('starts the pattern $offset into it, also from before it, and repeats an odd count', function (): void {
    $line = [[0, 0], [10, 0]];
    $piece = fn (float $from, float $to): array => ['points' => [[$from, 0.0], [$to, 0.0]], 'closed' => false, 'direction' => [1.0, 0.0]];

    // 4 on, 3 off, 4 on, 3 off... offset 4 is the start of the gap
    expect(RasterStroker::dashed($line, false, [4, 3], 4.0))->toBe([$piece(3.0, 7.0)])
        // -3 is the same place in the pattern as 4: 7 long
        ->and(RasterStroker::dashed($line, false, [4, 3], -3.0))->toBe([$piece(3.0, 7.0)])
        ->and(RasterStroker::dashed($line, false, [4, 3], 7.0))->toBe(RasterStroker::dashed($line, false, [4, 3], 0.0))
        // [5] is [5, 5, 5, 5]... on 0 to 5, off 5 to 10
        ->and(RasterStroker::dashed($line, false, [5], 0.0))->toBe([$piece(0.0, 5.0)])
        // [2, 1, 3] is [2, 1, 3, 2, 1, 3]: on 0-2, off 2-3, on 3-6, off 6-8, on 8-9, off 9-12
        ->and(RasterStroker::dashed($line, false, [2, 1, 3], 0.0))->toBe([$piece(0.0, 2.0), $piece(3.0, 6.0), $piece(8.0, 9.0)]);
});

it('keeps a corner inside a dash that runs round it', function (): void {
    expect(RasterStroker::dashed([[0, 0], [10, 0], [10, 10]], false, [14, 100], 0.0))->toBe([
        ['points' => [[0.0, 0.0], [10.0, 0.0], [10.0, 4.0]], 'closed' => false, 'direction' => [0.0, 1.0]],
    ]);
});

it('leaves a piece of no length for an on element of 0', function (): void {
    $dot = fn (float $x): array => ['points' => [[$x, 0.0], [$x, 0.0]], 'closed' => false, 'direction' => [1.0, 0.0]];

    expect(RasterStroker::dashed([[0, 0], [10, 0]], false, [0, 5], 0.0))->toBe([$dot(0.0), $dot(5.0), $dot(10.0)]);
});

it('walks a closed polyline once round, and joins the dash that runs across its start', function (): void {
    $square = [[0, 0], [10, 0], [10, 10], [0, 10]];

    // on 0 to 30, off 30 to 35, on 35 to 40: the last runs on into the first, and the piece ends where the first did: heading left along the top
    expect(RasterStroker::dashed($square, true, [30, 5], 0.0))->toBe([
        ['points' => [[0.0, 5.0], [0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]], 'closed' => false, 'direction' => [-1.0, 0.0]],
    ])
        // off 0 to 5, on 5 to 40: nothing crosses the start
        ->and(count(RasterStroker::dashed($square, true, [5, 35], 5.0)))->toBe(1);
});

it('answers the whole polyline, closed, for a dash that never stops', function (): void {
    expect(RasterStroker::dashed([[0, 0], [10, 0], [10, 10], [0, 10]], true, [100, 1], 0.0))->toBe([
        ['points' => [[0.0, 0.0], [10.0, 0.0], [10.0, 10.0], [0.0, 10.0]], 'closed' => true, 'direction' => [0.0, -1.0]],
    ]);
});

it('answers nothing for a pattern with no length to draw, or a polyline with none', function (array $points, array $dash): void {
    expect(RasterStroker::dashed($points, false, $dash, 0.0))->toBe([]);
})->with([
    'no pattern' => [[[0, 0], [10, 0]], []],
    'a pattern of zeros' => [[[0, 0], [10, 0]], [0, 0]],
    'a pattern of nothing a walk can tell from none' => [[[0, 0], [10, 0]], [1.0e-13, 1.0e-13]],
    'no points' => [[], [4, 3]],
    'one point' => [[[3, 3]], [4, 3]],
    'one point twice' => [[[3, 3], [3, 3]], [4, 3]],
]);

it('refuses what is not a point, a number or a lawful value, naming it', function (callable $call, string $message): void {
    expect($call)->toThrow(ValueError::class, $message);
})->with([
    'a point that is not a pair' => [fn () => RasterStroker::outline([[0, 0], [1]], 2.0, false), 'Point 1 is not [x, y]'],
    'a point that is a number' => [fn () => RasterStroker::outline([[0, 0], 5], 2.0, false), 'Point 1 is not [x, y]'],
    'a coordinate that is a string' => [fn () => RasterStroker::outline([[0, '0'], [1, 1]], 2.0, false), 'Point 0 is not [x, y]'],
    'a coordinate that is not finite' => [fn () => RasterStroker::outline([[0, NAN], [1, 1]], 2.0, false), 'not a finite number'],
    'a coordinate past the limit' => [fn () => RasterStroker::outline([[0, 0], [RASTER_LIMIT * 2, 1]], 2.0, false), 'is past'],
    'a stroke that is not finite' => [fn () => RasterStroker::outline([[0, 0], [1, 1]], INF, false), 'The stroke is not a finite number'],
    'a stroke past the limit' => [fn () => RasterStroker::outline([[0, 0], [1, 1]], RASTER_LIMIT * 4, false), 'The stroke'],
    'a cap that is not one' => [fn () => RasterStroker::outline([[0, 0], [1, 1]], 2.0, false, 3), 'RASTER_CAP_BUTT'],
    'a join that is not one' => [fn () => RasterStroker::outline([[0, 0], [1, 1]], 2.0, false, 0, -1), 'RASTER_JOIN_MITER'],
    'a direction that is not a pair' => [fn () => RasterStroker::outline([[0, 0], [1, 1]], 2.0, false, 0, 0, 4.0, [1.0]), 'The direction is not [x, y]'],
    'a dash length under 0' => [fn () => RasterStroker::dashed([[0, 0], [1, 1]], false, [4, -1], 0.0), 'not a finite number, 0 or more'],
    'a dash length that is not finite' => [fn () => RasterStroker::dashed([[0, 0], [1, 1]], false, [INF], 0.0), 'not a finite number, 0 or more'],
    'a dash length that is a string' => [fn () => RasterStroker::dashed([[0, 0], [1, 1]], false, ['4'], 0.0), 'not a number'],
    'an offset that is not finite' => [fn () => RasterStroker::dashed([[0, 0], [1, 1]], false, [4], NAN), 'The offset'],
]);

it('throws a division by zero where a length is so small that it is 0', function (): void {
    // 1e-200 squared is not a number a double can hold
    expect(fn () => RasterStroker::outline([[0, 0], [1.0e-200, 0]], 2.0, false))->toThrow(DivisionByZeroError::class);
});

it('is its static methods and nothing else', function (): void {
    expect(fn () => new RasterStroker())->toThrow(Error::class)
        ->and((new ReflectionClass(RasterStroker::class))->isFinal())->toBeTrue()
        ->and(RASTER_CAP_BUTT)->toBe(0)
        ->and(RASTER_JOIN_BEVEL)->toBe(2)
        ->and(RASTER_MITER_LIMIT)->toBe(4.0);
});
