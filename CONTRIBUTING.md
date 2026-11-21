# Contributing to Dalia

## Building & Testing

### Prerequisites

- CMake 3.25 or newer
- A C++20 compatible compiler (GCC or MSVC)
- Ninja or Visual Studio 2026 (for build presets)

### Building

Using the already configured presets in `CMakePresets.json` is the fastest way to build the project.

```bash
# Debug (Ninja)
cmake --workflow --preset config-build-debug

# Release (Ninja)
cmake --workflow --preset config-build-release
```

To generate a Visual Studio solution:

```bash
cmake --preset vs-win-x64
```

Or if you prefer to not use presets:

```bash
cmake -B build
cmake --build build --config Release
```

### Build Options

When building Dalia as a top-level project, the sandbox application, tests and profiling are enabled by default. You can
toggle components with `-D<OPTION>=ON/OFF`:

| Option                 | Default | Description                            |
|:-----------------------|:--------|:---------------------------------------|
| `DALIA_BUILD_TESTS`    | `ON`    | Build the Dalia test suite             |
| `DALIA_BUILD_SANDBOX`  | `ON`    | Build the Dalia Engine sandbox         |
| `DALIA_BUILD_STUDIO`   | `OFF`   | Build the Dalia Studio editor          |
| `DALIA_PROFILE_ENABLE` | `ON`    | Enable Tracy profiling in Dalia Engine |

### Running Tests

```bash
ctest --test-dir build --output-on-failure
```

Please make sure that all tests pass and that any new features are accompanied by appropriate tests before submitting
a pull request.

## Code Style

### Formatting

| Rule                   | Convention                                       |
|:-----------------------|:-------------------------------------------------|
| File extensions        | `.hpp` for headers, `.cpp` for sources           |
| Header Guards          | `#pragma once`                                   |
| Includes (project)     | `#include ""`                                    |
| Includes (third-party) | `#include <>`                                    |
| Indentations           | Tabs (width 4)                                   |
| Braces                 | Opening `{` on the same line, `else` on new line |
| Pointers & References  | Attach to Type: `Type* var`, `Type& var`         |

A `.clang-format` file that can be used for auto-formatting can be found in the root directory of the project.

### Naming Conventions

| Element                              | Convention       |
|:-------------------------------------|:-----------------|
| Classes, Structs, Enums              | PascalCase       |
| Functions & Methods                  | PascalCase       |
| Member Variables (private/protected) | m_camelCase      |
| Member Variables (public)            | camelCase        |
| Parameters & Local Variables         | camelCase        |
| Constants                            | UPPER_SNAKE_CASE |

### Real-Time Safety

Since Dalia is a real-time audio engine, it makes use of time-critical, high-priority threads. To avoid priority
inversions and audio buffer underruns, it is very important to follow some simple rules when writing code for these
threads:

- No heap allocations.
- No blocking locks such as mutexes or condition variables.
- No system calls.

In addition to this, the Dalia engine API should be lock-free and until it has its own intenral memory allocators, most
heap-memory should be pre-allocated at engine initialization.

## AI Policy

You are welcome to use AI tools to assist you while programming, subject to the following rules:

- All contributions (issues, pull requests, code) must be human-driven. Do not use autonomous agents in this repository.
- You are responsible for every line of code you have submitted.

## Git Workflow

- `main` is for stable releases only, do not open pull requests against this branch.
- `dev` is the active development branch. All pull requests should target this branch.

### Submitting a Pull Request

1. Open an issue for new features or large changes so that they can be reviewed and discussed before being implemented.
   Smaller changes such as bugfixes and typos can bypass this requirement.
2. Fork the repository and create a new branch from `dev`:
    - `feature/` for new features
    - `fix/` for bugfixes
3. Make sure your changes follow the code style, have corresponding tests, and pass all existing tests.
4. Open a pull request targeting `dev` in which a clear description includes what changed and why.

## License

By contributing to Dalia, you agree that your contributions will be licensed under the project's existing license.