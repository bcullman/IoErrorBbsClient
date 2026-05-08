# Changelog (Legacy Rollup)

## Current Fork Updates

### 2026-05

- Removed saved-password support in `.bbsrc`.
- Added optional macOS Keychain password storage with a build-time `--enable-keychain` switch and runtime `keychain 0|1` setting.
- Added per-BBS Keychain delete handling and automatic deletion when Keychain storage is disabled.

## Contributor Notes Moved Out of Source Comments

- Telnet negotiation fallback behavior (sending `IAC WONT` for unsupported options, including Heinous-compatible behavior) was added in the IO ERROR era.
- `.bbsfriends` discovery support was introduced by Dave (Isoroku) and later updated by IO ERROR to use read-only handling when the file exists.
- Struct layout ordering requirements were documented during IO ERROR-era refactors and retained as technical constraints.
- Legacy module banner attributions for `color.c` and `filter.c` were consolidated here: IO ERROR (Michael Hampton) maintained the 2.x client line based on the earlier ISCA client base and patches.

## 2.2 Series

### 2001-03

- Added workaround for glibc `setjmp()` issue affecting second edit run.
- Fixed non-ANSI MORE-prompt color leakage in `filter.c`.
- Released `2.2.2`.

### 2000-03

- Applied non-color-mode enemy-list fix (Sbum patch).
- Corrected extended time decoder behavior.
- Updated copyright and web address references.
- Released `2.2.1`.

### 1999-08 to 1999-10

- Reworked telnet parsing and moved key logic into `filter.c`.
- Added enemy kill-notification squelch option.
- Added initial color system for posts and express messages.
- Added automatic username support.
- Reworked away-from-keyboard behavior (moved away from macro hack).
- Added copyright/license/warranty info menu.
- Released `2.2.0`.

## 2.1 Series

### 1998-10

- Integrated client updates from the Client 9/Blackout work:
- Unlimited friend/enemy list sizing.
- Ctrl-P backward name scroll.
- Alternate BBS site via command line.
- `.bbsfriends` reintegrated into `.bbsrc`.
- New default macro keys and first-time setup updates.
- Known beta issues were documented at the time (name matching and incomplete non-Unix support work).
- Released `2.1.0` (beta), `2.1.1` (beta), `2.1.2` (beta).

## 2.0 Series

### 1998-01

- Added `xland` behavior for quicker reply setup.

### 1997-05

- Updated built-in default ISCA address after host move.
- Released `2.0.26`.

### 1996-09 to 1996-12

- Fixed shell feature regression.
- Updated banner/address details.
- Synced parsing logic with server-side text changes.
- Fixed enemy-list behavior in ANSI mode.
- Added command key remapping support and related send-path changes.
- Released `2.0.19` through `2.0.25`.

### 1996-02 to 1996-07

- Added friend-list color support and bold/non-bold ANSI preferences.
- Introduced major I/O routine updates.
- Added compatibility path for telnet daemons ("Heinous" patch).
- Added login-shell mode with PID-scoped temp files.
- Released `2.0.14` through `2.0.18`.

### 1995-09 to 1995-12

- Pulled in stdio updates and general stability fixes.
- Expanded Ctrl-N history behavior (name scroll with dedupe).
- Added ping-on-blank behavior controls, then later refined/removed as server behavior changed.
- Fixed several ANSI/capture/segfault issues.
- UI cleanup in config prompts and option naming.
- Released `2.0.8` through `2.0.13`.

### 1995-06

- Fixed infinite autoreply loop behavior.
- Added packaging/dist build updates.
- Released `2.0.5`, `2.0.6`, `2.0.7`.

### 1995-04

- Started 2.0 branch.
- Added automatic away-message autoreply support.
- Added Ctrl-N to paste/recall recently seen names from posts/X messages.
- Added early term compatibility option.
- Released `2.0.1` through `2.0.4`.

## 1.x Series (1993-1994)

### 1994-02

- `v1.5` fixed a serious character-dropping bug.
- Improved enemy-list behavior for long posts and ANSI mode.
- Added Ctrl-R reprint behavior to match server-side changes.
- Updated line-ending translation behavior for older BSD, VMS, and termio-style systems.
- Historical note: this era was treated as the likely end of major 1.x feature work.

### 1993-12

- `v1.41` added Ctrl-W line erase and fixed major X-message wrapping issues.
- `v1.42` added broader machine compatibility and term compatibility support.

### 1993-11

- `v1.31` minor fixes and better file truncation behavior; treated as an unreleased test version.
- `v1.4` more bug fixes, VMS compatibility updates, and safer default file permissions.

### 1993-10

- `v1.2` fixed multiple issues, improved Ctrl-W behavior, and improved capture output.
- `v1.21` added `BBSRC` and `BBSTMP` environment variable support for Unix builds.
- `v1.3` expanded `.bbsrc` setup so first-time install was easier.

### 1993-08

- `v1.11` minor bug fixes for Unix and VMS.

### 1993-07

- `v1.01` improved portability and added automatic X-message wrapping.
- `v1.02` added more portability work and fixed X-wrap bugs.
- `v1.03` fixed another X-wrap bug, added early VMS porting code, and improved invalid-character handling.
- `v1.1` added NeXT window titles, added a shell key, and shipped the initial public VMS client.

### 1993-06

- `v1.0` initial Unix client release.
