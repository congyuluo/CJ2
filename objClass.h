//
// Created by congyu on 8/19/23.
//

#ifndef CJ_2_OBJCLASS_H
#define CJ_2_OBJCLASS_H

#include "object.h"

void printObjClass(Object* c);

// Class functions

objClass* createClass(char* name, uint32_t classID, Value initFunc, objClass* pClass, initFuncType initType);
void deleteClass(objClass* c);
void initClassArray();
void setTotalClassCount(uint32_t count);
void freeClassArray();


#endif //CJ_2_OBJCLASS_H
