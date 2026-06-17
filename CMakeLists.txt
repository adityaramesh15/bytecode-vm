cmake_minimum_required(VERSION 3.24)
project(bytecode-vm LANGUAGES CXX)

# ==========================================
# 1. Global Language & Toolchain Settings
# ==========================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enforce Clang-specific standard library overrides early
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++ -lc++abi)
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

# Apply Runtime Sanitizers (ASan and UBSan)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(vm_compiler_settings INTERFACE /fsanitize=address)
    target_link_options(vm_compiler_settings INTERFACE /fsanitize=address)
else()
    target_compile_options(vm_compiler_settings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(vm_compiler_settings INTERFACE -fsanitize=address,undefined)
endif()

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
    set(TEST_CLANG_TIDY_CHECKS
        ${CLANG_TIDY_EXE}
        "-checks=-*,bugprone-*,cert-*,performance-*,portability-*,readability-*,modernize-*,-cert-err58-cpp,-bugprone-chained-comparison,-readability-magic-numbers,-bugprone-use-after-move,-modernize-avoid-c-arrays"
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