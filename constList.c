//
// Created by congyu on 8/19/23.
//

#include "constList.h"
#include "errors.h"

ConstBlock* constList = NULL;

void createConstList() {
    constList = (ConstBlock*)malloc(sizeof(ConstBlock));
    constList->nextSlot = constList->block;
    constList->next = NULL;
}

Object* addConst() {
    bool createNewBlock = constList->nextSlot == &constList->block[CONST_BLOCK_SIZE-1];
    Object* objSlot = constList->nextSlot;
    constList->nextSlot++;
    if (createNewBlock) {
        ConstBlock* newBlock = (ConstBlock*)malloc(sizeof(ConstBlock));
        if (newBlock == NULL) objManagerError("Failed to allocate memory for new const block.");
        newBlock->nextSlot = newBlock->block;
        newBlock->next = constList;
        constList = newBlock;
    }
    return objSlot;
}

void freeBlock(ConstBlock* block) {
    Object* objPtr = (Object*) block->block;
    while (objPtr != block->nextSlot) deleteConst(objPtr++);
    free(block);
}

void freeConstList() {
    ConstBlock* currBlock = constList;
    while (currBlock != NULL) {
        ConstBlock* nextBlock = currBlock->next;
        freeBlock(currBlock);
        currBlock = nextBlock;
    }
}

void printConstList() {
    ConstBlock* currBlock = constList;
    uint32_t index = 0;
    while (currBlock != NULL) {
        Object* currObj = currBlock->block;
        while (currObj != currBlock->nextSlot) {
            printf("#%d\t", index++);
            printObject(currObj++);
            printf("\n");
        }
        currBlock = currBlock->next;
    }
}


