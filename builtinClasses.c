//
// Created by congyu on 7/20/23.
//

#include "builtinClasses.h"
#include "errors.h"
#include "runtimeDS.h"
#include "objectManager.h"
#include "objClass.h"
#include "object.h"

#include <math.h>
#include <string.h>
#include <time.h>

#define DEF_BUILTIN_CFUNC_INIT_CLASS(name, classID, in, out, cFunc) createClass(name, classID, OBJECT_VAL(createConstCallableObject(CREATE_CFUNC_METHOD(in, out, cFunc)), BUILTIN_CALLABLE), NULL, C_FUNC_INIT_TYPE)
#define DEF_BUILTIN_CFUNC_METHOD_VALUE(in, out, cFunc) OBJECT_VAL(createConstCallableObject(CREATE_CFUNC_METHOD(in, out, cFunc)), BUILTIN_CALLABLE)
#define DEF_BUILTIN_CFUNC_FUNCTION_VALUE(in, out, cFunc) OBJECT_VAL(createConstCallableObject(CREATE_CFUNC_FUNCTION(in, out, cFunc)), BUILTIN_CALLABLE)

#define CHECK_NUM_TYPE(val) if (VALUE_TYPE(val) != VAL_NUMBER) runtimeError("Value is not of type num")

// Builtin classes
objClass* callableClass;
objClass* noneClass;
objClass* numClass;
objClass* boolClass;
objClass* stringClass;
objClass* listClass;
objClass* dictClass;
objClass* setClass;

void addGlobalReference(refTable* globalRefTable, runtimeList* globalRefList, Value val, char* name) {
    if (refTableContains(globalRefTable, name)) compilationError(0, 0, 0, "global reference already exists");
    uint16_t index = getRefIndex(globalRefTable, name);
    if (index != listAddElementReturnIndex(globalRefList, val)) compilationError(0, 0, 0, "reference table and list mismatch");
}

Value printPrim(Value self, Value* args, int numArgs) {
    printPrimitiveValue(self);
}

Value equalPrim(Value self, Value* args, int numArgs) {
    Value otherObj = args[0];
    if (VALUE_TYPE(otherObj) != VALUE_TYPE(self)) return BOOL_VAL(false);
    bool resultBool;
    switch (VALUE_TYPE(self)) {
        case BUILTIN_CALLABLE:
//            resultBool = self->objID == otherObj->objID;
            break;
        case VAL_NONE:
            resultBool = true;
            break;
        case VAL_NUMBER: {
            double diff = VALUE_NUMBER_VALUE(self) - VALUE_NUMBER_VALUE(otherObj);
            double epsilon = 1e-9; // Tolerance
            resultBool = fabs(diff) < epsilon ? true : false;
            break;
        }
        case VAL_BOOL:
            resultBool = VALUE_BOOL_VALUE(self) == VALUE_BOOL_VALUE(otherObj);
            break;
        case BUILTIN_STR:
            resultBool = strcmp(VALUE_STR_VALUE(self), VALUE_STR_VALUE(otherObj)) == 0;
            break;
        default:
            runtimeError("Unsupported type for equalPrim");
    }
    return resultBool ? BOOL_VAL(true) : BOOL_VAL(false);
}

// Runtime data structures

// List

Value initList(Value self, Value* args, int numArgs) {
    VALUE_LIST_VALUE(self) = createRuntimeList(RUNTIME_LIST_INIT_SIZE);
    for (int i=0; i<numArgs; i++) listAddElement(VALUE_LIST_VALUE(self), args[i]);
}

Value listAdd(Value self, Value* args, int numArgs) {
    listAddElement(VALUE_LIST_VALUE(self), args[0]);
}

Value listInsert(Value self, Value* args, int numArgs) {
    CHECK_NUM_TYPE(args[0]);
    listInsertElement(VALUE_LIST_VALUE(self), VALUE_NUMBER_VALUE(args[0]), args[1]);
}

Value listSet(Value self, Value* args, int numArgs) {
    CHECK_NUM_TYPE(args[0]);
    listSetElement(VALUE_LIST_VALUE(self),VALUE_NUMBER_VALUE(args[0]), args[1]);
}

Value listRemove(Value self, Value* args, int numArgs) {
    CHECK_NUM_TYPE(args[0]);
    listRemoveElement(VALUE_LIST_VALUE(self),VALUE_NUMBER_VALUE(args[0]));
}

Value listGet(Value self, Value* args, int numArgs) {
    CHECK_NUM_TYPE(args[0]);
    return listGetElement(VALUE_LIST_VALUE(self), VALUE_NUMBER_VALUE(args[0]));
}

Value listContains(Value self, Value* args, int numArgs) {
    bool result = listContainsElement(VALUE_LIST_VALUE(self), args[0]);
    return result ? BOOL_VAL(true) : BOOL_VAL(false);
}

Value listIndexOf(Value self, Value* args, int numArgs) {
    int result = listIndexOfElement(VALUE_LIST_VALUE(self), args[0]);
    Value resultObj = NUMBER_VAL(result);
    return resultObj;
}

// Dict
Value initDict(Value self, Value* args, int numArgs) {
    if (numArgs % 2 != 0) runtimeError("Dict init must have even number of arguments");
    VALUE_DICT_VALUE(self) = createRuntimeDict(RUNTIME_DICT_INIT_SIZE);
    for (int i=0; i<numArgs; i+=2) dictInsertElement(VALUE_DICT_VALUE(self), args[i], args[i+1]);
}

Value dictInsert(Value self, Value* args, int numArgs) {
    dictInsertElement(VALUE_DICT_VALUE(self), args[0], args[1]);
}

Value dictGet(Value self, Value* args, int numArgs) {
    dictGetElement(VALUE_DICT_VALUE(self), args[0]);
}

Value dictContains(Value self, Value* args, int numArgs) {
    bool result = dictContainsElement(VALUE_DICT_VALUE(self), args[0]);
    return result ? BOOL_VAL(true) : BOOL_VAL(false);
}

Value dictRemove(Value self, Value* args, int numArgs) {
    dictRemoveElement(VALUE_DICT_VALUE(self), args[0]);
}

// Set
Value initSet(Value self, Value* args, int numArgs) {
    VALUE_SET_VALUE(self) = createRuntimeSet(RUNTIME_SET_INIT_SIZE);
    for (int i=0; i<numArgs; i++) setInsertElement(VALUE_SET_VALUE(self), args[i]);
}

Value setInsert(Value self, Value* args, int numArgs) {
    setInsertElement(VALUE_SET_VALUE(self), args[0]);
}

Value setContains(Value self, Value* args, int numArgs) {
    bool result = setContainsElement(VALUE_SET_VALUE(self), args[0]);
    return result ? BOOL_VAL(true) : BOOL_VAL(false);
}

Value setRemove(Value self, Value* args, int numArgs) {
    setRemoveElement(VALUE_SET_VALUE(self), args[0]);
}

Value print(Value self, Value* args, int numArgs) {
    for (uint32_t i=0; i<numArgs; i++) DSPrintValue(args[i]);
}

Value println(Value self, Value* args, int numArgs) {
    for (uint32_t i=0; i<numArgs; i++) DSPrintValue(args[i]);
    printf("\n");
}

Value type(Value self, Value* args, int numArgs) {
    return OBJECT_VAL(createRuntimeStringObject(VALUE_CLASS(args[0])->className), BUILTIN_STR);
}

Value input(Value self, Value* args, int numArgs) {
    if (numArgs == 1) printf("%s", VALUE_STR_VALUE(args[0]));
    if (numArgs != 0 && numArgs != 1) runtimeError("Invalid number of arguments for input");

    char *line = NULL;
    size_t len = 0;
    getline(&line, &len, stdin);
    // Remove the trailing newline character
    size_t line_length = strlen(line);
    if (line_length > 0 && line[line_length - 1] == '\n') {
        line[line_length - 1] = '\0';
    }
    Value result = OBJECT_VAL(createRuntimeStringObject(line), BUILTIN_STR);
    free(line);
    return result;
}

Value clockFunc(Value self, Value* args, int numArgs) {
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value sayHi(Value self, Value* args, int numArgs) {
    printf("Hi\n");
}

void constructBuiltinClasses(refTable* globalRefTable, runtimeList* globalRefList, refTable* globalClassTable) {

    callableClass = createClass("callable", BUILTIN_CALLABLE, INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);

    // Internal NULL class
    objClass* internalNull = createClass("Internal NULL", getRefIndex(globalClassTable, "Internal NULL"), INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);

    // None class
    noneClass = createClass("none", getRefIndex(globalClassTable, "none"), INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);
    CLASS_ADD_ATTR(noneClass, "_eq", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &equalPrim));

    // Bool class
    boolClass = createClass("bool", getRefIndex(globalClassTable, "bool"), INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);
    CLASS_ADD_ATTR(boolClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(boolClass, "_eq", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &equalPrim));

    // Num class
    numClass = createClass("num", getRefIndex(globalClassTable, "num"), INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);
    CLASS_ADD_ATTR(numClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(numClass, "_eq", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &equalPrim));

    // Callable class
    getRefIndex(globalClassTable, "callable");
    CLASS_ADD_ATTR(callableClass, "_eq", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &equalPrim));

    // String class
    stringClass = createClass("str", getRefIndex(globalClassTable, "str"), INTERNAL_NULL_VAL, NULL, NONE_INIT_TYPE);
    CLASS_ADD_ATTR(stringClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(stringClass, "_eq", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &equalPrim));

    // List class
    listClass = DEF_BUILTIN_CFUNC_INIT_CLASS("list", getRefIndex(globalClassTable, "list"), -1, 0, &initList);
    CLASS_ADD_ATTR(listClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(listClass, "add", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 0, &listAdd));
    CLASS_ADD_ATTR(listClass, "insert", DEF_BUILTIN_CFUNC_METHOD_VALUE(2, 0, &listInsert));
    CLASS_ADD_ATTR(listClass, "set", DEF_BUILTIN_CFUNC_METHOD_VALUE(2, 0, &listSet));
    CLASS_ADD_ATTR(listClass, "remove", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 0, &listRemove));
    CLASS_ADD_ATTR(listClass, "get", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &listGet));
    CLASS_ADD_ATTR(listClass, "contains", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &listContains));
    CLASS_ADD_ATTR(listClass, "index", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &listIndexOf));

    // Dict class
    dictClass = DEF_BUILTIN_CFUNC_INIT_CLASS("dict", getRefIndex(globalClassTable, "dict"), -1, 0, &initDict);
    CLASS_ADD_ATTR(dictClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(dictClass, "add", DEF_BUILTIN_CFUNC_METHOD_VALUE(2, 0, &dictInsert));
    CLASS_ADD_ATTR(dictClass, "get", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &dictGet));
    CLASS_ADD_ATTR(dictClass, "contains", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &dictContains));
    CLASS_ADD_ATTR(dictClass, "remove", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 0, &dictRemove));

    // Set class
    setClass = DEF_BUILTIN_CFUNC_INIT_CLASS("set", getRefIndex(globalClassTable, "set"), -1, 0, &initSet);
    CLASS_ADD_ATTR(setClass, "print", DEF_BUILTIN_CFUNC_METHOD_VALUE(0, 0, &printPrim));
    CLASS_ADD_ATTR(setClass, "add", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 0, &setInsert));
    CLASS_ADD_ATTR(setClass, "contains", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 1, &setContains));
    CLASS_ADD_ATTR(setClass, "remove", DEF_BUILTIN_CFUNC_METHOD_VALUE(1, 0, &setRemove));

    // Builtin functions
    Value printFunc = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(-1, 0, &print);
    addGlobalReference(globalRefTable, globalRefList, printFunc, "print");

    Value printlnFunc = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(-1, 0, &println);
    addGlobalReference(globalRefTable, globalRefList, printlnFunc, "println");

    Value typeFunc = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(1, 1, &type);
    addGlobalReference(globalRefTable, globalRefList, typeFunc, "type");

    Value inputFunc = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(-1, 1, &input);
    addGlobalReference(globalRefTable, globalRefList, inputFunc, "input");

    Value clockFuncVal = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(0, 1, &clockFunc);
    addGlobalReference(globalRefTable, globalRefList, clockFuncVal, "clock");

    Value sayHiFuncVal = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(0, 0, &sayHi);
    addGlobalReference(globalRefTable, globalRefList, sayHiFuncVal, "sayHi");
}

void loadUserFunctions(refTable* globalRefTable, runtimeList* globalRefList, userFunction* userFunctions, uint32_t funcCount) {
    for (uint32_t i=0; i<funcCount; i++) {
        Value currFunc = DEF_BUILTIN_CFUNC_FUNCTION_VALUE(userFunctions[i].in, userFunctions[i].out, userFunctions[i].cFunc);
        addGlobalReference(globalRefTable, globalRefList, currFunc, userFunctions[i].name);
    }
    printf("User functions loaded\n");
}