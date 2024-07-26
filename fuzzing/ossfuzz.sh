#!/bin/sh
#
# this is the entry point for build.sh on oss fuzz, so that changes in how to
# build the fuzzers does not require changes in oss fuzz but instead can
# be done in the glaze repository.

set -eux

$CXX --version

for SRCFILE in $(ls fuzzing/*.cpp |grep -v -E '(exhaustive|main\.cpp)'); do
    NAME=$(basename $SRCFILE .cpp)
    $CXX $CXXFLAGS -std=c++23 -g -Iinclude \
         $SRCFILE -o $OUT/$NAME \
         $LIB_FUZZING_ENGINE
done
