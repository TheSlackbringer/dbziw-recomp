#!/usr/bin/env bash
# Runs a Windows program from an llvm-mingw cross-build under Wine.
set -euo pipefail
root=${LLVM_MINGW_ROOT:-$HOME/.local/opt/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64}
export WINEPREFIX=${DBZIW_WINEPREFIX:-$HOME/.local/opt/wine-dbziw-tests}
export WINEDEBUG=${WINEDEBUG:--all}
export WINEPATH="$root/x86_64-w64-mingw32/bin"
exec wine "$@"
