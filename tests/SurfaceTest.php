<?php

declare(strict_types=1);

/*
 * Every stub declaration is what the loaded extension exposes: the stubs are
 * the source of truth, so a method or constant missing from the build fails here.
 */

function stubDeclarations(): array
{
    $declared = ['constants' => [], 'classes' => []];

    foreach (glob(__DIR__.'/../stubs/*.stub.php') as $stub) {
        $class = null;
        foreach (file($stub) as $line) {
            if (preg_match('/^(?:final\s+)?class\s+(\w+)/', $line, $m)) {
                $class = $m[1];
                $declared['classes'][$class] ??= [];
            } elseif (preg_match('/^const\s+(\w+)/', $line, $m)) {
                $declared['constants'][] = $m[1];
            } elseif ($class !== null && preg_match('/^\s+public\s+function\s+(\w+)/', $line, $m)) {
                $declared['classes'][$class][] = $m[1];
            }
        }
    }

    return $declared;
}

it('exposes every constant, class and method the stubs declare', function (): void {
    $declared = stubDeclarations();

    expect($declared['constants'])->toHaveCount(5)
        ->and(array_keys($declared['classes']))->toBe(['RasterScanner'])
        ->and($declared['classes']['RasterScanner'])->toHaveCount(7);

    foreach ($declared['constants'] as $constant) {
        expect(defined($constant))->toBeTrue("{$constant} is missing");
    }
    foreach ($declared['classes'] as $class => $methods) {
        foreach ($methods as $method) {
            expect(method_exists($class, $method))->toBeTrue("{$class}::{$method}() is missing");
        }
    }
});

it('reports its version', function (): void {
    expect(phpversion('rasterize'))->toBe('0.10.0');
});

it('declares a final class that cannot be cloned or serialized', function (): void {
    $scanner = new RasterScanner(0, 0, 1, 1, false);

    expect((new ReflectionClass(RasterScanner::class))->isFinal())->toBeTrue()
        ->and(fn () => clone $scanner)->toThrow(Error::class)
        ->and(fn () => serialize($scanner))->toThrow(Exception::class);
});

it('refuses a second construction', function (): void {
    $scanner = new RasterScanner(0, 0, 2, 2, false);

    expect(fn () => $scanner->__construct(0, 0, 4, 4, true))->toThrow(Error::class, 'already constructed')
        ->and($scanner->clip())->toBe([0, 0, 2, 2])
        ->and($scanner->antialias())->toBeFalse();
});
