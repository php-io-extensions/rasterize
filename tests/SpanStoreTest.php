<?php

declare(strict_types=1);

/*
 * RasterSpanStore keeps span lists in C by number and paints them, moved by whole pixels and cut to a
 * clip, into an FbBuffer through ext-fb's span sink. Expected pixels are worked from the scanner's
 * coverage rule and ext-fb's blend.
 */

const STORE_RED = 0xFF0000FF;

/** A 20 x 12 RGBA buffer, every pixel 0. */
function storeBuffer(): FbBuffer
{
    return new FbBuffer(new FbFormat(FB_LAYOUT_RGBA8888, channelOrder: FB_CHANNELS_RGBA), 20, 12);
}

/** The buffer as rows: '#' a pixel painted opaque red, '.' one left 0, '?' anything else. */
function storeGrid(FbBuffer $buffer): array
{
    $rows = [];
    for ($y = 0; $y < $buffer->height(); $y++) {
        $row = '';
        for ($x = 0; $x < $buffer->width(); $x++) {
            $row .= match ($buffer->get($x, $y)) { 0 => '.', STORE_RED => '#', default => '?' };
        }
        $rows[] = $row;
    }

    return $rows;
}

/** A w x h box from (0, 0) as one contour. */
function storeBox(float $w, float $h): array
{
    return [[0, 0, $w, 0, $w, $h, 0, $h]];
}

it('paints a kept list where each placement puts it, and answers the box it painted', function (): void {
    $store = new RasterSpanStore(false);
    $box = $store->add(storeBox(3, 2), 3, 2);
    $buffer = storeBuffer();

    expect($store->fill([$box, 1, 1, $box, 12, 8], STORE_RED, $buffer, 0, 0, 20, 12))->toBe([1, 1, 14, 9])
        ->and(storeGrid($buffer))->toBe([
            '....................',
            '.###................',
            '.###................',
            '....................',
            '....................',
            '....................',
            '....................',
            '....................',
            '............###.....',
            '............###.....',
            '....................',
            '....................',
        ]);
})->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('cuts every placed list to the clip, a list placed partly off the buffer included', function (): void {
    $store = new RasterSpanStore(false);
    $box = $store->add(storeBox(4, 3), 4, 3);
    $buffer = storeBuffer();

    expect($store->fill([$box, -2, -1, $box, 5, 5], STORE_RED, $buffer, 1, 0, 6, 6))->toBe([1, 0, 6, 6])
        ->and(array_slice(storeGrid($buffer), 0, 7))->toBe([
            '.#..................',
            '.#..................',
            '....................',
            '....................',
            '....................',
            '.....##.............',
            '....................',
        ]);
})->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('keeps the edges it was made with: a half-covered pixel is painted at half coverage', function (): void {
    $store = new RasterSpanStore(true);
    $half = $store->add(storeBox(1.5, 1), 2, 1);
    $buffer = new FbBuffer(new FbFormat(FB_LAYOUT_RGBA8888, channelOrder: FB_CHANNELS_RGBA), 4, 1);
    $buffer->fill(0x000000FF);
    $store->fill([$half, 0, 0], STORE_RED, $buffer, 0, 0, 4, 1);

    // coverage floor(0.5 x 255 + 0.5) = 128: red (255 x 128 + 0 x 127 + 127) / 255 = 128
    expect($store->antialias())->toBeTrue()
        ->and($buffer->get(0, 0) >> 24)->toBe(255)
        ->and($buffer->get(1, 0) >> 24)->toBe(128)
        ->and($buffer->get(2, 0))->toBe(0x000000FF);
})->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('paints overlapping placements one after the other, in order', function (): void {
    $store = new RasterSpanStore(false);
    $dot = $store->add(storeBox(1, 1), 1, 1);
    $buffer = new FbBuffer(new FbFormat(FB_LAYOUT_RGBA8888, channelOrder: FB_CHANNELS_RGBA), 2, 1);
    $buffer->fill(0x000000FF);
    $store->fill([$dot, 0, 0, $dot, 0, 0], 0xFF000080, $buffer, 0, 0, 2, 1);

    // red over black at alpha 128, twice: (255 x 128 + 127) / 255 = 128, then (255 x 128 + 128 x 127 + 127) / 255 = 192
    expect($buffer->get(0, 0) >> 24)->toBe(192);
})->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');

it('keeps nothing for contours that cover no pixel, and hands a removed number out again', function (): void {
    $store = new RasterSpanStore(true);

    expect($store->add([], 4, 4))->toBe(-1)
        ->and($store->add(storeBox(2, 2), 4, 4))->toBe(0)
        ->and($store->add([[10, 10, 12, 10, 12, 12]], 4, 4))->toBe(-1)
        ->and($store->add(storeBox(3, 1), 4, 4))->toBe(1)
        // 2 x 2 hard box is 2 spans; 3 x 1 is 1: 7 bytes each
        ->and([$store->count(), $store->size()])->toBe([2, 21]);

    $store->remove(0);
    expect([$store->count(), $store->size()])->toBe([1, 7])
        ->and($store->add(storeBox(1, 1), 4, 4))->toBe(0)
        ->and($store->add(storeBox(1, 1), 4, 4))->toBe(2);

    $store->clear();
    expect([$store->count(), $store->size()])->toBe([0, 0])
        ->and($store->add(storeBox(1, 1), 4, 4))->toBe(0);
});

it('refuses what it cannot keep or paint, before painting anything', function (Closure $call, string $error, string $message): void {
    $buffer = storeBuffer();
    $store = new RasterSpanStore(false);
    $box = $store->add(storeBox(2, 2), 2, 2);
    $store->remove($store->add(storeBox(1, 1), 1, 1));

    expect(fn () => $call($store, $box, $buffer))->toThrow($error, $message)
        ->and($buffer->bytes())->toBe(str_repeat("\0", 20 * 12 * 4));
})->with([
    'a side of 0' => [fn ($s) => $s->add(storeBox(1, 1), 0, 4), ValueError::class, 'a box of 1..65535 by 1..65535'],
    'a side past 65535' => [fn ($s) => $s->add(storeBox(1, 1), 4, 65536), ValueError::class, 'a box of 1..65535 by 1..65535'],
    'an unknown rule' => [fn ($s) => $s->add(storeBox(1, 1), 4, 4, 7), ValueError::class, 'RASTER_NON_ZERO or RASTER_EVEN_ODD'],
    'a contour that is not numbers' => [fn ($s) => $s->add([[0, 0, 'x', 1]], 4, 4), ValueError::class, 'Contour 0'],
    'a number never handed out' => [fn ($s, $box, $b) => $s->fill([$box, 0, 0, 9, 0, 0], STORE_RED, $b, 0, 0, 20, 12), ValueError::class, "Placement's number 9 is not a span list"],
    'a number removed' => [fn ($s, $box, $b) => $s->fill([$box, 0, 0, 1, 0, 0], STORE_RED, $b, 0, 0, 20, 12), ValueError::class, "Placement's number 1 is not a span list"],
    'removing a number removed' => [fn ($s) => $s->remove(1), ValueError::class, 'Number 1 is not a span list'],
    'placements not in threes' => [fn ($s, $box, $b) => $s->fill([$box, 0], STORE_RED, $b, 0, 0, 20, 12), ValueError::class, 'a flat list of number, x, y'],
    'a placement not whole' => [fn ($s, $box, $b) => $s->fill([$box, 0.5, 0], STORE_RED, $b, 0, 0, 20, 12), ValueError::class, 'not a whole number'],
    'a placement past the reach' => [fn ($s, $box, $b) => $s->fill([$box, 0, -(2 ** 30) - 1], STORE_RED, $b, 0, 0, 20, 12), ValueError::class, 'past +/-2^30'],
    'a clip past the buffer' => [fn ($s, $box, $b) => $s->fill([$box, 0, 0], STORE_RED, $b, 10, 0, 11, 12), ValueError::class, 'not inside the 20x12 framebuffer'],
    'an empty clip' => [fn ($s, $box, $b) => $s->fill([$box, 0, 0], STORE_RED, $b, 0, 0, 0, 12), ValueError::class, 'The clip is empty'],
    'a colour past 32 bits' => [fn ($s, $box, $b) => $s->fill([$box, 0, 0], 1 << 32, $b, 0, 0, 20, 12), ValueError::class, '0xRRGGBBAA'],
    'not an FbBuffer' => [fn ($s, $box) => $s->fill([$box, 0, 0], STORE_RED, new stdClass, 0, 0, 1, 1), TypeError::class, 'must be a constructed FbBuffer, stdClass given'],
])->skip(! class_exists(FbBuffer::class), 'ext-fb is not loaded in this PHP.');
