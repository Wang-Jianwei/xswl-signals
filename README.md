# xswl-signals

Header-only, lightweight signals/slots library for modern C++ (C++11). Provides scoped connections, connection groups, priorities, single-shot slots, and member-function binding with lifetime safety.

## Features
- Header-only: just add the `include/` directory and link the interface target `xswl_signals`.
- Connection management: block/unblock, scoped connections, connection groups, tag-based disconnect.
- Prioritized dispatch: larger priority values execute first; stable order for equal priorities.
- Single-shot slots: `connect_once` removes the slot after first invocation.
- Thread-friendly basics: uses atomics where needed; suitable for typical single-threaded and modest multithreaded use.

## Requirements
- CMake >= 3.15
- C++11 compiler (tested with GCC/Clang)

## Build & Test
```bash
./build.sh                  # configure & build (Release)
./build.sh build --debug    # Debug build
./build.sh test             # run ctest
./build.sh help             # show help
```

## Quick Start
```cpp
#include "xswl/signals.hpp"

int main() {
	xswl::signal_t<int> on_value;

	auto c1 = on_value.connect([](int v) {
		// regular slot
	});

	on_value.connect_once([](int v) {
		// runs only once
	});

	on_value.connect([](int v) {
		// higher priority executes first
	}, /*priority*/ 50);

	emit on_value(42);
	c1.disconnect();
}
```

Integrate with CMake (in your project):
```cmake
add_subdirectory(path/to/xswl-signals)
target_link_libraries(your_target PRIVATE xswl_signals)
```

## Examples
After `./build.sh build`, run:
- `./build/examples/signals_basic`
- `./build/examples/signals_lifecycle`

Example sources:
- [examples/basic.cpp](examples/basic.cpp)
- [examples/lifecycle.cpp](examples/lifecycle.cpp)

## Tests
`./build.sh test` runs `ctest` for:
- SignalsBaseTest
- SignalsStrictTest

## Project Layout
- [include/xswl/signals.hpp](include/xswl/signals.hpp) — single-header library
- [tests](tests) — self-contained test executables
- [examples](examples) — runnable usage samples