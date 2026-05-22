# IoError BBS Client

This fork is **2.3.10-Stilgar** by Stilgar, based on the ISCABBS client 1.5.1
(stdio patch) by Serendipity.

Project page: <https://github.com/StilgarISCA/IoErrorBbsClient>
ISCA BBS site: <https://iscabbs.rocks/>

## Historical Contributors

- Michael Hampton (IO ERROR)
- Doug Siebert (Serendipity)
- Marc Dionne (Marco Polo)
- David Bailey
- Dave Lacey
- Dave (Isoroku)
- Client 9 / Blackout Group

More background is documented in `history.md`.

## Quick Start

Install test and analysis dependencies first:

```bash
brew install cmocka cppcheck llvm pkg-config
```

`make check` builds and runs the cmocka unit tests only when `cmocka` is
available through `pkg-config`.

```bash
make clean
autoreconf -i
./configure
make -j4
make check
make cppcheck
make scan-build
```

## Platform Support

- macOS is the primary target
- Other Unix-like systems receive least-effort support

### Build Modes

The default configure path produces a development build.

- Dev build is the default
- Dev build uses debug-friendly flags and sanitizers
- Supported builds still add host-appropriate tuning flags by default:
  - Apple Silicon builds use Apple Silicon tuning flags
  - Intel builds use Intel tuning flags

Default dev build:

```bash
make clean
autoreconf -i
./configure
make -j4
make check
make cppcheck
make scan-build
```

Release build:

```bash
make clean
autoreconf -i
./configure --enable-release-build
make -j4
make check
make cppcheck
make scan-build
```

Release package:

```bash
make clean
autoreconf -i
./configure --enable-release-build
make -j4
make release-package
```

`make release-package` creates a stripped release binary under `release/`.
On macOS, it also keeps a matching `.dSYM` bundle for postmortem debugging.

Optional macOS Keychain build:

```bash
make clean
autoreconf -i
./configure --enable-keychain
make -j4
make check
make cppcheck
```

Keychain-enabled builds can store passwords in macOS Keychain. The runtime
setting is written as `use_keychain = true` or `use_keychain = false` in
`~/.config/bbs/config.toml`, or in `$XDG_CONFIG_HOME/bbs/config.toml` when
`XDG_CONFIG_HOME` is set.

## Configuration File

The client stores settings in a TOML file at:

- `~/.config/bbs/config.toml`
- `$XDG_CONFIG_HOME/bbs/config.toml` when `XDG_CONFIG_HOME` is set and non-empty

The legacy `~/.bbsrc` and `~/.bbsfriends` files are no longer read or written.

Examples:

```toml
[connection]
host = "bbs.iscabbs.com"
port = 23

[behavior]
screen_reader_mode = false
update_title_bar = true

[local_command_keys]
command = "esc"
quit = "ctrl-d"
```

Boolean settings use explicit TOML values: `true` and `false`.

## Shell And Editor Commands

The external editor setting can use a normal command with optional arguments.
The shell hotkey is configurable, but the shell command itself still comes from
`$SHELL` at runtime.

Examples that work:
- `/bin/zsh`
- `/opt/homebrew/bin/fish -l`
- `vim -f`
- `"path with spaces/editor" --wait`

Still not supported:
- pipes
- redirection
- command substitution
- random shell code

`$SHELL` and the configured editor should point to a real executable, with
optional arguments if needed. They should not be shell script fragments.

Release validation:

```bash
make clean
autoreconf -i
./configure --enable-release-build
make -j4
make check
make cppcheck
make scan-build
make distcheck
```

`make distcheck` validates that the distribution tarball can be packaged, built,
tested, installed, uninstalled, and cleaned successfully from a fresh
out-of-tree build.

Universal macOS build for both Apple Silicon and Intel Macs:

```bash
make clean
autoreconf -i
./configure --enable-universal-binary
make -j4
make check
make cppcheck
```

## Formatting and Linting

```bash
# Apply repository formatting rules (.clang-format)
/opt/homebrew/opt/llvm/bin/clang-format -i $(git ls-files '*.c' '*.h')

# Verify the default dev build
make clean
autoreconf -i
./configure
make -j4
make check
make cppcheck
make scan-build
```

`make scan-build` runs Clang Static Analyzer and writes HTML reports under
`scan-build-report/`. The target fails if the analyzer finds bugs.

Optional clang-tidy setup:

```bash
# Requires: brew install bear
#
# Generate/refresh compile_commands.json for clang-tidy
make compile-commands

# Add required braces to control statements
/opt/homebrew/opt/llvm/bin/clang-tidy -p . -fix -fix-errors -format-style=file \
  -checks='-*,readability-braces-around-statements' \
  $(git ls-files '*.c')
```

## License

- See `LICENSE` for the GNU GPL v2.0-or-later terms used by this project.
