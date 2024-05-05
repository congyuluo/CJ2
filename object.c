 //
// Created by congyu on 7/18/23.
//

#include "object.h"
#include "common.h"
#include "errors.h"
#include "stringHash.h"

#include <string.h>
#include <assert.h>
#include <math.h>

strValueHash* createStrValHashTable(uint32_t table_size) {
    strValueHash* table = malloc(sizeof(strValueHash));
    if (table == NULL) objHashError("Memory allocation failed.\n");
    table->table_size = table_size;
    table->num_entries = 0;
    table->history_max_entries = 0;
    table->entries = calloc(table->table_size, sizeof(strValueEntry*));
    if (table->entries == NULL) objHashError("Memory allocation failed.\n");

    return table;
}

void deleteStrValHashTable(strValueHash* table) {
    assert(table != NULL);

    for (uint32_t i = 0; i < table->table_size; i++) {
        strValueEntry* entry = table->entries[i];
        while (entry != NULL) {
            strValueEntry* next = entry->next;
            removeReference(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(table->entries);
    free(table);
}

void strValResizeInsert(strValueHash* table, char* key, Value value) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    strValueEntry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            // Marking remove reference, replace Value.
            Value removedValue = entry->value;
            entry->value = value;

//            GCRemoveRef(removedValue);
            return;
        }
        entry = entry->next;
    }

    entry = malloc(sizeof(strValueEntry));
    if (entry == NULL) objHashError("Memory allocation for strObjTable entry failed");
    entry->key = addReference(key);
    entry->value = value;
    entry->next = table->entries[pos];
    table->entries[pos] = entry;

    table->num_entries++;

    if ((float)table->num_entries / (float)table->table_size > LOAD_FACTOR_THRESHOLD) strValResize(table);
}

void strValInsert(strValueHash* table, char* key, Value value) {
    strValResizeInsert(table, key, value);
    table->history_max_entries++;
    if (table->history_max_entries >= UINT32_MAX-1) objHashError("StrObjTable history_max_entries overflow during insert");
}

Value strValFind(strValueHash* table, char* key) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    strValueEntry* entry = table->entries[pos];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return INTERNAL_NULL_VAL;
}

void strValTableDeleteEntry(strValueHash* table, char* key) {
    assert(table != NULL);
    assert(key != NULL);

    uint32_t pos = hashString(key) % table->table_size;
    strValueEntry** p = &(table->entries[pos]);

    while (*p != NULL) {
        if (strcmp((*p)->key, key) == 0) {
            strValueEntry* entry = *p;
            *p = entry->next;
            removeReference(entry->key);
            // GC remove reference
            Value removedValue = entry->value;
            free(entry);
            table->num_entries--;

//            GCRemoveRef(removedValue);
            return;
        }
        p = &((*p)->next);
    }
    objHashError("Key not found in delete");
}

void strValResize(strValueHash* table) {
    assert(table != NULL);

    if (table->table_size >= UINT32_MAX/2) objHashError("StrObjTable exceeds max size during resize");

    uint32_t old_table_size = table->table_size;
    strValueEntry** old_entries = table->entries;

    table->table_size *= 2;
    table->num_entries = 0;
    table->entries = calloc(table->table_size, sizeof(strValueEntry*));
    if (table->entries == NULL) objHashError("Memory allocation failed during StrObjTable resize");

    for (uint32_t i = 0; i < old_table_size; i++) {
        strValueEntry* entry = old_entries[i];
        while (entry != NULL) {
            strValResizeInsert(table, entry->key, entry->value);
            strValueEntry* next = entry->next;
            removeReference(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(old_entries);
}

void printStrValHash(strValueHash* table, void (*printFunc)(Value)) {
    for (uint32_t i = 0; i < table->table_size; i++) {
        strValueEntry* entry = table->entries[i];
        while (entry != NULL) {
            if (printFunc == NULL) {
                printf("Key: \"%s\"", entry->key);
            } else {
                printf("Key: \"%s\", ", entry->key);
                printFunc(entry->value);
                printf("\n");
            }

            entry = entry->next;
        }
    }
    printf("Number of Entries: %u, Table Size: %u, History max entries: %u\n\n", table->num_entries, table->table_size, table->history_max_entries);
}

void printStrValStructure(strValueHash* table) {
    for (uint32_t i = 0; i < table->table_size; i++) {
        strValueEntry* entry = table->entries[i];
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

void printObjClass(objClass* oc) {
    if (oc == NULL) {
        printf("NULL\n");
        return;
    }

    printf("Class: [");
    printf("classID: %u", oc->classID);
    if (oc->parentClass == NULL) {
        printf(", parentClass: none");
    } else {
        printf(", parentClassID: %d", oc->parentClass->classID);
    }
    printf(", initType: ");
    switch (oc->initType) {
        case NONE_INIT_TYPE:
            printf("none");
            break;
        case C_FUNC_INIT_TYPE:
            printf("c_func_init");
            break;
        case CHUNK_FUNC_INIT_TYPE:
            printf("chunk_func_init");
            break;
        default:
            objHashError("Invalid initType in printObjClass()");
    }
    printf(", className: \"%s\"]\n", oc->className);

    // Print attributes
    strValueHash* table = oc->predefinedAttrs;
    printf("Attributes:\n");
    for (uint32_t i = 0; i < table->table_size; i++) {
        strValueEntry* entry = table->entries[i];
        while (entry != NULL) {
            printf("    Key: \"%s\" -> ", entry->key);
            printValue(entry->value);
            printf("\n");

            entry = entry->next;
        }
    }
    printf("    Number of Entries: %u, Table Size: %u\n\n", table->num_entries, table->table_size);
}

void deleteObject(Object* obj) {
    // Avoid freeing afterDefAttrs if is NULL (system defined class)
    if (!IS_SYSTEM_DEFINED_TYPE(obj->type)) {
        strValueHash* table = obj->primValue.afterDefAttributes;

        for (uint32_t i = 0; i < table->table_size; i++) {
            strValueEntry* entry = table->entries[i];
            while (entry != NULL) {
                strValueEntry* next = entry->next;
                removeReference(entry->key);
                free(entry);
                entry = next;
            }
        }

        free(table->entries);
        free(table);
    } else {
        switch (obj->type) {
            case BUILTIN_CALLABLE:
                deleteCallable(obj->primValue.call);
                break;
            case BUILTIN_STR:
                removeReference(obj->primValue.str);
                break;
            case BUILTIN_LIST:
                freeRuntimeList(obj->primValue.list);
                break;
            case BUILTIN_DICT:
                freeRuntimeDict(obj->primValue.dict);
                break;
            case BUILTIN_SET:
                freeRuntimeSet(obj->primValue.set);
                break;
            default:
                break;
        }
    }
    free(obj);
}

void deleteConst(Object* obj) {
    assert(obj != NULL);
    // Avoid freeing afterDefAttrs if is NULL (system defined class)
    if (!IS_SYSTEM_DEFINED_TYPE(obj->type)) {
        strValueHash* table = obj->primValue.afterDefAttributes;

        for (uint32_t i = 0; i < table->table_size; i++) {
            strValueEntry* entry = table->entries[i];
            while (entry != NULL) {
                strValueEntry* next = entry->next;
                removeReference(entry->key);
                free(entry);
                entry = next;
            }
        }

        free(table->entries);
        free(table);
    } else {
        switch (obj->type) {
            case BUILTIN_CALLABLE:
                deleteCallable(obj->primValue.call);
                break;
            case BUILTIN_STR:
                removeReference(obj->primValue.str);
                break;
            case BUILTIN_LIST:
                freeRuntimeList(obj->primValue.list);
                break;
            case BUILTIN_DICT:
                freeRuntimeDict(obj->primValue.dict);
                break;
            case BUILTIN_SET:
                freeRuntimeSet(obj->primValue.set);
                break;
            default:
                break;
        }
    }
}

void printPrimitiveValue(Value val) {
    switch (VALUE_TYPE(val)) {
        case VAL_NONE:
            printf("None");
            break;
        case VAL_NUMBER: {
            double r = fmod(VALUE_NUMBER_VALUE(val), 1.0);
            double epsilon = 1e-9; // Tolerance
            if (fabs(r) < epsilon) {
                printf("%d", (int) VALUE_NUMBER_VALUE(val));
            } else {
                printf("%.10f", VALUE_NUMBER_VALUE(val));
            }
            break;
        }
        case VAL_BOOL:
            printf(VALUE_BOOL_VALUE(val) ? "true" : "false");
            break;
        case BUILTIN_STR:
            printf("%s", VALUE_STR_VALUE(val));
            break;
        case BUILTIN_CALLABLE:
            printf(VALUE_CALLABLE_VALUE(val)->func == NULL ? "Callable" : "C_Callable");
            break;
        case BUILTIN_LIST:
            printRuntimeList(VALUE_LIST_VALUE(val));
            break;
        case BUILTIN_DICT:
            printRuntimeDict(VALUE_DICT_VALUE(val));
            break;
        case BUILTIN_SET:
            printRuntimeSet(VALUE_SET_VALUE(val));
            break;
        default:
            varError("Invalid primitive type");
            break;
    }
}

callable* createCallable(int in, uint8_t out, void* cFunc, Chunk* func, callableType type) {
    // Check if only one func is null
    if ((cFunc == NULL && func == NULL) || (cFunc != NULL && func != NULL)) {
        callableError("Exactly one of cFunc and func must be NULL");
    }
    callable* initFunc = malloc(sizeof(callable));
    if (initFunc == NULL) callableError("Memory allocation for callable failed");
    initFunc->in = in;
    initFunc->out = out;
    initFunc->cFunc = cFunc;
    initFunc->func = func;
    initFunc->type = type;
    return initFunc;
}

void deleteCallable(callable* c) {
    if (c->func != NULL) freeChunk(c->func);
    free(c);
}

Value getAttr(Value val, char* name) {
    if (IS_INTERNAL_NULL(val)) objHashError("Null object called on get attr.");
    Value value = INTERNAL_NULL_VAL;
    if (!IS_SYSTEM_DEFINED_TYPE(val.type)) value = strValFind(VALUE_ATTRS(val), name);
    if (!IS_INTERNAL_NULL(value)) return value;
    objClass* p_class = VALUE_CLASS(val);
    while (p_class != NULL && IS_INTERNAL_NULL(value)) {
        value = CLASS_FIND_ATTR(p_class, name);
        p_class = p_class->parentClass;
    }
    if (IS_INTERNAL_NULL(value)) objHashError("Attribute not found.");
    return value;
}

Value ignoreNullGetAttr(Value val, char* name) {
    if (IS_INTERNAL_NULL(val)) objHashError("Null object called on get attr.");
    Value value = INTERNAL_NULL_VAL;
    if (!IS_SYSTEM_DEFINED_TYPE(val.type)) value = strValFind(VALUE_ATTRS(val), name);
    if (!IS_INTERNAL_NULL(value)) return value;
    objClass* p_class = VALUE_CLASS(val);
    while (p_class != NULL && IS_INTERNAL_NULL(value)) {
        value = CLASS_FIND_ATTR(p_class, name);
        p_class = p_class->parentClass;
    }
    return value;
}

void printValue(Value val) {
    if (IS_INTERNAL_NULL(val)) {
        printf("NULL");
        return;
    }
    printf("Object: [");
    if (VALUE_TYPE(val) == BUILTIN_CALLABLE) {
        printf(" type: %s", (VALUE_CALLABLE_TYPE(val) == method) ? "method" : "function");
        printf(" [in: %d, out: %d]]" , VALUE_CALLABLE_VALUE(val)->in, VALUE_CALLABLE_VALUE(val)->out);
    } else {
        printf(" type: \"%s\"]", VALUE_CLASS(val)->className);
    }
}

void printObject(Object* obj) {
    if (obj == NULL) {
        printf("NULL");
        return;
    }
    printf("Object: [");
    if (obj->type == BUILTIN_CALLABLE) {
        printf(" type: %s", (obj->primValue.call->type == method) ? "method" : "function");
        printf(" [in: %d, out: %d]]" , obj->primValue.call->in, obj->primValue.call->out);
    } else {
        printf(" type: \"%s\"]", classArray[obj->type]->className);
    }
}
