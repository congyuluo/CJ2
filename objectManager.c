//
// Created by congyu on 8/19/23.
//
#include "objectManager.h"
#include "constList.h"
#include "stringHash.h"
#include "runtimeDS.h"
#include "errors.h"
#include "objClass.h"
#include "runtimeMemoryManager.h"

void initObjectManager() {
    initClassArray();
    createConstList();
}

void freeObjectManager() {
    freeConstList();
    freeClassArray();
}

Object* createConstObj(objClass* c) {
    if (isRuntime) objManagerError("Cannot create const object in runtime");
    Object* newObj = addConst();
    newObj->type = c->classID;
    if (IS_SYSTEM_DEFINED_CLASS(c)) {
        newObj->primValue.afterDefAttributes = NULL;
    } else {
        newObj->primValue.afterDefAttributes = createStrValHashTable(OBJECT_ATTR_TABLE_INIT_SIZE);
    }
    newObj->marked = false;
    newObj->isConst = true;
    return newObj;
}

Object* createConstCallableObject(callable* call) {
    // Create object
    Object* newObj = createConstObj(callableClass);
    newObj->primValue.call = call;
    return newObj;
}

Object* createConstStringObject(char* value) {
    // Create object
    Object* newObj = createConstObj(stringClass);
    newObj->primValue.str = addReference(value);
    return newObj;
}

Object* createConstListObject() {
    // Create object
    Object* newObj = createConstObj(listClass);
    newObj->primValue.list = createRuntimeList(RUNTIME_LIST_INIT_SIZE);
    return newObj;
}

Object* createConstDictObject() {
    // Create object
    Object* newObj = createConstObj(dictClass);
    newObj->primValue.dict = createRuntimeDict(RUNTIME_DICT_INIT_SIZE);
    return newObj;
}

Object* createConstSetObject() {
    // Create object
    Object* newObj = createConstObj(setClass);
    newObj->primValue.set = createRuntimeSet(RUNTIME_SET_INIT_SIZE);
    return newObj;
}

Object* createRuntimeObj(objClass* c) {
    if (!isRuntime) objManagerError("Cannot create runtime object prior to runtime");
    // Get slot
    Object* newObj = newObjectSlot();

    newObj->type = c->classID;
    if (IS_SYSTEM_DEFINED_CLASS(c)) {
        newObj->primValue.afterDefAttributes = NULL;
    } else {
        newObj->primValue.afterDefAttributes = createStrValHashTable(OBJECT_ATTR_TABLE_INIT_SIZE);
    }
    newObj->marked = false;
    newObj->isConst = false;
    return newObj;
}

Object* createRuntimeStringObject(char* value) {
    Object* newObj = createRuntimeObj(stringClass);
    newObj->primValue.str = addReference(value);
    return newObj;
}

Object* createRuntimeListObject() {
    Object* newObj = createRuntimeObj(listClass);
    newObj->primValue.list = createRuntimeList(RUNTIME_LIST_INIT_SIZE);
    return newObj;
}

Object* createRuntimeDictObject() {
    Object* newObj = createRuntimeObj(dictClass);
    newObj->primValue.dict = createRuntimeDict(RUNTIME_DICT_INIT_SIZE);
    return newObj;
}

Object* createRuntimeSetObject() {
    Object* newObj = createRuntimeObj(setClass);
    newObj->primValue.set = createRuntimeSet(RUNTIME_SET_INIT_SIZE);
    return newObj;
}
