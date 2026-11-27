#!/bin/sh
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" "$root/test/test_looper.c" "$root/main/looper.c" -o "$build/test_looper"
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" "$root/test/test_battery_level.c" "$root/main/battery_level.c" -o "$build/test_battery_level"
"$build/test_battery_level"
"$build/test_looper"
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" "$root/test/test_battery_level.c" "$root/main/battery_level.c" -o "$build/test_battery_level"
"$build/test_battery_level"
