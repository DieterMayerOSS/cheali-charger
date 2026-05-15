cmake_minimum_required(VERSION 3.20)
project(cheali-sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(sim_algorithms STATIC
    algorithms/find_min_cell.cpp
    algorithms/thevenin.cpp
    algorithms/balancer_logic.cpp
    algorithms/monitor_eta.cpp
)
target_include_directories(sim_algorithms PUBLIC algorithms)

enable_testing()

add_executable(test_find_min_cell tests/test_find_min_cell.cpp)
target_link_libraries(test_find_min_cell PRIVATE sim_algorithms)
add_test(NAME find_min_cell COMMAND test_find_min_cell)

add_executable(test_thevenin tests/test_thevenin.cpp)
target_link_libraries(test_thevenin PRIVATE sim_algorithms)
add_test(NAME thevenin COMMAND test_thevenin)

add_executable(test_balancer_logic tests/test_balancer_logic.cpp)
target_link_libraries(test_balancer_logic PRIVATE sim_algorithms)
add_test(NAME balancer_logic COMMAND test_balancer_logic)

add_executable(test_monitor_eta tests/test_monitor_eta.cpp)
target_link_libraries(test_monitor_eta PRIVATE sim_algorithms)
add_test(NAME monitor_eta COMMAND test_monitor_eta)
