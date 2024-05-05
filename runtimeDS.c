//
// Created by congyu on 7/23/23.
//

#include "runtimeDS.h"
#include "errors.h"
#include "vm.h"
#include "stringHash.h"

#include <math.h>
#include <string.h>

runtimeList* createRuntimeList(uint32_t size) {
    runtimeList* newList = (runtimeList*) malloc(sizeof(runtimeList));
    if (newList == NULL) listError("Failed to allocate memory for list.");
    newList->list = (Value*) malloc(sizeof(Value) * size);
    if (newList->list == NULL) listError("Failed to allocate memory for list elements.");
    newList->size = 0;
    newList->capacity = size;
    return newList;
}

void listAddElement(runtimeList* list, Value value) {
    if (list->size == list->capacity) {
        if (list->size >= (UINT32_MAX/2)) listError("List size exceeds maximum size during reallocation.");
        // Double the capacity if the list is full
        list->capacity *= 2;
        Value* newList = (Value*) realloc(list->list, sizeof(Value) * list->capacity);
        if (newList == NULL) listError("Failed to reallocate memory for list elements.");
        list->list = newList;
    }
    list->list[list->size++] = value;
}

uint32_t listAddElementReturnIndex(runtimeList* list, Value value) {
    listAddElement(list, value);
    return list->size - 1;
}

void listInsertElement(runtimeList* list, uint32_t index, Value value) {
    if (index > list->size) listError("List index out of range");

    if (list->size == list->capacity) {
        if (list->size >= (UINT32_MAX/2)) listError("List size exceeds maximum size during reallocation.");
        // Double the capacity if the list is full
        list->capacity *= 2;
        Value* newList = (Value*) realloc(list->list, sizeof(Value) * list->capacity);
        if (newList == NULL) listError("Failed to reallocate memory for list elements.");
        list->list = newList;
    }

    // Shift all elements to the right of the index one position to the right
    memmove(&list->list[index + 1], &list->list[index], sizeof(Value) * (list->size - index));

    list->list[index] = value;
    list->size++;
}

void listRemoveElement(runtimeList* list, uint32_t index) {
    if (index >= list->size) listError("List index out of range");

    // Shift all elements to the right of the index one position to the left
    memmove(&list->list[index], &list->list[index + 1], sizeof(Value) * (list->size - index - 1));

    list->size--;
}

void listSetElement(runtimeList* list, uint32_t index, Value value) {
    if (index >= list->size) listError("List index out of range");
    // Since index is within range of current list size, mark Value being replaced as GC removeRef
    list->list[index] = value;
}

Value listGetElement(runtimeList* list, uint32_t index) {
    if (index >= list->size) listError("List index out of range");
    return list->list[index];
}

bool listContainsElement(runtimeList* list, Value value) {
    for (uint32_t i = 0; i < list->size; i++) {
        if (compareValue(list->list[i], value)) return true;
    }
    return false;
}

uint32_t listIndexOfElement(runtimeList* list, Value value) {
    for (uint32_t i = 0; i < list->size; i++) {
        if (compareValue(list->list[i], value)) return i;
    }
    listError("Element not found in list");
    return 0; // Unreachable
}

void freeRuntimeList(runtimeList* list) {
    // Free the list and the structure
    free(list->list);
    free(list);
}

void printRuntimeList(runtimeList* list) {
    printf("[");
    for (uint32_t i = 0; i < list->size; i++) {
        // Find if print func is defined for the object
        Value obj = list->list[i];
        DSPrintValue(obj);
        if (i != list->size -1) printf(", ");
    }
    printf("]");
}

uint32_t hashObject(Value key) {
    if (VALUE_TYPE(key) == VAL_NUMBER) { // Use number hashString
        double num = VALUE_NUMBER_VALUE(key);
        double r = fmod(num, (double) 1);
        double epsilon = 1e-5; // Tolerance
        while (fabs(r) >= epsilon) {
            num *= 10;
            r = fmod(num, (double) 1);
        }
        return (uint32_t) num;
    } else if (VALUE_TYPE(key) == BUILTIN_STR) { // Use string hashString
        return hashString(VALUE_STR_VALUE(key));
    } else { // Search for hashString function
        Value objHashFunc = ignoreNullGetAttr(key, "hashString");
        if (IS_INTERNAL_NULL(objHashFunc)) dictError("Hash function undefined.");
        Value valueObj = execInput(objHashFunc, key, NULL, 0);
        if (VALUE_TYPE(valueObj) != VAL_NUMBER) dictError("Non number type hashString function return.");
        return (uint32_t) VALUE_NUMBER_VALUE(valueObj);
    }
}

#define MAX_LOAD_FACTOR 0.75

runtimeDict* createRuntimeDict(uint32_t size) {
    runtimeDict* dict = (runtimeDict*) malloc(sizeof(runtimeDict));
    if (dict == NULL) dictError("Failed to allocate memory for dict.");
    dict->tableSize = size;
    dict->numEntries = 0;
    dict->entries = (runtimeDictEntry**) calloc(size, sizeof(runtimeDictEntry*));
    if (dict->entries == NULL) dictError("Failed to allocate memory for dict entries.");
    return dict;
}

void resizeRuntimeDict(runtimeDict* dict, uint32_t newSize) {
    runtimeDictEntry** newEntries = (runtimeDictEntry**) calloc(newSize, sizeof(runtimeDictEntry*));
    if (newEntries == NULL) dictError("Failed to allocate memory for dict entries during resize");
    // Rehash all entries
    for (uint32_t i = 0; i < dict->tableSize; ++i) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry != NULL) {
            uint32_t newHash = hashObject(entry->key) % newSize;
            runtimeDictEntry* nextEntry = entry->next;
            entry->next = newEntries[newHash];
            newEntries[newHash] = entry;
            entry = nextEntry;
        }
    }

    free(dict->entries);
    dict->entries = newEntries;
    dict->tableSize = newSize;
}

void dictInsertElement(runtimeDict* dict, Value key, Value value) {
    uint32_t hash = hashObject(key) % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    while (entry) {
        if (compareValue(entry->key, key)) {
            entry->value = value;  // Overwrite Value if key already exists
            return;
        }
        entry = entry->next;
    }
    // Key does not exist in dict, create new entry
    entry = (runtimeDictEntry*) malloc(sizeof(runtimeDictEntry));
    if (entry == NULL) dictError("Failed to allocate memory for dict entry");
    entry->key = key;
    entry->value = value;
    entry->next = dict->entries[hash];  // Insert at head of linked list
    dict->entries[hash] = entry;
    dict->numEntries++;

    // Check if resize is needed
    if ((float)dict->numEntries / (float) dict->tableSize > MAX_LOAD_FACTOR) {
        if (dict->tableSize >= (UINT32_MAX/2)) dictError("Dict size exceeds maximum size during reallocation");
        resizeRuntimeDict(dict, dict->tableSize * 2);
    }
}

Value dictGetElement(runtimeDict* dict, Value key) {
    uint32_t hash = hashObject(key) % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    while (entry) {
        if (compareValue(entry->key, key)) return entry->value;
        entry = entry->next;
    }
    dictError("Key not found in dictionary");
    return INTERNAL_NULL_VAL;
}

bool dictContainsElement(runtimeDict* dict, Value key) {
    uint32_t hash = hashObject(key) % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    while (entry) {
        if (compareValue(entry->key, key)) return true;
        entry = entry->next;
    }
    return false;
}

void dictRemoveElement(runtimeDict* dict, Value key) {
    uint32_t hash = hashObject(key) % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    runtimeDictEntry* prevEntry = NULL;
    while (entry) {
        if (compareValue(entry->key, key)) {
            if (prevEntry) {
                prevEntry->next = entry->next;
            } else {
                dict->entries[hash] = entry->next;
            }
            free(entry);
            dict->numEntries--;

            return;
        }
        prevEntry = entry;
        entry = entry->next;
    }
    dictError("Key not found in dictionary");
}

Value dictStrGet(runtimeDict* dict, char* key) {
    uint32_t hash = hashString(key) % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    while (entry) {
        if (strcmp(VALUE_STR_VALUE(entry->key), key) == 0) return entry->key;
        entry = entry->next;
    }
    return INTERNAL_NULL_VAL;
}

Value dictNumGet(runtimeDict* dict, double key) {
    double num = key;
    double r = fmod(num, (double) 1);
    double epsilon = 1e-5; // Tolerance
    while (fabs(r) >= epsilon) {
        num *= 10;
        r = fmod(num, (double) 1);
    }
    uint32_t hash = (uint32_t) num % dict->tableSize;
    runtimeDictEntry* entry = dict->entries[hash];
    while (entry) {
        if (VALUE_NUMBER_VALUE(entry->key) == key) return entry->key;
        entry = entry->next;
    }
    return INTERNAL_NULL_VAL;
}

void freeRuntimeDict(runtimeDict* dict) {
    for (uint32_t i = 0; i < dict->tableSize; i++) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry != NULL) {
            runtimeDictEntry* temp = entry;
            entry = entry->next;
            free(temp);
        }
    }
    free(dict->entries);
    free(dict);
}

runtimeSet* createRuntimeSet(uint32_t size) {
    runtimeSet* set = (runtimeSet*) malloc(sizeof(runtimeSet));
    if (set == NULL) setError("Failed to allocate memory for set");
    set->dict = createRuntimeDict(size);
    return set;
}

void setInsertElement(runtimeSet* set, Value key) {
    dictInsertElement(set->dict, key, INTERNAL_NULL_VAL);
}

bool setContainsElement(runtimeSet* set, Value key) {
    return dictContainsElement(set->dict, key);
}

void setRemoveElement(runtimeSet* set, Value key) {
    dictRemoveElement(set->dict, key);
}

void freeRuntimeSet(runtimeSet* set) {
    freeRuntimeDict(set->dict);
    free(set);
}

void printRuntimeSet(runtimeSet* set) {
    runtimeDict* dict = set->dict;
    bool first = true;
    printf("s{");
    for (uint32_t i = 0; i < dict->tableSize; i++) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            DSPrintValue(entry->key);

            entry = entry->next;
        }
    }
    printf("}");
}

void printRuntimeDict(runtimeDict* dict) {
    bool first = true;
    printf("d{");
    for (uint32_t i = 0; i < dict->tableSize; i++) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry) {
            if (first) {
                first = false;
            } else {
                printf(", ");
            }
            DSPrintValue(entry->key);
            printf(":");
            DSPrintValue(entry->value);

            entry = entry->next;
        }
    }
    printf("}");
}

void DSPrintValue(Value val) {
    if (IS_INTERNAL_NULL(val)) {
        printf("NULL");
        return;
    }
    Value printFunc = ignoreNullGetAttr(val, "print");
    if (!IS_INTERNAL_NULL(printFunc)) {
        execInput(printFunc, val, NULL, 0);
    } else if (VALUE_TYPE(val) == BUILTIN_CALLABLE) {
        printf("%s object", (VALUE_CALLABLE_TYPE(val) == method) ? "Method" : "Function");
    } else {
        printf("%s object", VALUE_CLASS(val)->className);
    }
}
