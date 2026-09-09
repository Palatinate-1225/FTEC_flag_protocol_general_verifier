#!/usr/bin/env bash
#
# Build CUDD in place, for cmake/SPBDD.cmake.
#
#   build_cudd.sh <cudd source dir> [jobs]
#
# CUDD is autotools and ships a generated ./configure, so upstream's own
# instructions are just configure && make. That does not survive a git
# checkout: git gives every file the same mtime, so make cannot tell that
# aclocal.m4 is newer than configure.ac and tries to regenerate it with
# aclocal-1.14 -- the exact automake the release tarball was made with, which
# is not on any machine built after about 2015. Regenerating the whole build
# system with whatever autotools *are* installed is the fix, and is why this
# needs autoconf/automake/libtool rather than only a compiler.
set -euo pipefail

src=${1:?usage: build_cudd.sh <cudd source dir> [jobs]}
jobs=${2:-1}
lib="$src/cudd/.libs/libcudd.a"

if [ -f "$lib" ]; then
    echo "CUDD: $lib is already built"
    exit 0
fi

for tool in autoreconf automake libtoolize make; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "CUDD needs $tool to build. On Debian/Ubuntu:" >&2
        echo "    sudo apt install -y build-essential autoconf automake libtool" >&2
        echo "On macOS with Homebrew:" >&2
        echo "    brew install autoconf automake libtool" >&2
        echo "Or configure with -DFTEC_ENABLE_SPBDD=OFF to skip the spbdd backend." >&2
        exit 1
    }
done

cd "$src"
autoreconf -fi
./configure --enable-obj
make -j"$jobs"

test -f "$lib" || {
    echo "CUDD build finished but $lib is missing" >&2
    exit 1
}
echo "CUDD: built $lib"
