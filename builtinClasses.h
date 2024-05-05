//
// Created by congyu on 7/20/23.
//

#ifndef CJ_2_BUILTINCLASSES_H
#define CJ_2_BUILTINCLASSES_H

#include "primitiveVars.h"
#include "refManager.h"

void addGlobalReference(refTable* globalRefTable, runtimeList* globalRefList, Value obj, char* name);
void constructBuiltinClasses(refTable* globalRefTable, runtimeList* globalRuntimeList, refTable* globalClassTable);
void loadUserFunctions(refTable* globalRefTable, runtimeList* globalRefList, userFunction* userFunctions, uint32_t funcCount);

#endif //CJ_2_BUILTINCLASSES_H
