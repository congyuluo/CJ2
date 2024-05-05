//
// Created by congyu on 8/19/23.
//

#ifndef CJ_2_CONSTLIST_H
#define CJ_2_CONSTLIST_H

#include "common.h"
#include "object.h"

typedef struct ConstBlock ConstBlock;

// Declare a global variable for block head
extern ConstBlock* constList;

struct ConstBlock {
    Object block[CONST_BLOCK_SIZE];
    Object* nextSlot;
    ConstBlock* next;
};

void createConstList();
Object* addConst();
void freeConstList();

void printConstList();

#endif //CJ_2_CONSTLIST_H
