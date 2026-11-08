<?php

declare(strict_types=1);

it('holds its clip and edge mode', function (): void {
    $scanner = new RasterScanner(1, 2, 3, 4, true);

    expect($scanner->clip())->toBe([1, 2, 3, 4])
        ->and($scanner->antialias())->toBeTrue()
        ->and((new RasterScanner(0, 0, 1, 1, false))->antialias())->toBeFalse();
});

it('refuses a clip that is empty or reaches past 65535', function (array $clip): void {
    expect(fn () => new RasterScanner(...[...$clip, false]))->toThrow(ValueError::class, 'is empty or not inside 0..65535');
})->with([[[-1, 0, 1, 1]], [[0, -1, 1, 1]], [[0, 0, 0, 1]], [[0, 0, 1, 0]], [[65535, 0, 1, 1]], [[0, 1, 1, 65535]], [[0, 0, PHP_INT_MAX, 1]]]);

it('takes a clip that ends exactly at 65535', function (): void {
    expect(spans((new RasterScanner(0, 65534, 65535, 1, false))->path([[0, 65534, 65535, 65534, 65535, 65535, 0, 65535]])))->toBe([[65534, 0, 65535, 255]]);
});

it('fills a path, taking the pixels whose centres are inside', function (): void {
    $scanner = new RasterScanner(0, 0, 5, 5, false);

    expect(grid($scanner->path([[0, 0, 4, 0, 0, 4]]), 0, 0, 5, 5))->toBe(['###..', '##...', '#....', '.....', '.....']);
});

it('unites overlaps under non-zero and cuts them under even-odd', function (): void {
    $scanner = new RasterScanner(0, 0, 6, 1, false);
    $squares = [[0, 0, 4, 0, 4, 1, 0, 1], [2, 0, 6, 0, 6, 1, 2, 1]];

    expect(grid($scanner->path($squares), 0, 0, 6, 1))->toBe(['######'])
        ->and(grid($scanner->path($squares, RASTER_EVEN_ODD), 0, 0, 6, 1))->toBe(['##..##']);
});

it('answers coverage with anti-aliasing: halves at both ends, sixteen sample lines a row', function (): void {
    $scanner = new RasterScanner(0, 0, 4, 2, true);

    expect(spans($scanner->path([[0.5, 0, 2.5, 0, 2.5, 1, 0.5, 1]])))->toBe([[0, 0, 1, 128], [0, 1, 1, 255], [0, 2, 1, 128]])
        ->and(spans((new RasterScanner(0, 0, 1, 2, true))->path([[0, 0.2, 1, 0.2, 1, 1.2, 0, 1.2]])))->toBe([[0, 0, 1, 0xCF], [1, 0, 1, 0x30]]);
});

it('fills an ellipse and the ring of a stroked one', function (): void {
    $scanner = new RasterScanner(0, 0, 8, 8, false);

    expect(grid($scanner->ellipse(4, 4, 3, 3), 0, 0, 8, 8))->toBe(['........', '..####..', '.######.', '.######.', '.######.', '.######.', '..####..', '........'])
        ->and(grid($scanner->ring(4, 4, 3, 3, 2), 0, 0, 8, 8))->toBe(['..####..', '.######.', '###..###', '##....##', '##....##', '###..###', '.######.', '..####..']);
});

it('draws one-pixel polylines between the floored points, every pixel once, in either mode', function (bool $antialias): void {
    $scanner = new RasterScanner(0, 0, 6, 5, $antialias);

    expect(grid($scanner->polyline([1, 1, 4.9, 1.2, 4, 3, 1, 3], true), 0, 0, 6, 5))->toBe(['......', '.####.', '.#..#.', '.####.', '......'])
        ->and(spans($scanner->polyline([1, 1, 4, 1, 4, 3, 1, 3], true)))->toHaveCount(4)
        ->and(spans($scanner->polyline([2, 2])))->toBe([[2, 2, 1, 255]]);
})->with([false, true]);

it('walks only the part of a far-off line inside the clip', function (): void {
    $spans = spans((new RasterScanner(0, 0, 4, 4, false))->polyline([-1e9, -1e9, 1e9, 1e9]));

    expect($spans)->toBe([[0, 0, 1, 255], [1, 1, 1, 255], [2, 2, 1, 255], [3, 3, 1, 255]]);
});

it('answers nothing for shapes with no inside or outside the clip', function (): void {
    $scanner = new RasterScanner(0, 0, 4, 4, true);

    expect($scanner->path([]))->toBe('')
        ->and($scanner->path([[0, 0, 4, 4]]))->toBe('')
        ->and($scanner->path([[10, 10, 20, 10, 20, 20]]))->toBe('')
        ->and($scanner->ellipse(2, 2, 0, 1))->toBe('')
        ->and($scanner->ring(2, 2, 1, 1, 0))->toBe('')
        ->and($scanner->polyline([]))->toBe('')
        ->and($scanner->polyline([10, 10, 20, 20]))->toBe('');
});

it('refuses malformed contours and points, naming them', function (Closure $call, string $message): void {
    expect($call)->toThrow(ValueError::class, $message);
})->with([
    'an odd count' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, 0, 4]]), 'Contour 0 is not a flat list of x, y pairs.'],
    'not a list' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, 0, 4, 4], 'x']), 'Contour 1 is not a flat list of x, y pairs.'],
    'keyed points' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([['a' => 0, 'b' => 0]]), 'Contour 0 is not a flat list of x, y pairs.'],
    'a string' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, 4, '0', 4, 4]]), 'Contour 0 holds something that is not a number.'],
    'keyed contours' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path(['a' => [0, 0, 4, 0, 4, 4]]), 'must be a list of contours'],
    'a rule' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([], 2), 'must be RASTER_NON_ZERO or RASTER_EVEN_ODD'],
    'NAN' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->path([[0, 0, NAN, 0, 4, 4]]), 'A coordinate is not a finite number.'],
    'past the limit' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->polyline([0, 0, 2 ** 30 + 1, 0]), 'A coordinate 1073741825 is past ±1073741824.'],
    'an odd point list' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->polyline([0, 0, 1]), 'Points is not a flat list of x, y pairs.'],
    'an infinite radius' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->ellipse(2, 2, INF, 1), 'rx is not a finite number.'],
    'a stroke past the limit' => [fn () => (new RasterScanner(0, 0, 4, 4, false))->ring(2, 2, 1, 1, -2e9), 'stroke -2000000000 is past ±1073741824.'],
]);

it('takes the limit itself', function (): void {
    expect((new RasterScanner(0, 0, 4, 4, false))->polyline([-RASTER_LIMIT, -RASTER_LIMIT, RASTER_LIMIT, RASTER_LIMIT]))->not->toBe('');
});

it('cuts a rounded rect corner by its own quarter circle', function (): void {
    // row 0 samples y = 0.5: t = -0.75, h = 2·sqrt(0.4375) ≈ 1.32, left ≈ 0.68, so the first centre in is 1.5
    $scanner = new RasterScanner(0, 0, 6, 4, false);

    expect(grid($scanner->roundRect(0, 0, 6, 4, [2, 0, 0, 0]), 0, 0, 6, 4))->toBe(['.#####', '######', '######', '######']);
});

it('answers a square rounded rect as the path of its box when the radii are zero', function (bool $antialias): void {
    $scanner = new RasterScanner(0, 0, 16, 12, $antialias);

    expect($scanner->roundRect(2.3, 1.7, 11.7, 7.9, [0, 0, 0, 0]))->toBe($scanner->path([[2.3, 1.7, 11.7, 1.7, 11.7, 7.9, 2.3, 7.9]]));
})->with([false, true]);

it('answers a rounded rect that is a circle as the ellipse', function (bool $antialias): void {
    $scanner = new RasterScanner(0, 0, 24, 24, $antialias);

    expect($scanner->roundRect(0, 0, 20, 20, [10, 10, 10, 10]))->toBe($scanner->ellipse(10, 10, 10, 10));
})->with([false, true]);

it('rings a rounded rect: the band and the inset box share no pixel and make the box', function (): void {
    $scanner = new RasterScanner(0, 0, 32, 24, false);
    $cells = fn (string $bytes): array => array_merge(...array_map(fn (array $s): array => array_map(fn (int $i): string => ($s[1] + $i).','.$s[0], range(0, $s[2] - 1)), spans($bytes)));
    $band = $cells($scanner->roundRing(2, 2, 26, 18, [6, 3, 0, 5], 2));
    $inner = $cells($scanner->roundRect(4, 4, 24, 16, [4, 1, 0, 3]));
    $outer = $cells($scanner->roundRect(2, 2, 26, 18, [6, 3, 0, 5]));

    expect(array_intersect($band, $inner))->toBe([])
        ->and([...$band, ...$inner])->toEqualCanonicalizing($outer)
        ->and($scanner->roundRing(2, 2, 22, 12, [3, 3, 3, 3], 5))->toBe($scanner->roundRect(2, 2, 22, 12, [3, 3, 3, 3]));
});

it('blurs a shadow by the smoothstep profile at each pixel centre', function (): void {
    $scanner = new RasterScanner(0, 0, 64, 64, true);
    $row = [];
    foreach (spans($scanner->shadow(12, 12, 52, 52, 0, 8)) as [$y, $x, $length, $coverage]) {
        if ($y === 32) {
            for ($i = 0; $i < $length; $i++) {
                $row[$x + $i] = $coverage;
            }
        }
    }

    for ($k = -8; $k <= 8; $k++) {
        $t = min(1.0, max(0.0, ($k + 0.5 + 8) / 16));
        $expected = (int) floor((1 - $t * $t * (3 - 2 * $t)) * 255 + 0.5);
        expect($row[52 + $k] ?? 0)->toBe($expected)->and($row[11 - $k] ?? 0)->toBe($expected);
    }
});

it('draws a zero-blur shadow as the hard box in both edge modes, and thresholds hard edges at 128', function (): void {
    $hard = new RasterScanner(0, 0, 32, 32, false);
    $soft = new RasterScanner(0, 0, 32, 32, true);

    expect($soft->shadow(4, 4, 14, 12, 0, 0))->toBe($hard->path([[4, 4, 14, 4, 14, 12, 4, 12]]))
        ->and($hard->shadow(4, 4, 14, 12, 0, 0))->toBe($hard->path([[4, 4, 14, 4, 14, 12, 4, 12]]))
        ->and(array_unique(array_column(spans($hard->shadow(8, 8, 24, 24, 3, 5)), 3)))->toBe([255]);
});

it('cuts the rounded shapes to the clip', function (bool $antialias): void {
    $scanner = new RasterScanner(8, 8, 16, 16, $antialias);

    foreach ([$scanner->roundRect(2, 2, 14, 14, [4, 4, 4, 4]), $scanner->roundRing(18, 18, 28, 28, [3, 3, 3, 3], 2), $scanner->shadow(2, 2, 14, 14, 3, 4)] as $bytes) {
        expect($bytes)->not->toBe('');
        foreach (spans($bytes) as [$y, $x, $length]) {
            expect($y >= 8 && $y < 24 && $x >= 8 && $x + $length <= 24)->toBeTrue();
        }
    }
    expect($scanner->roundRect(30, 0, 35, 5, [2, 2, 2, 2]))->toBe('')
        ->and($scanner->shadow(40, 40, 45, 45, 1, 2))->toBe('');
})->with([false, true]);

it('refuses radii that are not four finite numbers', function (array $radii): void {
    expect(fn () => (new RasterScanner(0, 0, 8, 8, false))->roundRect(0, 0, 4, 4, $radii))->toThrow(ValueError::class);
})->with([[[1, 2, 3]], [[1, 2, 3, 'x']], [[1, 2, 3, INF]], [['a' => 1, 'b' => 2, 'c' => 3, 'd' => 4]]]);

it('walks only the rows and columns a shape reaches: a small shape in a large clip answers the spans of a tight one', function (bool $antialias): void {
    $tight = new RasterScanner(497, 697, 26, 22, $antialias);
    $large = new RasterScanner(0, 0, 4000, 3000, $antialias);
    $shapes = [
        'path' => fn (RasterScanner $s) => $s->path([[500.3, 701.2, 517.7, 703.4, 506.1, 713.9]]),
        'ellipse' => fn (RasterScanner $s) => $s->ellipse(509.25, 707.5, 8.6, 6.3),
        'ring' => fn (RasterScanner $s) => $s->ring(509.25, 707.5, 8.6, 6.3, 2.5),
        'rounded rect' => fn (RasterScanner $s) => $s->roundRect(500.4, 700.6, 518.6, 713.7, [4.0, 0.0, 6.5, 2.25]),
        'rounded ring' => fn (RasterScanner $s) => $s->roundRing(500.4, 700.6, 518.6, 713.7, [4.0, 0.0, 6.5, 2.25], 1.75),
    ];
    foreach ($shapes as $name => $draw) {
        expect($draw($tight))->not->toBe('', $name)
            ->and(bin2hex($draw($large)))->toBe(bin2hex($draw($tight)), $name);
    }
})->with([true, false]);

it('answers a batch of ops as the calls of the same names, one span string an op, in order', function (bool $antialias): void {
    $scanner = new RasterScanner(0, 0, 40, 30, $antialias);
    $ops = [
        [RASTER_OP_PATH, [[1.5, 2, 30.2, 4.1, 9, 25.7], [5, 5, 12, 5, 12, 12]], RASTER_EVEN_ODD],
        [RASTER_OP_ELLIPSE, 20.25, 15, 9.5, 6],
        [RASTER_OP_RING, 20, 15.5, 9, 7, 2.5],
        [RASTER_OP_ROUND_RECT, 2.4, 3, 33.1, 26, [4, 0, 6.5, 2]],
        [RASTER_OP_ROUND_RING, 2.4, 3, 33.1, 26, [4, 0, 6.5, 2], 1.75],
        [RASTER_OP_SHADOW, 6, 7, 30, 22, 3, 2.5],
        [RASTER_OP_POLYLINE, [1, 1, 38, 4, 20, 28], true],
        [RASTER_OP_ELLIPSE, 500, 500, 3, 3],
    ];
    $one = [
        $scanner->path($ops[0][1], RASTER_EVEN_ODD),
        $scanner->ellipse(20.25, 15, 9.5, 6),
        $scanner->ring(20, 15.5, 9, 7, 2.5),
        $scanner->roundRect(2.4, 3, 33.1, 26, [4, 0, 6.5, 2]),
        $scanner->roundRing(2.4, 3, 33.1, 26, [4, 0, 6.5, 2], 1.75),
        $scanner->shadow(6, 7, 30, 22, 3, 2.5),
        $scanner->polyline([1, 1, 38, 4, 20, 28], true),
        '',
    ];

    expect(array_map('bin2hex', $scanner->batch($ops)))->toBe(array_map('bin2hex', $one))
        ->and($scanner->batch([]))->toBe([]);
})->with([true, false]);

it('refuses a batch with any bad op before scanning one, naming it', function (array $op, string $message): void {
    $scanner = new RasterScanner(0, 0, 8, 8, true);

    expect(fn () => $scanner->batch([[RASTER_OP_ELLIPSE, 4, 4, 2, 2], $op]))->toThrow(ValueError::class, $message);
})->with([
    'not a list' => [['a' => 1], 'Op 1 is not a list that starts with a RASTER_OP_* shape'],
    'an unknown op' => [[99, 1, 2], 'Op 1 is not a list that starts with a RASTER_OP_* shape'],
    'too few entries' => [[RASTER_OP_ELLIPSE, 4, 4, 2], 'Op 1 takes 5 entries, the op and its arguments; it has 4'],
    'a string for a number' => [[RASTER_OP_RING, 4, 4, 2, 2, 'x'], 'Op 1: stroke is not a number'],
    'a number past the limit' => [[RASTER_OP_SHADOW, 0, 0, 2 ** 31, 4, 1, 1], 'x1 2147483648 is past'],
    'bad radii' => [[RASTER_OP_ROUND_RECT, 0, 0, 4, 4, [1, 1, 1]], 'Radii are a list of four numbers'],
    'a bad rule' => [[RASTER_OP_PATH, [[0, 0, 4, 0, 0, 4]], 7], 'Op 1: the rule is not RASTER_NON_ZERO or RASTER_EVEN_ODD'],
    'a bad contour' => [[RASTER_OP_PATH, [[0, 0, 4]], RASTER_NON_ZERO], 'Contour 0 is not a flat list of x, y pairs'],
    'closed not a bool' => [[RASTER_OP_POLYLINE, [0, 0, 4, 4], 1], 'Op 1: closed is not a bool'],
    'bad points' => [[RASTER_OP_POLYLINE, [0, 0, 'y', 4], false], 'Points holds something that is not a number'],
]);
