//
// Created by congyu on 7/18/23.
//

#ifndef CJ_2_STRINGHASH_H
#define CJ_2_STRINGHASH_H

#include <stdint.h>

typedef struct Entry {
    char* key;
    uint32_t value;
    struct Entry* next;
} Entry;

typedef struct HashTable {
    uint32_t table_size;
    uint32_t num_entries;
    Entry** entries;
} HashTable;

uint32_t hashString(char* str);
HashTable* createHashTable(uint32_t table_size);
void deleteHashTable(HashTable* table);
char* internalInsert(HashTable* table, char* key, uint32_t value);
void internalResizeInsert(HashTable* table, char* key, uint32_t value);
uint32_t* internalFind(HashTable* table, char* key);
char* internalFindKeyPtr(HashTable* table, char* key);
void delete(HashTable* table, char* key);
void resize(HashTable* table);
void printHashTable(HashTable* table);
void printHashTableStructure(HashTable* table);

void initStringHash();
void deleteStringHash();
char* addReference(char* key);
void removeReference(char* key);
void printStringHash();

void printStringHashStructure();

#endif //CJ_2_STRINGHASH_H
