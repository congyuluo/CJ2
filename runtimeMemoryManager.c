//
// Created by congyu on 8/20/23.
//

#include "runtimeMemoryManager.h"
#include "errors.h"
#include "vm.h"

RuntimeMemoryManager* memoryManager;

Object* rtHead;

static inline void newBlock() {
#ifdef PRINT_MEMORY_INFO
    printf("Allocating new block\n");
#endif
    RuntimeBlock* newBlock = (RuntimeBlock*) malloc(sizeof(RuntimeBlock));
    if (newBlock == NULL) objManagerError("Memory allocation failed for new block");
    newBlock->nextBlock = memoryManager->headBlock;
    memoryManager->headBlock = newBlock;
    uint16_t newBlockID = newBlock->nextBlock == NULL ? 0 : newBlock->nextBlock->blockID + 1;
    newBlock->blockID = newBlockID;
    // Insert new free slots
    Object** stackTop = memoryManager->freeStackTop;
    Object* newSlot = (Object*) newBlock->block;
    for (int i = 0; i < RUNTIME_BLOCK_SIZE; i++) {
        newSlot->blockID = newBlockID;
        *stackTop++ = newSlot++;
    }
    memoryManager->freeStackTop = stackTop;
}

void initMemoryManager() {
    memoryManager = (RuntimeMemoryManager*) malloc(sizeof(RuntimeMemoryManager));
    if (memoryManager == NULL) objManagerError("Memory allocation failed for memory manager");
    memoryManager->freeStackTop = memoryManager->freeStack;
    memoryManager->headBlock = NULL;
    // Init runtime head
    rtHead = NULL;
    // Allocate head block
    newBlock();
}

void freeMemoryManager() {
    RuntimeBlock* currBlock = memoryManager->headBlock;
    while (currBlock != NULL) {
        RuntimeBlock* nextBlock = currBlock->nextBlock;
        free(currBlock);
        currBlock = nextBlock;
    }
    free(memoryManager);
}

// Forward declaration
static inline void collectGarbage();

Object* newObjectSlot() {
    // First attempt to free garbage if free stack is empty
    if (memoryManager->freeStackTop == memoryManager->freeStack) collectGarbage();
    // If free stack is still empty, allocate new block
    if (memoryManager->freeStackTop == memoryManager->freeStack) newBlock();
    Object* newSlot = *--memoryManager->freeStackTop;
    newSlot->next = rtHead;
    rtHead = newSlot;
    return newSlot;
}

// Print function
void printRTLL() {
    Object* currObj = rtHead;
    while (currObj != NULL) {
        printObject(currObj);
        printf("\n");
        currObj = currObj->next;
    }
}

// Forward declaration
static inline void iterateValue(Value val);

void iterateList(Value val) {
    runtimeList* list = VALUE_LIST_VALUE(val);
    Value* currValPtr = list->list;
    for (int i=0; i<list->size; i++) {
        Value currVal = *currValPtr;
        if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
            Object* currObj = VALUE_OBJ_VAL(currVal);
            if (!(currObj->isConst || currObj->marked)) {
                currObj->marked = true;
                if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
            }
        }
        currValPtr++;
    }
}

void iterateDict(Value val) {
    runtimeDict* dict = VALUE_DICT_VALUE(val);
    for (uint32_t i=0; i < dict->tableSize; i++) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry) {
            Value currVal = entry->key;
            if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
                Object* currObj = VALUE_OBJ_VAL(currVal);
                if (!(currObj->isConst || currObj->marked)) {
                    currObj->marked = true;
                    if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
                }
            }
            currVal = entry->value;
            if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
                Object* currObj = VALUE_OBJ_VAL(currVal);
                if (!(currObj->isConst || currObj->marked)) {
                    currObj->marked = true;
                    if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
                }
            }
            entry = entry->next;
        }
    }
}

void iterateSet(Value val) {
    runtimeSet* set = VALUE_SET_VALUE(val);
    runtimeDict* dict = set->dict;
    for (uint32_t i=0; i < dict->tableSize; i++) {
        runtimeDictEntry* entry = dict->entries[i];
        while (entry) {
            Value currVal = entry->key;
            if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
                Object* currObj = VALUE_OBJ_VAL(currVal);
                if (!(currObj->isConst || currObj->marked)) {
                    currObj->marked = true;
                    if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
                }
            }
            entry = entry->next;
        }
    }
}

void iterateStrObjHashTable(strValueHash* table) {
    for (uint32_t i=0; i < table->table_size; i++) {
        strValueEntry* entry = table->entries[i];
        while (entry != NULL) {
            Value currVal = entry->value;
            if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
                Object* currObj = VALUE_OBJ_VAL(currVal);
                if (!(currObj->isConst || currObj->marked)) {
                    currObj->marked = true;
                    if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
                }
            }
            entry = entry->next;
        }
    }
}

static inline void iterateValue(Value val) {
    if (!IS_SYSTEM_DEFINED_TYPE(val.type)) iterateStrObjHashTable(VALUE_ATTRS(val));
    // Runtime data structure attributes
    switch (val.type) {
        case BUILTIN_LIST:
            iterateList(val);
            break;
        case BUILTIN_DICT:
            iterateDict(val);
            break;
        case BUILTIN_SET:
            iterateSet(val);
            break;
        default:
            GCError("Invalid value type for iteration");
    }
}

static inline void markObject() {
    VM* currVM = vm;
    // Iterate stack
    Value* currStackPtr = currVM->stack;
    Value currVal = *currStackPtr;
    while (currStackPtr != currVM->stackTop) {
        if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
            Object* currObj = VALUE_OBJ_VAL(currVal);
            if (!(currObj->isConst || currObj->marked)) {
                currObj->marked = true;
                if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
            }
        }
        currVal = *currStackPtr++;
    }
    // Iterate global ref array
    currStackPtr = currVM->globalRefArray;
    currVal = *currStackPtr;
    for (int i=0; i<vm->globalRefCount; i++) {
        if (!IS_INTERNAL_NULL(currVal) && IS_MARKABLE_VAL(currVal)) {
            Object* currObj = VALUE_OBJ_VAL(currVal);
            if (!(currObj->isConst || currObj->marked)) {
                currObj->marked = true;
                if (IS_ITERABLE_VAL(currVal)) iterateValue(currVal);
            }
        }
        currVal = *currStackPtr++;
    }
}

static inline void sweepObject() {
#ifdef PRINT_GC_INFO
    uint32_t removedCount = 0;
#endif
#ifdef PRINT_GC_REMOVAL
    uint32_t totalCount = 0;
#endif
//    uint64_t blockBitMap = 0;
    Object* currObj = rtHead;
    Object* prevObj = NULL;
    while (currObj != NULL) {
        if (currObj->marked) { // Marked object
            // Unmark object
            currObj->marked = false;
            // Move to next object
            prevObj = currObj;
            currObj = currObj->next;
        } else { // Unmarked object
#ifdef PRINT_GC_REMOVAL
            printf("Removed Object [%u]: ", totalCount);
            printObject(currObj);
            printf("\n");
#endif
#ifdef PRINT_GC_INFO
            removedCount++;
#endif
            // LL delete
            if (prevObj == NULL) {
                rtHead = currObj->next;
            } else {
                prevObj->next = currObj->next;
            }
            Object* nextObj = currObj->next;
            // Push to free stack
            *memoryManager->freeStackTop++ = currObj;
            currObj = nextObj;
        }
#ifdef PRINT_GC_REMOVAL
        totalCount++;
#endif
    }
#ifdef PRINT_GC_INFO
    printf("GC removed %u objects\n", removedCount);
#endif
}

static inline void collectGarbage() {
    markObject();
    sweepObject();
}
