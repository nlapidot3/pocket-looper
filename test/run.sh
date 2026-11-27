#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" "$root/test/test_looper.c" "$root/main/looper.c" -o "$build/test_looper"
"$build/test_looper"
