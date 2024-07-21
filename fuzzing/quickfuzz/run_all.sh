#!/bin/sh
#
# invoked from github action.
# runs all fuzzers using all cores for a limited time

set -eu

SCRIPTDIR=$(dirname "$0")

ls fuzzing/fuzz_*|sort --random-sort | \
   xargs --verbose --max-procs $(nproc) -n 1 "$SCRIPTDIR"/run_single.sh
