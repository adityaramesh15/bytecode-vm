cmake_minimum_required(VERSION 3.20)
project(bytecode-vm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vm_compiler_settings INTERFACE)
target_include_directories(vm_compiler_settings INTERFACE include)

target_compile_options(vm_compiler_settings INTERFACE
    # If using Microsoft Visual Studio Compiler (MSVC)
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4          # Level 4 warnings
        /WX          # Treat warnings as errors
        /permissive- # Enforce strict ISO C++ compliance
        /w14242      # Warn about conversion problems
        /w14287      # Unsigned negative constant warning
    >
    # If using GNU (GCC) or Clang (Mac/Linux)
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
        -Wall -Wextra -Werror -Wpedantic
        -Wshadow -Wconversion -Wnon-virtual-dtor
    >
)


if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(vm_compiler_settings INTERFACE /fsanitize=address)
    target_link_options(vm_compiler_settings INTERFACE /fsanitize=address)
else()
    target_compile_options(vm_compiler_settings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(vm_compiler_settings INTERFACE -fsanitize=address,undefined)
endif()




add_executable(vm_tests 
    src/main.cpp
    tests/test_day1.cpp
)
target_link_libraries(vm_tests PRIVATE vm_compiler_settings)