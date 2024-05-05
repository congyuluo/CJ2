//
// Created by congyu on 7/23/23.
//

#ifndef CJ_2_RUNTIMEDS_H
#define CJ_2_RUNTIMEDS_H

#include "object.h"

// Data structure declarations

struct runtimeList {
    Value* list;
    uint32_t size;
    uint32_t capacity;
};

typedef struct runtimeDictEntry runtimeDictEntry;

struct runtimeDictEntry {
    Value key;
    Value value;
    runtimeDictEntry* next;
};

struct runtimeDict {
    uint32_t tableSize;
    uint32_t numEntries;
    runtimeDictEntry** entries;
};

struct runtimeSet {
    runtimeDict* dict;
};

// List functions
runtimeList* createRuntimeList(uint32_t size);
void listAddElement(runtimeList* list, Value value);
uint32_t listAddElementReturnIndex(runtimeList* list, Value value);
void listInsertElement(runtimeList* list, uint32_t index, Value value);
void listSetElement(runtimeList* list, uint32_t index, Value value);
void listRemoveElement(runtimeList* list, uint32_t index);
Value listGetElement(runtimeList* list, uint32_t index);
bool listContainsElement(runtimeList* list, Value value);
uint32_t listIndexOfElement(runtimeList* list, Value value);

// Dict functions
runtimeDict* createRuntimeDict(uint32_t size);
void dictInsertElement(runtimeDict* dict, Value key, Value value);
Value dictGetElement(runtimeDict* dict, Value key);
bool dictContainsElement(runtimeDict* dict, Value key);
void dictRemoveElement(runtimeDict* dict, Value key);

// Dict additional functions
Value dictStrGet(runtimeDict* dict, char* key);
Value dictNumGet(runtimeDict* dict, double key);

// Set functions
runtimeSet* createRuntimeSet(uint32_t size);
void setInsertElement(runtimeSet* set, Value key);
bool setContainsElement(runtimeSet* set, Value key);
void setRemoveElement(runtimeSet* set, Value key);

// General Purpose Functions
uint32_t hashObject(Value key);
void DSPrintValue(Value val);

#endif //CJ_2_RUNTIMEDS_H
