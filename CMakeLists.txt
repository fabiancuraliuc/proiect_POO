cmake_minimum_required(VERSION 3.12)
project(SimulatorAuto VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NCURSESW REQUIRED ncursesw)

add_executable(TrafficSimulator
    src/main.cpp
    src/ncurses_wrapper.cpp
    src/entity_manager.cpp
    src/game_state.cpp
    src/rules_engine.cpp
)

target_include_directories(TrafficSimulator PRIVATE
    ${NCURSESW_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(TrafficSimulator PRIVATE ${NCURSESW_LIBRARIES})
target_compile_options(TrafficSimulator PRIVATE ${NCURSESW_CFLAGS_OTHER} -Wall -Wextra -O2)
