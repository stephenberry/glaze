#!/bin/sh
#
# minimizes and cleanses a crash.
# arg1 is the fuzzer
# arg2 is the crash case

usage() {
  echo "$0 fuzzer crashcase"
}
if [ $# -ne 2 ] ; then
usage
exit 1
fi

if [ ! -x "$1" ] ; then
echo "fuzzer should be passes as arg 1"
exit 1
fi

if [ ! -e "$2" ] ; then
echo "crash case should be passes as arg 2"
exit 1
fi

"$1" "$2" -minimize_crash=1 -exact_artifact_path=minimized_crash -max_total_time=30

"$1" minimized_crash -cleanse_crash=1 -exact_artifact_path=cleaned_crash


