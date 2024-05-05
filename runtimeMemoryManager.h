//
// Created by congyu on 8/20/23.
//

#ifndef CJ_2_RUNTIMEMEMORYMANAGER_H
#define CJ_2_RUNTIMEMEMORYMANAGER_H

#include "object.h"


typedef struct RuntimeBlock RuntimeBlock;

struct RuntimeBlock {
    Object block[RUNTIME_BLOCK_SIZE];
    RuntimeBlock* nextBlock;
    uint16_t blockID;
};

typedef struct RuntimeMemoryManager {
    Object* freeStack[FREE_STACK_SIZE];
    Object** freeStackTop;
    RuntimeBlock* headBlock;
} RuntimeMemoryManager;

extern RuntimeMemoryManager* memoryManager;

void initMemoryManager();
void freeMemoryManager();

Object* newObjectSlot();

#endif //CJ_2_RUNTIMEMEMORYMANAGER_H
