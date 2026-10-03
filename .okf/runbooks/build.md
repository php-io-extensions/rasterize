---
type: Runbook
title: Build, install, test
description: Scratch build with gen_stub, the two installers, Pest, the Pi loop.
resource: install-macos.sh
tags: [build, linux, macos, pest]
status: draft
generated: { by: claude-opus/5.5, at: 2026-10-03T22:56:49Z }
sources:
  - id: mac
    resource: install-macos.sh
    title: install-macos.sh
  - id: debian
    resource: install-debian-trixie.sh
    title: install-debian-trixie.sh
---

# Overview

No system library needed: php-dev and a C compiler.

Dev loop: copy `config.m4 php_rasterize.h src stubs` to a scratch dir, `phpize`, `php build/gen_stub.php stubs`, `./configure --enable-rasterize --with-php-config=…`, `make`. Copy `stubs/*_arginfo.h` back after a stub edit. Run Pest with `-d extension=<scratch>/modules/rasterize.so`. Build with `-Wall -Wextra`; no warning from `src/` is acceptable.

macOS (`install-macos.sh`): builds in a temp copy for php@8.4 and php@8.4-zts, ad-hoc signs, writes `30-rasterize.ini`.[^mac]

Linux (`install-debian-trixie.sh`): builds in place, installs `rasterize.so`, writes `30-rasterize.ini`, removes build output.[^debian]

Pi from the Mac: tree on the Mac is authoritative, Pi copy disposable. `COPYFILE_DISABLE=1 tar --no-mac-metadata --exclude .git -czf - -C <ext> . | fnk 'tar -xzf - -C ~/rz'`, install, Pest, `rm -rf ~/rz`. Without `--no-mac-metadata` the copy carries `._*` files that gen_stub trips on.

`config.m4` passes `-ffp-contract=off`; a build without it may fuse multiply-adds and answer different bytes from a PHP implementation of the same arithmetic. Check with `objdump -d src/.libs/core.o | grep -c fmadd`: 0 on arm64.

To test a consumer without the ext, run with a `PHP_INI_SCAN_DIR` that omits `30-rasterize.ini`; `php -n` drops every other conf.d extension too.

[^mac]: Same script as ext-fb's, names changed.
[^debian]: Same script as ext-fb's, names changed.
