//
// Created by congyu on 8/19/23.
//

#include "objClass.h"
#include "stringHash.h"
#include "errors.h"
#include "common.h"

uint32_t classCount = 0;

objClass** classArray;

objClass* createClass(char* name, uint32_t classID, Value initFunc, objClass* pClass, initFuncType initType) {
    // Create class
    objClass* newClass = malloc(sizeof(objClass));
    classArray[classID] = newClass;

    if (newClass == NULL) objHashError("Memory allocation for class failed");
    newClass->classID = classID;
    newClass->className = addReference(name);
    newClass->parentClass = pClass;
    newClass->initFunc = initFunc;
    newClass->predefinedAttrs = createStrValHashTable(CLASS_ATTR_TABLE_INIT_SIZE);
    newClass->initType = initType;

    return newClass;
}

void initClassArray() {
    classArray = malloc(sizeof(objClass*) * MAX_CLASS_NUM);
    if (classArray == NULL) objHashError("Memory allocation for class array failed");
}

void setTotalClassCount(uint32_t count) {
    classCount = count;
#ifdef PRINT_CLASS_AFTER_INIT
    printf("\n");
    for (int i = 0; i < classCount; i++) printf("Class %d: %s\n", i, classArray[i]->className);
    printf("Total class count: %d\n\n", classCount);
#endif
}

void deleteClass(objClass* c) {
    removeReference(c->className);
    deleteStrValHashTable(c->predefinedAttrs);
    free(c);
}

void freeClassArray() {
    for (int i = 0; i < classCount; i++) deleteClass(classArray[i]);
    free(classArray);
    classCount = 0;
}

