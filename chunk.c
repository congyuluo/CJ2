//
// Created by Congyu Luo on 7/18/23.
//

#include <string.h>

#include "chunk.h"
#include "object.h"
#include "errors.h"

valueArray* createValueArray(uint16_t size) {
    valueArray* array = malloc(sizeof(valueArray));

    if (array == NULL) compilationError(0, 0, 0, "Memory allocation failed.");

    array->data = malloc(size * sizeof(Value));
    if (array->data == NULL) compilationError(0, 0, 0, "Memory allocation failed.");

    array->count = 0;
    array->capacity = size;

    return array;
}

uint16_t addValToList(valueArray* array, Value obj) {
    if (array->capacity <= array->count + 1) {
        array->capacity *= 2;

        Value* newData = realloc(array->data, sizeof(Value) * array->capacity);

        if (array->data == NULL) compilationError(0, 0, 0, "Memory allocation failed.");

        // If realloc succeeded, update the array->data pointer
        array->data = newData;
    }

    array->data[array->count++] = obj;
    return array->count - 1;
}

bool areValuesEqual(Value v1, Value v2) {
    if (v1.type != v2.type) return false;

    switch (v1.type) {
        case VAL_NONE:
            return true;
        case VAL_BOOL:
            return v1.boolean == v2.boolean;
        case VAL_NUMBER:
            return v1.num == v2.num;
        case BUILTIN_STR:
            return strcmp(VALUE_STR_VALUE(v1), VALUE_STR_VALUE(v2)) == 0;
        default:
            return v1.obj == v2.obj;
    }
}


uint8_t addObj(valueArray* array, Value obj) {
    // Performs linear search for the object by its objID
    for (int i=0; i<array->count; i++) if (areValuesEqual(array->data[i], obj)) return i;
    // If not found, add the object to the array
    return addValToList(array, obj);
}

void freeObjArray(valueArray* array) {
    free(array->data);
    array->data = NULL;
    array->count = 0;
    array->capacity = 0;
    free(array);
}

Chunk* createChunk() {
    Chunk* c = malloc(sizeof(Chunk));
    c->count = 0;
    c->capacity = CHUNK_INIT_SIZE;
    c->currIndexAtLine = 0;
    // Local ref array will be set after chunk compilation
    c->localRefArraySize = 0;
    c->code = malloc(c->capacity*sizeof(uint64_t));
    c->lines = malloc(c->capacity*sizeof(uint16_t));
    c->indices = malloc(c->capacity*sizeof(uint8_t));
    c->sourceIndices = malloc(c->capacity*sizeof(uint8_t));
    // Check if memory allocation succeeded
    if (c->code == NULL || c->lines == NULL || c->indices == NULL || c->sourceIndices == NULL) {
        compilationError(0, 0, 0, "Memory allocation failed.");
    }
    // Init objArray
    c->constants = createValueArray(OBJ_ARRAY_INIT_SIZE);
    return c;
}

void freeChunk(Chunk* c) {
    // Free objArray
    freeObjArray(c->constants);
    free(c->code);
    free(c->lines);
    free(c->indices);
    free(c->sourceIndices);
    free(c);
}

void internalWriteChunk4(Chunk* c, uint8_t data) {
    // Ensure the data is only 4 bits
    if (data > 15) compilationError(c->lines[c->count-1], c->indices[c->count-1], c->sourceIndices[c->count-1], "Data exceeds 4 bits.");
    // Check if there's enough space in the current line for 4 more bits
    if (c->currIndexAtLine > 60) compilationError(c->lines[c->count-1], c->indices[c->count-1], c->sourceIndices[c->count-1], "Instruction too long.");
    // Write the 4 bits to the current line
    c->code[c->count-1] |= ((uint64_t)data << c->currIndexAtLine);
    // Move the current index 4 bits to the right
    c->currIndexAtLine += 4;
}

void internalWriteChunk8(Chunk* c, uint8_t data) {
    if (c->currIndexAtLine > 56) compilationError(c->lines[c->count-1], c->indices[c->count-1], c->sourceIndices[c->count-1], "Instruction too long.");
    c->code[c->count-1] |= ((uint64_t)data << c->currIndexAtLine);
    c->currIndexAtLine += 8;
}

void internalWriteChunk16(Chunk* c, uint16_t data) {
    if (c->currIndexAtLine > 48) compilationError(c->lines[c->count-1], c->indices[c->count-1], c->sourceIndices[c->count-1], "Instruction too long.");
    c->code[c->count-1] |= ((uint64_t)data << c->currIndexAtLine);
    c->currIndexAtLine += 16;
}

void internalWriteChunk32(Chunk* c, uint32_t data) {
    if (c->currIndexAtLine > 32) compilationError(c->lines[c->count-1], c->indices[c->count-1], c->sourceIndices[c->count-1], "Instruction too long.");
    c->code[c->count-1] |= ((uint64_t)data << c->currIndexAtLine);
    c->currIndexAtLine += 32;
}

void writeOp(Chunk* c, OpCode op, uint16_t line, uint8_t index, uint8_t sourceIndex) {
    // Check if the capacity is enough
    if (c->capacity <= c->count + 1) {
        c->capacity *= 2;
        // Temporary pointers for realloc results
        uint64_t* newCode = realloc(c->code, sizeof(uint64_t) * c->capacity);
        uint16_t* newLines = realloc(c->lines, sizeof(uint16_t) * c->capacity);
        uint8_t* newIndices = realloc(c->indices, sizeof(uint8_t) * c->capacity);
        uint8_t* newSourceIndices = realloc(c->sourceIndices, sizeof(uint8_t) * c->capacity);

        if (newCode == NULL || newLines == NULL || newIndices == NULL || newSourceIndices == NULL) {
            compilationError(0, 0, 0, "Memory allocation failed.");
        }

        // Update the pointers in the Chunk structure
        c->code = newCode;
        c->lines = newLines;
        c->indices = newIndices;
        c->sourceIndices = newSourceIndices;
    }
    // Increment count
    c->count++;
    // Set line & index number
    c->lines[c->count-1] = line;
    c->indices[c->count-1] = index;
    c->sourceIndices[c->count-1] = sourceIndex;
    // Reset line index to 0
    c->currIndexAtLine = 0;
    // Init to zero
    c->code[c->count-1] = 0;
    internalWriteChunk8(c, op);
}

void writeLine(Chunk* c, uint64_t data, uint16_t line, uint8_t index, uint8_t sourceIndex) {
    // Check if the capacity is enough
    if (c->capacity <= c->count + 1) {
        c->capacity *= 2;
        // Temporary pointers for realloc results
        uint64_t* newCode = realloc(c->code, sizeof(uint64_t) * c->capacity);
        uint16_t* newLines = realloc(c->lines, sizeof(uint16_t) * c->capacity);
        uint8_t* newIndices = realloc(c->indices, sizeof(uint8_t) * c->capacity);
        uint8_t* newSourceIndices = realloc(c->sourceIndices, sizeof(uint8_t) * c->capacity);

        if (newCode == NULL || newLines == NULL || newIndices == NULL || newSourceIndices == NULL) {
            compilationError(0, 0, 0, "Memory allocation failed.");
        }

        // Update the pointers in the Chunk structure
        c->code = newCode;
        c->lines = newLines;
        c->indices = newIndices;
        c->sourceIndices = newSourceIndices;
    }
    // Increment count
    c->count++;
    // Set line & index number
    c->lines[c->count-1] = line;
    c->indices[c->count-1] = index;
    c->sourceIndices[c->count-1] = sourceIndex;

    c->code[c->count-1] = data;

    // Set line index to 64
    c->currIndexAtLine = 64;
}

void writeChunk4(Chunk* c, uint8_t data) {
    internalWriteChunk4(c, data);
}

void writeChunk8(Chunk*c, uint8_t data) {
    internalWriteChunk8(c, data);
}

void writeChunk16(Chunk* c, uint16_t data) {
    internalWriteChunk16(c, data);
}

void writeChunk32(Chunk* c, uint32_t data) {
    internalWriteChunk32(c, data);
}

void writeValConstant(Chunk* c, Value constant) {
    internalWriteChunk8(c, addObj(c->constants, constant));
}

uint16_t writeJump(Chunk* c, OpCode op, uint16_t line, uint8_t index, uint8_t sourceIndex) {
    // Write the jump instruction
    writeOp(c, op, line, index, sourceIndex);
    // Note the index of the jump instruction
    uint16_t jumpAddrChunkIndex = c->count-1;
    // Write the placeholder for the jump offset and carry over line & index number
    internalWriteChunk16(c, 0);
    return jumpAddrChunkIndex;
}

void patchJump(Chunk* c, uint16_t chunkIndex, uint16_t jumpAddr) {
    // Directly patch the jump address into the chunk
    c->code[chunkIndex] |= ((uint64_t)jumpAddr << 8);
}

void patchJumpAtCurrent(Chunk* c, uint16_t chunkIndex) {
    patchJump(c, chunkIndex, c->count);
}

void writeJumpBack(Chunk* c, OpCode op, uint16_t jumpAddr, uint16_t line, uint8_t index, uint8_t sourceIndex) {
    // Write the jump instruction
    writeOp(c, op, line, index, sourceIndex);
    internalWriteChunk16(c, jumpAddr);
}

Chunk* cropChunk(Chunk* c, uint16_t start) { // Copies chunk c from start to ending index and clears
    // Create a new chunk
    Chunk* newChunk = createChunk();
    for (int i=start; i<c->count; i++) {
        writeLine(newChunk, c->code[i], c->lines[i], c->indices[i], c->sourceIndices[i]);
        // Clear index on chunk c
        c->code[i] = 0;
        c->lines[i] = 0;
        c->indices[i] = 0;
        c->sourceIndices[i] = 0;
    }
    // Set count to start
    c->count = start;
    return newChunk;
}

void copyChunk(Chunk* main, Chunk* addChunk) {
    // Copy instructions
    for (int i=0; i<addChunk->count; i++) writeLine(main, addChunk->code[i], addChunk->lines[i], addChunk->indices[i], addChunk->sourceIndices[i]);
}
