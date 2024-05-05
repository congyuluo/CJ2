//
// Created by Congyu Luo on 7/18/23.
//

#ifndef CJ_2_CHUNK_H
#define CJ_2_CHUNK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "common.h"

typedef struct Value Value;

typedef enum OpCode {
    OP_CONSTANT,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_LESS,
    OP_MORE,
    OP_LESS_EQUAL,
    OP_MORE_EQUAL,
    OP_NEGATE,
    OP_NOT,
    OP_EQUAL,
    OP_AND,
    OP_OR,
    OP_POW,
    OP_IS,
    OP_GET_SELF,
    OP_GET_INDEX_REF,
    OP_GET_GLOBAL_REF_ATTR,
    OP_GET_LOCAL_REF_ATTR,
    OP_GET_COMBINED_REF_ATTR,
    OP_GET_ATTR,
    OP_GET_ATTR_CALL,
    OP_SET_GLOBAL_REF_ATTR,
    OP_SET_LOCAL_REF_ATTR,
    OP_SET_COMBINED_REF_ATTR,
    OP_SET_INDEX_REF,
    OP_SET_ATTR,
    OP_EXEC_FUNCTION_ENFORCE_RETURN,
    OP_EXEC_FUNCTION_IGNORE_RETURN,
    OP_EXEC_METHOD_ENFORCE_RETURN,
    OP_EXEC_METHOD_IGNORE_RETURN,
    OP_INIT, // Creates a new object, and push it to the stack with init function
    OP_GET_PARENT_INIT,
    OP_RETURN,
    OP_RETURN_NONE,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
} OpCode;

typedef enum specialAssignment {
    ASSIGNMENT_NONE,
    ASSIGNMENT_ADD,
    ASSIGNMENT_SUB,
    ASSIGNMENT_MUL,
    ASSIGNMENT_DIV,
    ASSIGNMENT_MOD,
    ASSIGNMENT_POWER,
} specialAssignment;

typedef struct objArray {
    int count;
    int capacity;
    Value* data;
} valueArray;

typedef struct Chunk {
    uint32_t count;
    uint32_t capacity;
    uint8_t currIndexAtLine; // Current index of the line within the 64 bits
    uint16_t localRefArraySize;
    uint64_t* code;
    uint16_t* lines;
    uint8_t* indices;
    uint8_t* sourceIndices;
    valueArray* constants;
} Chunk;

valueArray* createValueArray(uint16_t size);
uint16_t addValToList(valueArray* array, Value obj);

bool areValuesEqual(Value v1, Value v2);
void freeObjArray(valueArray* array);

Chunk* createChunk();
void freeChunk(Chunk* c);

void writeChunk4(Chunk* c, uint8_t data);
void writeChunk8(Chunk* c, uint8_t data);
void writeChunk16(Chunk* c, uint16_t data);
void writeChunk32(Chunk* c, uint32_t data);
void writeOp(Chunk* c, OpCode op, uint16_t line, uint8_t index, uint8_t sourceIndex);
void writeLine(Chunk* c, uint64_t data, uint16_t line, uint8_t index, uint8_t sourceIndex);

void writeValConstant(Chunk* c, Value constant);
uint16_t writeJump(Chunk* c, OpCode op, uint16_t line, uint8_t index, uint8_t sourceIndex);
void patchJump(Chunk* c, uint16_t chunkIndex, uint16_t jumpAddr);
void patchJumpAtCurrent(Chunk* c, uint16_t chunkIndex);
void writeJumpBack(Chunk* c, OpCode op, uint16_t jumpAddr, uint16_t line, uint8_t index, uint8_t sourceIndex);

Chunk* cropChunk(Chunk* c, uint16_t start);
void copyChunk(Chunk* main, Chunk* addChunk); // Adds addChunk to main chunk

#endif //CJ_2_CHUNK_H
