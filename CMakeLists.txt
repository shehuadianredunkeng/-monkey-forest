cmake_minimum_required(VERSION 3.16)

project(monkey_forest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(monkey_map src/Room.cpp)
target_include_directories(monkey_map PUBLIC include)

add_library(monkey_player
    src/Item.cpp
    src/Inventory.cpp
    src/Player.cpp
    src/PlayerActions.cpp
)
target_include_directories(monkey_player PUBLIC include)

add_library(monkey_npc_combat
    src/Character.cpp
    src/CollectionSystem.cpp
    src/Enemy.cpp
    src/NPCSystem.cpp
    src/CombatSystem.cpp
)
target_include_directories(monkey_npc_combat PUBLIC include)
target_link_libraries(monkey_npc_combat PUBLIC monkey_player)

add_library(monkey_story
    src/Event.cpp
    src/EventFactory.cpp
    src/EventSystem.cpp
    src/StoryText.cpp
)
target_include_directories(monkey_story PUBLIC include)
target_link_libraries(monkey_story PUBLIC monkey_player)

add_library(monkey_state
    src/EndingSystem.cpp
    src/ProgressSystem.cpp
    src/SaveManager.cpp
    src/StatusView.cpp
    src/WorldState.cpp
)
target_include_directories(monkey_state PUBLIC include)
target_link_libraries(monkey_state PUBLIC monkey_player)

add_library(monkey_ui
    src/UI/TextLayout.cpp
    src/UI/ConsoleRenderer.cpp
    src/UI/GameUI.cpp
    src/ConsoleUI.cpp
)
target_include_directories(monkey_ui PUBLIC include)
target_link_libraries(monkey_ui PUBLIC monkey_map monkey_player monkey_state monkey_npc_combat)

add_executable(monkey_forest_first_year src/main.cpp)
target_link_libraries(monkey_forest_first_year PRIVATE
    monkey_ui monkey_map monkey_player monkey_npc_combat monkey_story monkey_state)

add_executable(ui_layout_tests tests/ui_layout_test.cpp)
target_link_libraries(ui_layout_tests PRIVATE monkey_ui)

# Native screen-buffer verification needs an attached Windows console; not a CI pipe.
if(WIN32)
    add_executable(ui_windows_smoke tests/ui_windows_smoke.cpp)
    target_link_libraries(ui_windows_smoke PRIVATE monkey_ui)
    if(MSVC)
        target_compile_options(ui_windows_smoke PRIVATE /utf-8)
    endif()
endif()

add_executable(map_tests
    tests/TestMain.cpp
    tests/TestSupport.cpp
    tests/RoomTests.cpp
)
target_link_libraries(map_tests PRIVATE monkey_map)

add_executable(member4_tests tests/member4_player_test.cpp)
target_link_libraries(member4_tests PRIVATE monkey_player monkey_map)

add_executable(member3_tests tests/member3_npc_combat_test.cpp)
target_link_libraries(member3_tests PRIVATE monkey_npc_combat monkey_player monkey_map)

add_executable(member2_tests tests/member2_event_test.cpp)
target_link_libraries(member2_tests PRIVATE
    monkey_story monkey_npc_combat monkey_player monkey_map monkey_state)

add_executable(member5_tests tests/member5_state_save_test.cpp)
target_link_libraries(member5_tests PRIVATE
    monkey_state monkey_story monkey_player monkey_map)

if(MSVC)
    target_compile_options(monkey_ui PRIVATE /utf-8)
    target_compile_options(ui_layout_tests PRIVATE /utf-8)
    target_compile_options(monkey_map PRIVATE /utf-8)
    target_compile_options(monkey_player PRIVATE /utf-8)
    target_compile_options(map_tests PRIVATE /utf-8)
    target_compile_options(member4_tests PRIVATE /utf-8)
    target_compile_options(monkey_npc_combat PRIVATE /utf-8)
    target_compile_options(member3_tests PRIVATE /utf-8)
    target_compile_options(member2_tests PRIVATE /utf-8)
    target_compile_options(member5_tests PRIVATE /utf-8)
    target_compile_options(monkey_story PRIVATE /utf-8)
    target_compile_options(monkey_state PRIVATE /utf-8)
    target_compile_options(monkey_forest_first_year PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME ui_layout_tests COMMAND ui_layout_tests)
add_test(NAME map_tests COMMAND map_tests)
add_test(NAME member4_tests COMMAND member4_tests)
add_test(NAME member3_tests COMMAND member3_tests)
add_test(NAME member2_tests COMMAND member2_tests)
add_test(NAME member5_tests COMMAND member5_tests)
