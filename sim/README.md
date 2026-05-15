# sim/ — Host-side build for learning & testing

A standalone CMake project that compiles and tests algorithms from
the cheali-charger firmware on the host machine (no AVR/ARM toolchain
needed). Useful for:

- understanding the charging math (Thevenin model, balancer logic)
- writing tests for pure-logic functions
- experimenting with refactorings before touching real firmware

This directory is independent of the main firmware build. Nothing here
gets flashed to hardware.

## Layout

```
sim/
├── CMakeLists.txt      # standalone host build
├── algorithms/         # ported / reimplemented algorithms (pure C++)
├── tests/              # unit tests (no framework — plain asserts)
└── README.md
```

## Build (MSYS2 UCRT64)

From a UCRT64 shell, or via PowerShell with the toolchain on PATH:

```
cmake -S sim -B sim/build -G Ninja
cmake --build sim/build
ctest --test-dir sim/build --output-on-failure
```
