#!/usr/bin/env python3
"""
Patch compile_commands.json so Homebrew clang-tidy can resolve macOS SDK headers.
"""

import json
import os
import subprocess
import sys
from pathlib import Path


def sdk_path() -> str | None:
    if sys.platform != "darwin":
        return None

    result = subprocess.run(
        ["xcrun", "--sdk", "macosx", "--show-sdk-path"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None

    path = result.stdout.strip()
    return path or None


def needs_sysroot(arguments: list[str]) -> bool:
    return "-isysroot" not in arguments


def patch_arguments(arguments: list[str], sdkroot: str | None) -> list[str]:
    if sdkroot is None or not needs_sysroot(arguments):
        return arguments

    compiler_index = 1 if arguments and arguments[0] == "ccache" else 0
    if compiler_index > len(arguments):
        compiler_index = 0

    insert_index = compiler_index + 1
    patched = list(arguments)
    patched[insert_index:insert_index] = ["-isysroot", sdkroot]
    return patched


def main() -> int:
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("compile_commands.json")
    if not path.exists():
        print(f"missing {path}", file=sys.stderr)
        return 1

    data = json.loads(path.read_text())
    sdkroot = sdk_path()
    updated = False

    for entry in data:
        arguments = entry.get("arguments")
        if not isinstance(arguments, list):
            continue

        patched = patch_arguments(arguments, sdkroot)
        if patched != arguments:
            entry["arguments"] = patched
            entry.pop("command", None)
            updated = True

    if updated:
        path.write_text(json.dumps(data, indent=2) + "\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
