#!/usr/bin/env bash

# By Gabriel Staples
# For documentation on this file, see my answer here:
# [my answer] How to call clang-format over a cpp project folder?:
# https://stackoverflow.com/a/65988393/4561887

# See my ans: https://stackoverflow.com/a/60157372/4561887
FULL_PATH_TO_SCRIPT="$(realpath "$0")"
SCRIPT_DIRECTORY="$(dirname "$FULL_PATH_TO_SCRIPT")/../"

RETURN_CODE_SUCCESS=0
RETURN_CODE_ERROR=1

# Find all files in SCRIPT_DIRECTORY with one of these extensions
FILE_LIST="$(find "$SCRIPT_DIRECTORY" | grep -E ".*\.(ino|cpp|c|h|hpp|hh)$")"
# echo "\"$FILE_LIST\"" # debugging
# split into an array; see my ans: https://stackoverflow.com/a/71575442/4561887
# mapfile -t FILE_LIST_ARRAY <<< "$FILE_LIST"
IFS=$'\n' read -r -d '' -a FILE_LIST_ARRAY <<< "$FILE_LIST"

num_files="${#FILE_LIST_ARRAY[@]}"
echo -e "$num_files files found to format:"
if [ "$num_files" -eq 0 ]; then
    echo "Nothing to do."
    exit $RETURN_CODE_SUCCESS
fi

# print the list of all files
for i in "${!FILE_LIST_ARRAY[@]}"; do
    file="${FILE_LIST_ARRAY["$i"]}"
    printf "  %2i: %s\n" $((i + 1)) "$file"
done
echo ""

format_files="false"
# See: https://stackoverflow.com/a/226724/4561887
read -p "Do you wish to auto-format all of these files [y/N] " user_response
case "$user_response" in
    [Yy]* ) format_files="true"
esac

if [ "$format_files" = "false" ]; then
    echo "Aborting."
    exit $RETURN_CODE_SUCCESS
fi

# Format each file.
clang-format --verbose -i --style=file "${FILE_LIST_ARRAY[@]}"
