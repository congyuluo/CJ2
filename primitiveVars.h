//
// Created by congyu on 7/18/23.
//

#ifndef CJ_2_PRIMITIVEVARS_H
#define CJ_2_PRIMITIVEVARS_H

#include "chunk.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct Object Object;

typedef struct runtimeList runtimeList;
typedef struct runtimeDict runtimeDict;
typedef struct runtimeSet runtimeSet;

typedef Value (*cMethodType)(Value, Value*, int);

typedef struct userFunction {
    int32_t in;
    int32_t out;
    char* name;
    cMethodType cFunc;
} userFunction;

#define CREATE_CHUNK_FUNCTION(in, out, chunk) createCallable(in, out, NULL, chunk, function)
#define CREATE_CHUNK_METHOD(in, out, chunk) createCallable(in, out, NULL, chunk, method)
#define CREATE_CFUNC_METHOD(in, out, cFunc) createCallable(in, out, cFunc, NULL, method)
#define CREATE_CFUNC_FUNCTION(in, out, cFunc) createCallable(in, out, cFunc, NULL, function)

typedef enum callableType {
    method,
    function,
} callableType;

typedef struct callable {
    int32_t in;
    int32_t out;
    cMethodType cFunc;
    Chunk* func;
    callableType type;
} callable;

void freeRuntimeList(runtimeList* list);
void freeRuntimeDict(runtimeDict* dict);
void freeRuntimeSet(runtimeSet* set);

void printRuntimeList(runtimeList* list);
void printRuntimeDict(runtimeDict* dict);
void printRuntimeSet(runtimeSet* set);

#endif //CJ_2_PRIMITIVEVARS_H
