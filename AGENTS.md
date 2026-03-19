# AGENTS.md - FaultLoggerd Development Guide

## Build System

This project uses GN (Generate Ninja) build system for OpenHarmony.

### Build Commands
```bash
# Build all targets
./build.sh --product-name <product> --build-target faultloggerd_targets

# Build all tests
./build.sh --product-name <product> --build-target faultloggerd_tests

# Run specific test modules
./build.sh --product-name <product> --build-target faultloggerd_unittest
./build.sh --product-name <product> --build-target faultloggerd_systemtest
./build.sh --product-name <product> --build-target faultloggerd_moduletest

# Run single unit test (after build)
./out/<product>/tests/unittest/faultloggerd/faultloggerd/dump_catcher/test_dumpcatcher
./out/<product>/tests/unittest/faultloggerd/faultloggerd/unwind/test_unwind
```

### Test Targets
- `faultloggerd_unittest` - Unit tests (gtest)
- `faultloggerd_systemtest` - System integration tests
- `faultloggerd_moduletest` - Module tests
- `faultloggerd_benchmarktest` - Performance benchmarks
- `faultloggerd_fuzzertest` - Fuzz tests

## Code Style Guidelines

### File Organization
- Source files: `snake_case.cpp` / `snake_case.h`
- Headers in `interfaces/innerkits/*/include/` for public APIs
- Implementation in corresponding directories
- Test files in `test/unittest/<module>/` matching source structure

### Naming Conventions
- Classes: `PascalCase` (e.g., `DfxDumpCatcher`, `FaultLoggerDaemon`)
- Functions/Methods: `PascalCase` (e.g., `DumpCatch`, `GetInstance`)
- Member variables: `trailing_underscore_` (e.g., `impl_`, `code_`)
- Constants: `UPPER_SNAKE_CASE` or `camelCase constexpr` (e.g., `DEFAULT_MAX_FRAME_NUM`)
- Namespaces: `OHOS::HiviewDFX`
- Enums: `PascalCase` with `UPPER_SNAKE_CASE` values (e.g., `UnwindErrorCode::UNW_ERROR_NONE`)

### Imports and Includes
- Use C++ style includes: `<cinttypes>`, `<cstdint>`, `<cstring>` instead of `<inttypes.h>`
- System headers first, then project headers
- Use `#include guards` with `FILENAME_H` pattern
- External dependencies: `hilog`, `jsoncpp`, `bounds_checking_function`, `c_utils`

### Formatting
- Apache 2.0 license header at top of all files
- 4-space indentation (no tabs)
- Opening brace on same line for functions/classes
- Line length: ~120 characters
- Delete copy constructors/assignment for singletons and non-copyable classes

### Types and Modern C++
- Use `std::shared_ptr` and `std::unique_ptr` for ownership
- Use `std::string` for string handling
- Use `std::vector` for dynamic arrays
- Use `constexpr` for compile-time constants
- Use `enum class` for type-safe enums
- Use `auto` sparingly, prefer explicit types for APIs

### Error Handling
- Custom error codes in `interfaces/common/dfx_errors.h`
- Use enum classes for error types: `UnwindErrorCode`, `DumpCatchErrInfo`
- Return `bool` for success/failure in simple cases
- Return `std::pair<int, std::string>` for complex error reporting
- Log errors using DFXLOGE/DFXLOGF macros

### Logging
- Use DFXLOG* macros from `common/dfxlog/DFX_LOG_H`:
  - `DFXLOGD()` - Debug (only when DFX_LOG_UNWIND defined)
  - `DFXLOGI()` - Info
  - `DFXLOGW()` - Warning
  - `DFXLOGE()` - Error
  - `DFXLOGF()` - Fatal (includes file:line)
  - `DFXLOGU()` - Unwind-specific logging
- Define `LOG_DOMAIN` and `LOG_TAG` in cpp files
- Use `%{public}s` format specifier for public strings in HiLog

### Memory and Resources
- Use RAII for resource management
- Prefer stack allocation over heap
- Use `std::make_unique` and `std::make_shared`
- Close file descriptors using `close()` or smart wrappers like `SmartFd`

### Concurrency
- Use `std::mutex` for synchronization
- Use `std::lock_guard` or `std::unique_lock` for scoped locking
- Singleton implementations with `GetInstance()` pattern

### Testing
- Use Google Test framework
- Test class naming: `<ModuleName>Test` (e.g., `DumpCatcherInterfacesTest`)
- Use `SetUpTestCase()` and `TearDownTestCase()` for class-level setup
- Use `TEST_F()` for fixture-based tests
- Use `TEST()` for simple tests
- Mock external dependencies when needed

### GN Build Files
- Use `import("//base/hiviewdfx/faultloggerd/faultloggerd.gni")` for common config
- Libraries: `ohos_shared_library` or `ohos_static_library`
- Executables: `ohos_executable`
- Tests: `ohos_unittest`
- Use `external_deps` for OpenHarmony system dependencies
- Use `deps` for internal dependencies

### Architecture Patterns
- Pimpl idiom for hiding implementation details (e.g., `DfxDumpCatcher::Impl`)
- Service-oriented architecture with client-server communication
- Signal handling for crash detection
- Stack unwinding for backtrace generation

### Platform Support
- Multi-architecture: ARM, ARM64, x86_64, RISC-V64, LoongArch64
- Use `#ifdef` guards for platform-specific code
- Define files in `interfaces/common/unwind_*_define.h` for each architecture
