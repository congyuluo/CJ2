//
// Created by congyu on 7/18/23.
//

#ifndef CJ_2_OBJECT_H
#define CJ_2_OBJECT_H

#include "primitiveVars.h"
#include "chunk.h"

#include <stdint.h>

#define LOAD_FACTOR_THRESHOLD 0.75
#define CLASS_ADD_ATTR(c, attrName, attrValue) strValInsert((c)->predefinedAttrs, attrName, attrValue)
#define CLASS_FIND_ATTR(c, attrName) strValFind((c)->predefinedAttrs, attrName)
#define VALUE_TYPE(val) val.type

#define NONE_VAL (Value) { .obj = NULL, .type = VAL_NONE }
#define NUMBER_VAL(n) (Value) { .num = (n), .type = VAL_NUMBER }
#define OBJECT_VAL(o, t) (Value) { .obj = (o), .type = (t) }
#define BOOL_VAL(b) (Value) { .boolean = (b), .type = VAL_BOOL }

#define INTERNAL_NULL_VAL (Value) { .obj = NULL, .type = VAL_INTERNAL_NULL }
#define IS_INTERNAL_NULL(val) ((val).type == VAL_INTERNAL_NULL)

#define VALUE_NUMBER_VALUE(val) val.num
#define VALUE_BOOL_VALUE(val) val.boolean
#define VALUE_STR_VALUE(val) val.obj->primValue.str
#define VALUE_CALLABLE_VALUE(val) val.obj->primValue.call
#define VALUE_CALLABLE_TYPE(val) val.obj->primValue.call->type
#define VALUE_LIST_VALUE(val) val.obj->primValue.list
#define VALUE_DICT_VALUE(val) val.obj->primValue.dict
#define VALUE_SET_VALUE(val) val.obj->primValue.set
#define VALUE_ATTRS(val) val.obj->primValue.afterDefAttributes
#define VALUE_CLASS(val) classArray[VALUE_TYPE(val)]
#define VALUE_OBJ_VAL(val) val.obj

#define IS_SYSTEM_DEFINED_CLASS(c) ((c)->classID < 9)
#define IS_SYSTEM_DEFINED_TYPE(t) ((t) < 9)
#define IS_ITERABLE_VAL(val) ((val).type > 5)
#define IS_MARKABLE_VAL(val) ((val).type > 3)


typedef struct strValueHash strValueHash;
typedef struct objClass objClass;

typedef enum {
    // Internal null value
    VAL_INTERNAL_NULL = 0,
    // Value types
    VAL_NONE = 1,
    VAL_BOOL = 2,
    VAL_NUMBER = 3,
    // Object types
    BUILTIN_CALLABLE = 4,
    BUILTIN_STR = 5,
    // Iterable types
    BUILTIN_LIST = 6,
    BUILTIN_DICT = 7,
    BUILTIN_SET = 8
} ValueType;

typedef enum {
    NONE_INIT_TYPE,
    C_FUNC_INIT_TYPE,
    CHUNK_FUNC_INIT_TYPE,
} initFuncType;

// Class & object definition

extern objClass** classArray;

struct Object {
    union {
        char* str;
        callable* call;
        runtimeList* list;
        runtimeDict* dict;
        runtimeSet* set;
        strValueHash *afterDefAttributes;
    } primValue;
    Object* next;
    uint16_t type;
    uint16_t blockID;
    bool marked;
    bool isConst;
};

struct Value {
    union {
        Object* obj;
        double num;
        bool boolean;
    };
    uint16_t type;
};

struct objClass {
    uint32_t classID;
    char* className;
    objClass* parentClass;
    Value initFunc;
    strValueHash *predefinedAttrs;
    initFuncType initType;
};

// Builtin classes
extern objClass* callableClass;
extern objClass* noneClass;
extern objClass* numClass;
extern objClass* boolClass;
extern objClass* stringClass;
extern objClass* listClass;
extern objClass* dictClass;
extern objClass* setClass;

// strValueHash definition

typedef struct strValueEntry {
    char* key;
    Value value;
    struct strValueEntry* next;
} strValueEntry;

struct strValueHash {
    uint32_t table_size;
    uint32_t num_entries;
    uint32_t history_max_entries;
    strValueEntry** entries;
};

// strValueHash functions

strValueHash* createStrValHashTable(uint32_t table_size);
void deleteStrValHashTable(strValueHash* table);
void strValInsert(strValueHash* table, char* key, Value value);
void strValResizeInsert(strValueHash* table, char* key, Value value);
Value strValFind(strValueHash* table, char* key);
void strValTableDeleteEntry(strValueHash* table, char* key);
void strValResize(strValueHash* table);
void printStrValHash(strValueHash* table, void (*printFunc)(Value));
void printStrValStructure(strValueHash* table);

// Callable functions
callable* createCallable(int in, uint8_t out, void* cFunc, Chunk* func, callableType type);
void deleteCallable(callable* c);

// Object functions
void deleteObject(Object* obj); // Not to be used by user's runtime operations
void deleteConst(Object* obj);
Value getAttr(Value val, char* name);
Value ignoreNullGetAttr(Value val, char* name);

void printPrimitiveValue(Value val);
void printValue(Value val);
void printObject(Object* obj);
#endif //CJ_2_OBJECT_H
