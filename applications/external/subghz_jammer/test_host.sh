#!/usr/bin/env bash
# Host unit-test runner for the Sub-Ghz jammer pure-math modules.
#
# Compiles a test file + its Furi-free source(s) with the host gcc and runs
# the result, so the scheduling math is validated on the host BEFORE it ships
# on device. Invoked by every stage to prove a feature before the next begins.
#
# Usage: ./test_host.sh tests/test_<name>.c [source.c ...]
#
# Example:
#   ./test_host.sh tests/test_jammer_freq.c jammer_freq.c
set -u

# MSYS2 gcc (ucrt64). Native `gcc` may be missing or the wrong build.
export PATH=/c/msys64/ucrt64/bin:$PATH

# Run from the app root so `-I.` finds the quoted "#include \"jammer_freq.h\"".
cd "$(dirname "$0")"

# gcc needs a WRITABLE temp dir for its intermediate files; the system default
# (C:\WINDOWS) is not user-writable ("Cannot create temporary file in C:\WINDOWS").
# Point it at a fresh local dir under build/ (auto-removed on exit).
mkdir -p build
tmpdir="$(mktemp -d "build/test-tmp.XXXXXX" 2>/dev/null)" || tmpdir="build/test-tmp-$$"
export TEMP="$tmpdir"
export TMP="$tmpdir"
export TMPDIR="$tmpdir"

# Output binary goes to ./build (stable across runs; NOT /tmp, which this host
# cleans out from under us).
test_file="$1"
shift
out="build/$(basename "$test_file" .c)"

cleanup() { rm -rf "$tmpdir"; }
trap cleanup EXIT

if ! gcc -I. -o "$out" "$test_file" "$@"; then
    echo "BUILD FAILED: gcc -I. -o \"$out\" \"$test_file\" $*" >&2
    exit 1
fi

"$out"
rc=$?
echo "exit: $rc"
exit "$rc"
