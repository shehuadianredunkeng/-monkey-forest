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

add_executable(map_tests
    tests/TestMain.cpp
    tests/TestSupport.cpp
    tests/RoomTests.cpp
)
target_link_libraries(map_tests PRIVATE monkey_map)

add_executable(member4_tests tests/member4_player_test.cpp)
target_link_libraries(member4_tests PRIVATE monkey_player monkey_map)

if(MSVC)
    target_compile_options(monkey_map PRIVATE /utf-8)
    target_compile_options(monkey_player PRIVATE /utf-8)
    target_compile_options(map_tests PRIVATE /utf-8)
    target_compile_options(member4_tests PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME map_tests COMMAND map_tests)
add_test(NAME member4_tests COMMAND member4_tests)
