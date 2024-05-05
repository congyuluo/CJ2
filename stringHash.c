//
// Created by congyu on 7/18/23.
//

#include "stringHash.h"
#include "common.h"
#include "errors.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define LOAD_FACTOR_THRESHOLD 0.75

uint32_t hashString(char* str) {
    uint32_t hash = 5381;
    unsigned char c;  // Change to unsigned char

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hashString * 33 + c */

    return hash;
}

HashTable* createHashTable(uint32_t table_size) {
    HashTable* table = malloc(sizeof(HashTable));
    if (table == NULL) strHashError("String hashString table allocation failed");
    table->table_size = table_size;
    table->num_entries = 0;
    table->entries = calloc(table->table_size, sizeof(Entry*));
    if (table->entries == NULL) strHashError("String hashString table allocation failed");

    return table;
}

void deleteHashTable(HashTable* table) {
    assert(table != NULL);

    for (uint32_t i = 0; i < table->table_size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            Entry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(table->entries);
    free(table);
}

char* internalInsert(HashTable* table, char* key, uint32_t value) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    Entry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            strHashError("Inserting duplicate key in string hashString table");
            return NULL;
        }
        entry = entry->next;
    }

    entry = malloc(sizeof(Entry));
    if (entry == NULL) strHashError("String hashString table entry allocation failed");
    char* newKey = strdup(key);
    entry->key = newKey;
    entry->value = value;
    entry->next = table->entries[pos];
    table->entries[pos] = entry;

    table->num_entries++;

    if ((float)table->num_entries / (float)table->table_size > LOAD_FACTOR_THRESHOLD) {
        resize(table);
    }
    return newKey;
}

void internalResizeInsert(HashTable* table, char* key, uint32_t value) {
    assert(table != NULL);
    assert(key != NULL);

    unsigned int pos = hashString(key) % table->table_size;
    Entry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            fprintf(stderr, "Hash table error: key already exists\n");
            return;
        }
        entry = entry->next;
    }

    entry = malloc(sizeof(Entry));
    assert(entry != NULL);
    entry->key = key;
    entry->value = value;
    entry->next = table->entries[pos];
    table->entries[pos] = entry;

    table->num_entries++;

    if ((float)table->num_entries / (float) table->table_size > LOAD_FACTOR_THRESHOLD) {
        resize(table);
    }
}

uint32_t* internalFind(HashTable* table, char* key) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    Entry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return &(entry->value);
        }
        entry = entry->next;
    }

    return NULL;
}

char* internalFindKeyPtr(HashTable* table, char* key) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    Entry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->key;
        }
        entry = entry->next;
    }

    return NULL;
}

void delete(HashTable* table, char* key) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    Entry** p = &(table->entries[pos]);

    while (*p != NULL) {
        if (strcmp((*p)->key, key) == 0) {
            Entry* entry = *p;
            *p = entry->next;
            free(entry->key);
            free(entry);
            table->num_entries--;
            return;
        }
        p = &((*p)->next);
    }
}

void resize(HashTable* table) {
    assert(table != NULL);

    if (table->table_size >= (UINT32_MAX / 2)) strHashError("String hashString table size exceeds maximum during resize");
    uint32_t old_table_size = table->table_size;
    Entry** old_entries = table->entries;

    table->table_size *= 2;
    table->num_entries = 0;
    table->entries = calloc(table->table_size, sizeof(Entry*));
    if (table->entries == NULL) strHashError("String hashString table reallocation failed");

    for (uint32_t i = 0; i < old_table_size; i++) {
        Entry* entry = old_entries[i];
        while (entry != NULL) {
            internalResizeInsert(table, entry->key, entry->value);
            Entry* next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(old_entries);
}

void printHashTable(HashTable* table) {
    assert(table != NULL);

    for (uint32_t i = 0; i < table->table_size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            printf("Key: %s, Value: %u\n", entry->key, entry->value);
            entry = entry->next;
        }
    }
    printf("HashTable of size %u with %u entries:\n\n", table->table_size, table->num_entries);
}

HashTable* stringTable;

void printHashTableStructure(HashTable* table) {
    for (uint32_t i = 0; i < table->table_size; i++) {
        Entry* entry = table->entries[i];
        if (entry != NULL) {
            printf("[%u]: ", i);
        } else {
            printf("[%u]: EMPTY", i);
        }
        while (entry != NULL) {
            printf("\"%s\"", entry->key);

            entry = entry->next;
            if (entry != NULL) {
                printf(" -> ");
            }
        }
        printf("\n");
    }
    printf("Number of Entries: %u, Table Size: %u\n\n", table->num_entries, table->table_size);
}

void initStringHash() {
    stringTable = createHashTable(STRING_TABLE_INIT_SIZE);
}

void deleteStringHash() {
    deleteHashTable(stringTable);
}

char* addReference(char* key) { // Increase reference count and return the key pointer in hashString entry
    // Find the entry
    uint32_t* value = internalFind(stringTable, key);
    if (value == NULL) {
        // If not found, insert a new entry
        return internalInsert(stringTable, key, 1);
    } else {
        // If found, increment the reference count
        (*value)++;
        return internalFindKeyPtr(stringTable, key);
    }
}

void removeReference(char* key) {
    // Find the entry
    uint32_t* value = internalFind(stringTable, key);
    if (value == NULL) {
        fprintf(stderr, "%s\n", key);
        // Error
        strHashError("Key not found");
    } else {
        // If found, decrement the reference count
        (*value)--;
        if (*value == 0) {
            // If the reference count is 0, delete the entry
            delete(stringTable, key);
        }
    }
}

void printStringHash() {
    printHashTable(stringTable);
}

void printStringHashStructure() {
    printHashTableStructure(stringTable);
}

