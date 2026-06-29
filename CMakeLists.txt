cmake_minimum_required(VERSION 3.24)
project(bytecode-vm LANGUAGES CXX)

# ==========================================
# 1. Global Language & Toolchain Settings
# ==========================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Apple Clang uses libc++ natively; Linux Clang uses default libstdc++ to avoid
# Ubuntu packaged libc++/ASan alloc-dealloc-mismatch on exception teardown.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(APPLE)
        add_compile_options(-stdlib=libc++ -fexperimental-library)
        add_link_options(-stdlib=libc++ -lc++abi)
    endif()
endif()

# Apply runtime sanitizers globally so FetchContent deps (Catch2) share instrumentation
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/fsanitize=address)
    add_link_options(/fsanitize=address)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ==========================================
# 2. Third-Party Dependencies (FetchContent)
# ==========================================
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0 
)
FetchContent_MakeAvailable(Catch2)

# ==========================================
# 3. Global Interface Settings (Warnings & Sanitizers)
# ==========================================
add_library(vm_compiler_settings INTERFACE)
target_include_directories(vm_compiler_settings INTERFACE include)

# Apply strict compiler warnings
target_compile_options(vm_compiler_settings INTERFACE
    # MSVC (Windows) Specific Rules
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4          # Level 4 warnings
        /WX          # Treat warnings as errors
        /permissive- # Enforce strict ISO C++ compliance
        /w14242      # Warn about conversion problems
        /w14287      # Unsigned negative constant warning
    >
    # GCC / Clang (Linux/macOS) Specific Rules
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Werror -Wpedantic
        -Wshadow -Wconversion -Wnon-virtual-dtor
    >
)

# ==========================================
# 4. Primary Targets (Executables & Benchmarks)
# ==========================================

# Main VM Executable
add_executable(bytecode_vm src/main.cpp)
target_link_libraries(bytecode_vm PRIVATE vm_compiler_settings)

# Benchmark Executable
add_executable(matrix_benchmark benchmarks/matrix_benchmark.cpp)
target_compile_options(matrix_benchmark PRIVATE 
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Werror -O3
    >
)
add_executable(false_sharing_benchmark benchmarks/false_sharing_benchmark.cpp)
target_compile_options(false_sharing_benchmark PRIVATE 
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Werror -O3
    >
)
add_executable(ast_traversal_benchmark benchmarks/ast_traversal_benchmark.cpp)
target_compile_options(ast_traversal_benchmark PRIVATE 
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Werror -O3
    >
)

# ==========================================
# 5. Automated Testing Target
# ==========================================
file(GLOB_RECURSE TEST_SOURCES 
    CONFIGURE_DEPENDS 
    "tests/test_*.cpp"
)
add_executable(vm_tests ${TEST_SOURCES})
target_link_libraries(vm_tests 
    PRIVATE 
        vm_compiler_settings
        Catch2::Catch2WithMain
)

# ==========================================
# 6. Code Quality & Static Analysis (Clang-Tidy)
# ==========================================
unset(CMAKE_CXX_CLANG_TIDY)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    message(STATUS "DevDiv Toolchain: Configuring localized static analysis passes.")
    
    set(CORE_CLANG_TIDY_CHECKS
        ${CLANG_TIDY_EXE}
        "-checks=-*,bugprone-*,cert-*,performance-*,portability-*,readability-*,modernize-*"
        "-warnings-as-errors=*"
    )
    
    # ADDED EXCLUSIONS: Suppress macro complexity and trailing returns for Catch2 compatibility
    set(TEST_CLANG_TIDY_CHECKS
        ${CLANG_TIDY_EXE}
        "-checks=-*,bugprone-*,cert-*,performance-*,portability-*,readability-*,modernize-*,-cert-err58-cpp,-bugprone-chained-comparison,-readability-magic-numbers,-bugprone-use-after-move,-modernize-avoid-c-arrays,-readability-function-cognitive-complexity,-modernize-use-trailing-return-type"
        "-warnings-as-errors=*"
    )
    
    # Safely attach Clang-Tidy parameters to defined targets
    if(TARGET bytecode_vm)
        set_target_properties(bytecode_vm PROPERTIES CXX_CLANG_TIDY "${CORE_CLANG_TIDY_CHECKS}")
    endif()
    
    if(TARGET vm_tests)
        set_target_properties(vm_tests PROPERTIES CXX_CLANG_TIDY "${TEST_CLANG_TIDY_CHECKS}")
    endif()
else()
    message(WARNING "DevDiv Toolchain: clang-tidy binary was not detected on this system host environment.")
endif()