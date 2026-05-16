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
    algorithms/charge_strategy_decisions.cpp
    algorithms/monitor_safety.cpp
    algorithms/calibration.cpp
    algorithms/thevenin_fsm.cpp
)
target_include_directories(sim_algorithms PUBLIC algorithms)

enable_testing()

# All tests use plain assert() from <cassert>. Release-style builds
# define NDEBUG, which would silently turn every check into a no-op.
# Drop NDEBUG on test targets so asserts always fire, no matter the
# CMAKE_BUILD_TYPE.
function(cheali_sim_test name)
    add_executable(test_${name} tests/test_${name}.cpp)
    target_link_libraries(test_${name} PRIVATE sim_algorithms)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /U NDEBUG)
    else()
        target_compile_options(test_${name} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
endfunction()

cheali_sim_test(find_min_cell)
cheali_sim_test(thevenin)
cheali_sim_test(balancer_logic)
cheali_sim_test(monitor_eta)
cheali_sim_test(charge_strategy_decisions)
cheali_sim_test(monitor_safety)
cheali_sim_test(calibration)
cheali_sim_test(thevenin_fsm)
