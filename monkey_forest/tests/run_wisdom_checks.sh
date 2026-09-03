#!/usr/bin/env bash
# Direct C++17 build fallback. Run from any directory; no source-tree artifacts.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    printf 'Usage: bash %s /absolute/build-directory\n' "$0" >&2
    exit 2
fi

wisdom_project_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
mkdir -p "$1"
wisdom_build_dir=$(cd "$1" && pwd -P)
case "$wisdom_build_dir/" in
    "$wisdom_project_dir/"*)
        printf 'Use a build directory outside the project source tree.\n' >&2
        exit 2
        ;;
esac

wisdom_run_dir=$(mktemp -d "$wisdom_build_dir/run.XXXXXXXX")
mkdir -p "$wisdom_run_dir/compiler-temp"
export TMPDIR="$wisdom_run_dir/compiler-temp"
wisdom_compiler=${CXX:-g++}
wisdom_options=(-std=c++17 -Wall -Wextra -Wpedantic -Werror -I"$wisdom_project_dir/include")
if [ "${WISDOM_SANITIZE:-0}" = 1 ]; then
    wisdom_options+=(-fsanitize=undefined -fno-sanitize-recover=undefined -g)
fi

wisdom_objects=()
for wisdom_source in "$wisdom_project_dir"/src/*.cpp; do
    if [ "$(basename "$wisdom_source")" = main.cpp ]; then continue; fi
    wisdom_object="$wisdom_run_dir/$(basename "${wisdom_source%.cpp}").o"
    "$wisdom_compiler" "${wisdom_options[@]}" -c "$wisdom_source" -o "$wisdom_object"
    wisdom_objects+=("$wisdom_object")
done

"$wisdom_compiler" "${wisdom_options[@]}" "$wisdom_project_dir/src/main.cpp" \
    "${wisdom_objects[@]}" -o "$wisdom_run_dir/monkey_forest_game"

"$wisdom_compiler" "${wisdom_options[@]}" \
    "$wisdom_project_dir/tests/TestMain.cpp" \
    "$wisdom_project_dir/tests/TestSupport.cpp" \
    "$wisdom_project_dir/tests/RoomTests.cpp" \
    "${wisdom_objects[@]}" -o "$wisdom_run_dir/map_tests"
"$wisdom_run_dir/map_tests"
printf 'PASS map_tests\n'

for wisdom_suite in member2_event member3_npc_combat member4_player member5_state_save wisdom_growth wisdom_main; do
    "$wisdom_compiler" "${wisdom_options[@]}" \
        "$wisdom_project_dir/tests/${wisdom_suite}_test.cpp" \
        "${wisdom_objects[@]}" -o "$wisdom_run_dir/${wisdom_suite}_tests"
    "$wisdom_run_dir/${wisdom_suite}_tests"
    printf 'PASS %s\n' "$wisdom_suite"
done

printf 'All seven test programs passed. Game executable: %s/monkey_forest_game\n' "$wisdom_run_dir"
