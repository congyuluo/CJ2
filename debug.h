//
// Created by congyu on 7/18/23.
//

#ifndef CJ_2_DEBUG_H
#define CJ_2_DEBUG_H

#include "common.h"
#include "chunk.h"
#include "primitiveVars.h"
#include "tokenizer.h"

#include <stdio.h>

void printConstOp(char* name, Chunk* c, uint64_t line);
void printSingleOp(char* name, Chunk* c, uint64_t line);

void printInstr(uint64_t line, Chunk* c);

void printObjArray(valueArray* array);
void printChunk(Chunk* c);

void printToken(token* t);


#endif //CJ_2_DEBUG_H
