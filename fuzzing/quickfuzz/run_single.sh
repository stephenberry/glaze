#!/bin/sh
#
# invoked from run_all.sh
#
# runs a single fuzzer a limited time.
# if no problems are found, the results are removed.
# problems are found, logs and artifacts are left and the exit
# status is nonzero.

set -eu

fuzzer="$1"

if [ ! -x "$fuzzer" ] ; then
 exit 1
fi

shortname=$(basename "$fuzzer")

ARTIFACTS=artifacts/$shortname
CORPUS=$ARTIFACTS/corpus
LOG=$ARTIFACTS/$shortname.log

mkdir -p $ARTIFACTS $CORPUS

export UBSAN_OPTIONS=halt_on_error=1
export ASAN_OPTS=halt_on_error=1

if ! $fuzzer -max_total_time=40 -artifact_prefix=$ARTIFACTS/ $CORPUS >$LOG 2>&1 ; then
  echo FAIL - fuzzer $shortname failed - see $LOG
  exit 1
fi

echo OK - fuzzer $shortname ran without problems
rm -rf $ARTIFACTS/
