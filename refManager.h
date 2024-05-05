//
// Created by congyu on 8/12/23.
//

#ifndef CJ_2_REFMANAGER_H
#define CJ_2_REFMANAGER_H

#include "runtimeDS.h"

typedef struct refTableEntry refTableEntry;

struct refTableEntry {
    char* key;
    uint16_t value;
    refTableEntry* next;
};

typedef struct refTable {
    uint32_t tableSize;
    uint32_t numEntries;
    refTableEntry** entries;
} refTable;

refTable* createRefTable(uint32_t size);

uint16_t getRefIndex(refTable* refTable, char* identifier);
bool refTableContains(refTable* dict, char* key);
void freeRefTable(refTable* dict);
void printRefTable(refTable* dict);

#endif //CJ_2_REFMANAGER_H
