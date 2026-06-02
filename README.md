# IO ERROR BBS Client

This is Stilgar's fork of IO ERROR's BBS Client, which itself was based on the ISCABBS client 1.5.1 (stdio patch) by Serendipity.

- Client project page: <https://github.com/StilgarISCA/IoErrorBbsClient>
- The best ISCA BBS site: <https://iscabbs.rocks/>

See [HISTORY.md](HISTORY.md) for 3.0.0-Stilgar highlights and prior release history.

## Install

Install with Homebrew on macOS:

```bash
brew tap StilgarISCA/tap
brew install bbsclient
```

The installed command is:

```bash
bbs
```

## Platform Support

- macOS is the primary target of this fork.
  - The compiler optimizes for Apple Silicon and Intel.
  - Universal binaries can also be built.
- Other Unix-like systems receive least-effort support.

## Configuration

Client settings are saved in a TOML file at:

- `~/.config/bbs/config.toml`
- `$XDG_CONFIG_HOME/bbs/config.toml` when `XDG_CONFIG_HOME` is set and non-empty

Settings files (`.bbsrc`) from older client builds are not imported, updated, or removed.

Set `pane_ui = true` in the `[behavior]` config section to enable the
experimental wide-terminal pane layout. When the terminal is wide enough, the
client keeps the normal 80-column BBS session on the left and shows a buffered
sidebar on the right. Narrow terminals and screen-reader mode continue to use
the classic view.

At a normal BBS command prompt, these commands populate the sidebar instead of
printing their output in the left pane:

- `W`: online users
- `w`: online users when lowercase `w` is mapped to the long Who command
- `?`: command help
- `@`: Sysops, Programmers, and Roomaides
- `i` or `I`: information for the current forum
- `n`: next post when used at a room prompt

The online-user view refreshes automatically every five seconds while it is
selected. Other views remain pinned until another sidebar command is used. Long
responses advance through BBS `MORE` prompts automatically and can be scrolled
with the mouse wheel over the sidebar. Use the mouse wheel over the left pane to
scroll through up to 1,000 retained rows without moving the sidebar. Sidebar
lines are clipped to the available width.

## Shell And Editor Commands

The external editor setting can use a normal command with optional arguments. By default, it uses the value of the `EDITOR` environment variable.

The shell hotkey is configurable, but the shell command itself comes from `$SHELL` at runtime.

Supported command forms include:

- `/bin/zsh`
- `/opt/homebrew/bin/fish -l`
- `vim -f`
- `"path with spaces/editor" --wait`

Do not use pipes, redirection, command substitution, or arbitrary shell code in configured commands.

## Developer Build

Install build, test, and analysis dependencies:

```bash
brew install cmocka cppcheck llvm pkg-config
```

Build and test:

```bash
make clean
autoreconf -i
./configure
make -j4
make check
make cppcheck
```

`make check` builds and runs the cmocka unit tests only when `cmocka` is available through `pkg-config`.

Run `./configure --help` for optional build flags.

## Formatting And Static Analysis

Apply repository formatting rules:

```bash
clang-format -i $(git ls-files '*.c' '*.h')
```

Run Clang Static Analyzer:

```bash
make scan-build
```

`make scan-build` writes HTML reports under `scan-build-report/` and fails if the analyzer finds bugs. The project also supports clang-tidy through `compile_commands.json`; `make compile-commands` generates it and requires `bear`.

## Historical Contributors

- Michael Hampton (IO ERROR)
- Doug Siebert (Serendipity)
- Marc Dionne (Marco Polo)
- David Bailey
- Dave Lacey
- Dave (Isoroku)
- Client 9 / Blackout Group

More background is documented in [HISTORY.md](HISTORY.md).

## License

See [LICENSE](LICENSE) for the GNU GPL v2.0-or-later terms used by this project.
