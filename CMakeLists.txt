cmake_minimum_required(VERSION 3.20)
project(CJ_2 C)

set(CMAKE_C_STANDARD 11)

add_executable(CJ_2 main.c chunk.c debug.c object.c errors.c stringHash.c builtinClasses.c vm.c runtimeDS.c tokenizer.c compiler.c refManager.h refManager.c constList.h constList.c objectManager.h objectManager.c objClass.h objClass.c runtimeMemoryManager.h runtimeMemoryManager.c value.h)

# Link the math library
target_link_libraries(CJ_2 m)