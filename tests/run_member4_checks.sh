#!/usr/bin/env bash
# Usage: bash tests/run_member4_checks.sh BUILD_DIR [COMPLETE_MEMBER5_PROJECT_DIR]
# Only build artifacts are written; the supplied integration project is read-only.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "Usage: $0 BUILD_DIR [COMPLETE_MEMBER5_PROJECT_DIR]" >&2
    exit 2
fi

repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p -- "$1"
build_dir=$(cd -- "$1" && pwd)
mkdir -p -- "$build_dir/tmp"
export TMPDIR="$build_dir/tmp"
compiler=${CXX:-g++}
flags=(-std=c++17 -Wall -Wextra -Wpedantic -Werror)
if [[ ${MEMBER4_SANITIZE:-0} == 1 ]]; then
    flags+=(-fsanitize=undefined -fno-sanitize-recover=all -g)
fi

"$compiler" "${flags[@]}" -I"$repo_root/include" -I"$repo_root/tests" \
    "$repo_root/src/Room.cpp" "$repo_root/tests/TestMain.cpp" \
    "$repo_root/tests/TestSupport.cpp" "$repo_root/tests/RoomTests.cpp" \
    -o "$build_dir/map_tests"
"$build_dir/map_tests"

player_sources=("$repo_root/src/Item.cpp" "$repo_root/src/Inventory.cpp"
                "$repo_root/src/Player.cpp" "$repo_root/src/PlayerActions.cpp")
"$compiler" "${flags[@]}" -I"$repo_root/include" "${player_sources[@]}" \
    "$repo_root/src/Room.cpp" "$repo_root/tests/member4_player_test.cpp" \
    -o "$build_dir/member4_tests"
"$build_dir/member4_tests"

if [[ $# == 1 ]]; then
    echo "Member-4 branch checks passed (integration checks not requested)."
    exit 0
fi

integration_dir=$(cd -- "$2" && pwd)
mkdir -p -- "$build_dir/integration-objects"
objects=()
for source in "$integration_dir"/src/*.cpp; do
    name=${source##*/}
    case "$name" in main.cpp|PlayerActions.cpp) continue ;; esac
    object="$build_dir/integration-objects/${name%.cpp}.o"
    "$compiler" "${flags[@]}" -I"$integration_dir/include" -c "$source" -o "$object"
    objects+=("$object")
done
"$compiler" "${flags[@]}" -I"$integration_dir/include" \
    -c "$repo_root/src/PlayerActions.cpp" -o "$build_dir/integration-objects/PlayerActions.o"
objects+=("$build_dir/integration-objects/PlayerActions.o")

for suite in map member2 member3 member4 member5 main; do
    defines=()
    case "$suite" in
        map) sources=("$integration_dir/tests/TestMain.cpp" "$integration_dir/tests/TestSupport.cpp"
                      "$integration_dir/tests/RoomTests.cpp") ;;
        member2) sources=("$integration_dir/tests/member2_event_test.cpp") ;;
        member3) sources=("$integration_dir/tests/member3_npc_combat_test.cpp") ;;
        member4) sources=("$repo_root/tests/member4_player_test.cpp")
                 defines=(-DMEMBER4_USE_REAL_WORLD) ;;
        member5) sources=("$integration_dir/tests/member5_state_save_test.cpp") ;;
        main) sources=("$repo_root/tests/member4_main_test.cpp") ;;
    esac
    "$compiler" "${flags[@]}" "${defines[@]}" -I"$integration_dir/include" \
        "${sources[@]}" "${objects[@]}" -o "$build_dir/integration_${suite}_tests"
    "$build_dir/integration_${suite}_tests"
    echo "Integration suite passed: $suite"
done

"$compiler" "${flags[@]}" -I"$integration_dir/include" \
    "$repo_root/src/main.cpp" "${objects[@]}" -o "$build_dir/monkey_forest_game"
echo "All checks passed; game executable: $build_dir/monkey_forest_game"
