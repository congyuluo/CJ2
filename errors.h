//
// Created by congyu on 7/18/23.
//

#ifndef CJ_2_ERRORS_H
#define CJ_2_ERRORS_H

#include "chunk.h"
#include <stdbool.h>

extern uint64_t** ipStack[VM_LOCAL_REF_TABLE_STACK_INIT_SIZE];
extern uint64_t*** ipStackTop;
extern bool isRuntime;

void attachSource(char* s, char* sourceName);
void attachChunkArray(Chunk** ca, uint32_t size);

void varError(char *message);

void objHashError(char *message);

void callableError(char *message);

void objManagerError(char *message);

void strHashError(char *message);

void listError(char *message);

void dictError(char *message);

void setError(char *message);

void runtimeError(char *message);

void GCError(char *message);

void parsingError(uint16_t line, uint8_t index, uint8_t sourceIndex, char *message);

void compilationError(uint16_t line, uint8_t index, uint8_t sourceIndex, char *message);

void freeErrorTracer();

#endif //CJ_2_ERRORS_H
