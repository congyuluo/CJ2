//
// Created by congyu on 8/19/23.
//

#ifndef CJ_2_OBJECTMANAGER_H
#define CJ_2_OBJECTMANAGER_H

#include "object.h"

void initObjectManager();
void freeObjectManager();

Object* createConstObj(objClass* c);

Object* createConstCallableObject(callable* call);
Object* createConstStringObject(char* value);

Object* createRuntimeObj(objClass* c);
Object* createRuntimeStringObject(char* value);
Object* createRuntimeListObject();
Object* createRuntimeDictObject();
Object* createRuntimeSetObject();


#endif //CJ_2_OBJECTMANAGER_H
