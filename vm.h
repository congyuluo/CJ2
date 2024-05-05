//
// Created by congyu on 7/21/23.
//

#ifndef CJ_2_VM_H
#define CJ_2_VM_H

#include "chunk.h"
#include "refManager.h"

#define CREATE_BUILTIN_CHUNK_FUNCTION_OBJECT(chunk, in, out) createConstCallableObject(CREATE_CHUNK_FUNCTION(in, out, chunk))

typedef struct {
    Value stack [VM_STACK_INIT_SIZE];
    Value* stackTop;
    Value* globalRefArray; // Global reference array
    callable** functionArray; // Function array
    uint16_t globalRefCount; // Number of global references
    uint16_t localScopeCount; // Number of local scopes
} VM;

extern VM* vm;

Value execInput(Value callableObj, Value selfObj, Value* attrs, uint8_t inCount);
void execInplace(Value callableObj, uint8_t inCount);
void execChunk(Chunk* chunk, Value* dataSection);

bool compareValue(Value v1, Value v2);

void initVM(Value* globalRefArray, callable** functionArray, uint16_t globalRefCount);

Value unaryOperation(Value obj1, char* op);
Value binaryOperation(Value v1, Value v2, OpCode op);

Value performValueModification(specialAssignment sa, Value value, Value modValue);

void indexSpecialAssignment(specialAssignment sa , Value target, Value index, Value value);
void attrSpecialAssignment(specialAssignment sa, Value target, char* attrName, Value value);

Value objGetIndexRef(Value target, Value index);

void freeVM();

void runVM(Value mainFunc, Value attrs, int inCount);

void defaultPrint(Value obj);

void printStack();

#endif //CJ_2_VM_H
