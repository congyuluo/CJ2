//
// Created by congyu on 7/18/23.
//

#include "errors.h"
#include "debug.h"
#include "stringHash.h"

#include <stdio.h>
#include <stdlib.h>

char* sourceArray[MAX_SOURCE_SIZE];
char* fileNameArray[MAX_SOURCE_SIZE];

uint32_t sourceCount = 0;
Chunk** cArray = NULL;
uint32_t chunkArraySize = 0;

void attachSource(char* s, char* sourceName) {
    sourceArray[sourceCount] = s;
    fileNameArray[sourceCount] = addReference(sourceName);
    sourceCount++;
}

void attachChunkArray(Chunk** ca, uint32_t size) {
    cArray = ca;
    chunkArraySize = size;
}

void nullSourceError() {
    fprintf(stderr, "\nnullSourceError: Source never attached\n");
    exit(EXIT_FAILURE);
}

void printSourceLocation(uint32_t line, uint32_t index, uint32_t sourceIndex) {
    if (sourceIndex >= sourceCount) nullSourceError();
    int lineCount = 0;
    char* ptr = sourceArray[sourceIndex];
    // Find the line
    while (lineCount < line) {
        if (lineCount >= line - 2) fprintf(stderr, "%c", *ptr);
        if (*ptr == '\n') lineCount++;
        ptr++;
    }
    // Print the line
    while (*ptr != '\n' && *ptr != '\0') {
        fprintf(stderr, "%c", *ptr);
        ptr++;
    }
    fprintf(stderr, "\n");
    for (int i = 0; i < index; i++) fprintf(stderr, " ");
    fprintf(stderr, "^\n");
    for (int i = 0; i < index; i++) fprintf(stderr, " ");
    fprintf(stderr, "In \"%s\": [line: %d, index %d]\n", fileNameArray[sourceIndex], line+1, index+1);
}

void printFrame(uint64_t* ip) {
    ip--;
    for (uint32_t i=0; i<chunkArraySize; i++) {
        if (ip >= cArray[i]->code && ip < cArray[i]->code + cArray[i]->count) {
            uint32_t offset = ip - cArray[i]->code;
            uint16_t line = cArray[i]->lines[offset];

            uint8_t index = cArray[i]->indices[offset];
            uint8_t sourceIndex = cArray[i]->sourceIndices[offset];
#ifdef PRINT_ERROR_OP
            printf("Current instruction: \n");
            printInstr(*ip, cArray[i]);
            printf("\n");
#endif
            printSourceLocation(line, index, sourceIndex);
            return;
        }
    }
    fprintf(stderr, "Instruction pointer not found in any chunk\n");
}

void printRuntimeTraceback() {
    fprintf(stderr, "Runtime traceback:\n");
    uint64_t*** currIpStackPtr = ipStackTop-1;
    uint32_t offset = currIpStackPtr - ipStack;
    while (1) {
        fprintf(stderr, "\nCall Frame [%u]:\n", offset--);
        printFrame(**currIpStackPtr);
        if (currIpStackPtr == ipStack) break;
        currIpStackPtr--;
    }
}

void varError(char *message) {
    fprintf(stderr, "\nvarError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void objHashError(char *message) {
    fprintf(stderr, "\nobjHashError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void callableError(char *message) {
    fprintf(stderr, "\ncallableError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void objManagerError(char *message) {
    fprintf(stderr, "\nobjManagerError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void strHashError(char *message) {
    fprintf(stderr, "\nstrHashError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void listError(char *message) {
    fprintf(stderr, "\nlistError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void dictError(char *message) {
    fprintf(stderr, "\ndictError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void setError(char *message) {
    fprintf(stderr, "\nsetError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void GCError(char *message) {
    fprintf(stderr, "\nGCError: %s\n", message);
    if (isRuntime) printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void runtimeError(char *message) {
    fprintf(stderr, "\nruntimeError: %s\n", message);
    printRuntimeTraceback();
    exit(EXIT_FAILURE);
}

void parsingError(uint16_t line, uint8_t index, uint8_t sourceIndex, char *message) {
    fprintf(stderr, "\nparsingError: %s\n", message);
    printSourceLocation(line, index, sourceIndex);
    exit(EXIT_FAILURE);
}

void compilationError(uint16_t line, uint8_t index, uint8_t sourceIndex, char *message) {
    fprintf(stderr, "\ncompilationError: %s\n", message);
    printSourceLocation(line, index, sourceIndex);
    exit(EXIT_FAILURE);
}

void freeErrorTracer() {
    for (uint32_t i=0; i<sourceCount; i++) free(sourceArray[i]);
    if (cArray != NULL) free(cArray);
}
