# Agent guidance — php-io-extensions/rasterize

1. **Read [`.okf/index.md`](.okf/index.md) first** before changing the API, the C, or packaging. Open only the concepts the change touches.
2. **Geometry only.** `RasterScanner` answers span bytes and never touches a pixel. No pixel stores, no colours, no dependency, no framework names. See [`.okf/api/scanner.md`](.okf/api/scanner.md).
3. **Plain C on the Zend API.** No Zephir, no FFI, no system library. `src/core.{h,c}` is plain C with no Zend in it and takes its memory through the two function pointers the class passes; `src/RasterScanner.c` is the class over it; `src/rasterize.c` is the module and its constants.
4. **The stubs are the declaration.** Edit `stubs/*.stub.php`, regenerate the matching `*_arginfo.h` with gen_stub, commit both. Never hand-edit arginfo. See [`.okf/runbooks/build.md`](.okf/runbooks/build.md).
5. **The span bytes are a contract.** 7 bytes, little-endian `y`, `x`, `length` (uint16) and `coverage` (uint8); rows ascending, `x` ascending, no overlap, coverage 1..255, an empty string for nothing. Consumers parse them as they are.
6. **The arithmetic is a contract.** IEEE double with `+ − × ÷`, `sqrt`, `floor` and `ceil` in the order [`.okf/api/scanner.md`](.okf/api/scanner.md) writes down, so the same input answers the same bytes on every platform and from a PHP implementation of the same steps. A change to it changes every caller's output: it needs its reason, and its tests work the expected coverage by hand.
7. **`-ffp-contract=off` stays in `config.m4`.** Without it the compiler fuses multiply-adds in `core.c` and the bytes move. Check with `objdump -d src/.libs/core.o | grep -c fmadd`: 0.
8. **Refuse, don't guess.** A non-finite coordinate, one past `RASTER_LIMIT`, a malformed list or an unknown rule is a `ValueError` naming what was wrong, before any scanning.
9. **NTS and ZTS.** No module globals. Every change builds with `-Wall -Wextra` without a warning from `src/` and passes Pest under NTS and ZTS on macOS and Linux.
10. **Tests are Pest v4 and hardware-free.** Expected spans come from the edge rules, worked by hand.
11. **Durable facts go in `.okf`.** Update the matching concept, bump its `generated.at`, append `.okf/log.md`. The bundle documents the package, never a session.
12. **Version**: `PHP_RASTERIZE_VERSION` moves only past a published tag; tags drive Packagist.
