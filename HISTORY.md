# Release History

### Version 3.0.0-Stilgar (May 2026)

- Full-color terminal support: ANSI 16-color, 256-color, and 24-bit truecolor are all supported, with named colors and safe defaults so terminals degrade gracefully.
- Color themes and customization: Built-in themes, plus a completely redesigned custom color editor with robust customization and live previews.
- TOML-based config: The client now uses `~/.config/bbs/config.toml` instead of the old `.bbsrc`/`.bbsfriends` setup. *Important: the old client files are not imported or removed.*
- Keychain support (macOS): Client passwords can optionally be saved to Keychain. Plain-text password saving was removed.
- Better URL support: `https://` and `www.` link detection. Supports multi-line URLs, and presents OSC-8 terminal links.
- Keepalive support: Uses TCP keepalive to help preserve sessions when aggressive timeouts are imposed by local hardware, network gear, or an ISP.
- Default editor behavior: The client now defaults to the `EDITOR` environment variable, but you can still configure a different editor command.
- Accessibility improvements:Colorblind theme, screen reader mode, cleaner paging prompts, runtime control of title bar updates. Disclaimer: Not as improved as I wanted; most accessibility problems are server-side.
- Other improvements: Better line wrapping, improved UTF-8 character handling, many edge-case bug fixes, and a major code refactor.
- Support was removed for: macros, SSL/TLS, plain-text password saves, old configuration files.

### Version 2.2.2 (March 2001)

- Added workaround for glibc `setjmp()` issue affecting second edit run.
- Fixed non-ANSI MORE-prompt color leakage in `filter.c`.

### Version 2.2.1 (March 2000)

- Applied non-color-mode enemy-list fix (Sbum patch).
- Corrected extended time decoder behavior.
- Updated copyright and web address references.

### Version 2.2.0 (August to October 1999)

- Reworked telnet parsing and moved key logic into `filter.c`.
- Added enemy kill-notification squelch option.
- Added initial color system for posts and express messages.
- Added automatic username support.
- Reworked away-from-keyboard behavior (moved away from macro hack).
- Added copyright/license/warranty info menu.

### Versions 2.1.0-2.1.2 Beta (October 1998)

- Integrated client updates from the Client 9/Blackout work:
- Unlimited friend/enemy list sizing.
- Ctrl-P backward name scroll.
- Alternate BBS site via command line.
- `.bbsfriends` reintegrated into `.bbsrc`.
- New default macro keys and first-time setup updates.
- Known beta issues were documented at the time (name matching and incomplete non-Unix support work).

## Version 2.? (January 1998)

- Added `xland` behavior for quicker reply setup.

### Version 2.0.26 (May 1997)

- Updated built-in default ISCA address after host move.

### Versions 2.0.19-2.0.25 (September to December 1996)

- Fixed shell feature regression.
- Updated banner/address details.
- Synced parsing logic with server-side text changes.
- Fixed enemy-list behavior in ANSI mode.
- Added command key remapping support and related send-path changes.

### Versions 2.0.14-2.0.18 (February to July 1996)

- Added friend-list color support and bold/non-bold ANSI preferences.
- Introduced major I/O routine updates.
- Added compatibility path for telnet daemons ("Heinous" patch).
- Added login-shell mode with PID-scoped temp files.

### Versions 2.0.8-2.0.13 (September to December 1995)

- Pulled in stdio updates and general stability fixes.
- Expanded Ctrl-N history behavior (name scroll with dedupe).
- Added ping-on-blank behavior controls, then later refined/removed as server behavior changed.
- Fixed several ANSI/capture/segfault issues.
- UI cleanup in config prompts and option naming.

### Versions 2.0.5-2.0.7 (June 1995)

- Fixed infinite autoreply loop behavior.
- Added packaging/dist build updates.

### Versions 2.0.1-2.0.4 (April 1995)

- Started 2.0 branch.
- Added automatic away-message autoreply support.
- Added Ctrl-N to paste/recall recently seen names from posts/X messages.
- Added early term compatibility option.

### Version 1.5 (February 1994)

- `v1.5` fixed a serious character-dropping bug.
- Improved enemy-list behavior for long posts and ANSI mode.
- Added Ctrl-R reprint behavior to match server-side changes.
- Updated line-ending translation behavior for older BSD, VMS, and termio-style systems.
- Historical note: this era was treated as the likely end of major 1.x feature work.

### Versions 1.41-1.42 (December 1993)

- `v1.41` added Ctrl-W line erase and fixed major X-message wrapping issues.
- `v1.42` added broader machine compatibility and term compatibility support.

### Versions 1.31 and 1.4 (November 1993)

- `v1.31` minor fixes and better file truncation behavior; treated as an unreleased test version.
- `v1.4` more bug fixes, VMS compatibility updates, and safer default file permissions.

### Versions 1.2, 1.21, and 1.3 (October 1993)

- `v1.2` fixed multiple issues, improved Ctrl-W behavior, and improved capture output.
- `v1.21` added `BBSRC` and `BBSTMP` environment variable support for Unix builds.
- `v1.3` expanded `.bbsrc` setup so first-time install was easier.

### Version 1.11 (August 1993)

- `v1.11` minor bug fixes for Unix and VMS.

### Versions 1.01-1.1 (July 1993)

- `v1.01` improved portability and added automatic X-message wrapping.
- `v1.02` added more portability work and fixed X-wrap bugs.
- `v1.03` fixed another X-wrap bug, added early VMS porting code, and improved invalid-character handling.
- `v1.1` added NeXT window titles, added a shell key, and shipped the initial public VMS client.

### Version 1.0 (June 1993)

- `v1.0` initial Unix client release.
