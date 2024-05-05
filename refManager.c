//
// Created by congyu on 8/12/23.
//

#include "refManager.h"
#include "errors.h"
#include "stringHash.h"

#include <string.h>
#include <assert.h>

refTable* createRefTable(uint32_t size) {
    refTable* dict = (refTable*) malloc(sizeof(refTable));
    if (dict == NULL) dictError("Failed to allocate memory for refTable.");
    dict->tableSize = size;
    dict->numEntries = 0;
    dict->entries = (refTableEntry**) calloc(size, sizeof(refTableEntry*));
    if (dict->entries == NULL) dictError("Failed to allocate memory for dict entries.");
    return dict;
}

void resizeRefTable(refTable* dict, uint32_t newSize) {
    refTableEntry** newEntries = (refTableEntry**) calloc(newSize, sizeof(refTableEntry*));
    if (newEntries == NULL) dictError("Failed to allocate memory for dict entries during resize");
    // Rehash all entries
    for (uint32_t i = 0; i < dict->tableSize; ++i) {
        refTableEntry* entry = dict->entries[i];
        while (entry != NULL) {
            uint32_t newHash = hashString(entry->key) % newSize;
            refTableEntry* nextEntry = entry->next;
            entry->next = newEntries[newHash];
            newEntries[newHash] = entry;
            entry = nextEntry;
        }
    }

    free(dict->entries);
    dict->entries = newEntries;
    dict->tableSize = newSize;
}

void refTableInsert(refTable* dict, char* key, uint16_t value) {
    uint32_t hash = hashString(key) % dict->tableSize;
    refTableEntry* entry = dict->entries[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;  // Overwrite Value if key already exists
            return;
        }
        entry = entry->next;
    }
    // Key does not exist in dict, create new entry
    entry = (refTableEntry*) malloc(sizeof(refTableEntry));
    if (entry == NULL) dictError("Failed to allocate memory for reference table entry");
    entry->key = key;
    entry->value = value;
    entry->next = dict->entries[hash];  // Insert at head of linked list
    dict->entries[hash] = entry;
    dict->numEntries++;

    // Check if resize is needed
    if ((float)dict->numEntries / (float) dict->tableSize > 0.75) {
        if (dict->tableSize >= (UINT32_MAX/2)) dictError("Reference table size exceeds maximum size during reallocation");
        resizeRefTable(dict, dict->tableSize * 2);
    }
}

bool refTableContains(refTable* dict, char* key) {
    uint32_t hash = hashString(key) % dict->tableSize;
    refTableEntry* entry = dict->entries[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) return true;
        entry = entry->next;
    }
    return false;
}

uint16_t refTableGet(refTable* dict, char* key) {
    uint32_t hash = hashString(key) % dict->tableSize;
    refTableEntry* entry = dict->entries[hash];
    while (entry) {
        if (strcmp(entry->key, key) == 0) return entry->value;
        entry = entry->next;
    }
    dictError("Key not found in reference table");
    return 0;
}

void freeRefTable(refTable* dict) {
    for (uint32_t i = 0; i < dict->tableSize; i++) {
        refTableEntry* entry = dict->entries[i];
        while (entry != NULL) {
            refTableEntry* temp = entry;
            entry = entry->next;
            free(temp);
        }
    }
    free(dict->entries);
    free(dict);
}

uint16_t getRefIndex(refTable* refTable, char* identifier) {
    assert(refTable != NULL);
    // Check if object is already in refTable
    if (refTableContains(refTable, identifier)) return refTableGet(refTable, identifier);
    // If not, assign a new index and add to ref Table
    if (refTable->numEntries >= UINT8_MAX-1) compilationError(0, 0, 0, "RefTable overflow");
    uint16_t objIndex = refTable->numEntries;
    refTableInsert(refTable, identifier, objIndex);
    return objIndex;
}

void printRefTable(refTable* dict) {
    bool first = true;
    printf("refTable{");
    for (uint32_t i = 0; i < dict->tableSize; i++) {
        refTableEntry* entry = dict->entries[i];
        while (entry) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            printf("%s:", entry->key);
            printf("%u", entry->value);

            entry = entry->next;
        }
    }
    printf("}");
}