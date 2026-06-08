# bytecode-vm
A high-performance, register-based Bytecode VM featuring a custom memory subsystem, an optimizing compiler frontend, and an isolated execution runtime.


## curr build instructions
``` bash

cmake -DCMAKE_CXX_COMPILER=clang++ ..
make
./vm_tests

```