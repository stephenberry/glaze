#!/bin/sh
#
# builds and runs the fuzzers for a short while.
#
# intended to be run manually during development. gitlab CI has it's own
# script.
#

set -eu

SCRIPTDIR=$(dirname "$0")

builddir=build-fuzzer-clang

cmake -B "$builddir" -S "$SCRIPTDIR/.." -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=Off -GNinja -DCMAKE_CXX_COMPILER=/usr/bin/clang++-17
cmake --build "$builddir"

cd "$builddir"
mkdir -p corpus

fuzzers=$(ls fuzzing/fuzz_*|sort --random-sort)

export UBSAN_OPTIONS=halt_on_error=1
export ASAN_OPTS=halt_on_error=1

for fuzzer in $fuzzers ; do
  echo looking at $fuzzer
  shortname=$(basename $fuzzer)
  mkdir -p corpus/$shortname
  $fuzzer -max_total_time=60 corpus/$shortname
done
