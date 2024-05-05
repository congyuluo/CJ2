//
// Created by congyu on 7/21/23.
//

#include "vm.h"
#include "errors.h"
#include "objectManager.h"
#include "compiler.h"

#include <math.h>
#include <string.h>

#ifdef DEBUG_PRINT_VM_STACK
#include "debug.h"
#endif

#define IS_METHOD(o) (VALUE_TYPE(o) == BUILTIN_CALLABLE) && (VALUE_CALLABLE_TYPE(o) == method)
#define IS_C_CALLABLE(c) c->func == NULL

#define GLOBAL_REF(index) globalRefArray[index]
#define LOCAL_REF(index) localRefArray[index]
#define CONST_REF(index) constants[index]

#define GET_NIBBLE(shift) ((uint8_t)((line >> ((shift) * 4)) & 0xF))
#define GET_BYTE(shift)  ((uint8_t) ((line >> ((shift) * 8)) & 0xFF))
#define GET_WORD(shift) ((uint16_t)((line >> ((shift) * 8)) & 0xFFFF))
#define GET_DWORD(shift) ((uint32_t)((line >> ((shift) * 8)) & 0xFFFFFFFF))

#define STACK_PUSH(obj) (*vm->stackTop++ = (obj))
#define STACK_POP() (*(--vm->stackTop))

VM* vm;
uint64_t** ipStack[VM_LOCAL_REF_TABLE_STACK_INIT_SIZE];
uint64_t*** ipStackTop;
bool isRuntime = false;

uint32_t cycleCount;

static inline Value* newLocalScope(uint16_t dataSectionSize, uint8_t shiftDown) {
    Value* dataPtr = vm->stackTop-shiftDown;
    // Clear data section
    for (int i=shiftDown; i<dataSectionSize; i++) *(dataPtr+i) = INTERNAL_NULL_VAL;
    // Increment stack top
    vm->stackTop += dataSectionSize-shiftDown;
    return dataPtr;
}

static inline void popLocalScope(uint16_t dataSectionSize) {
    // Decrement stack top
    vm->stackTop -= dataSectionSize;
}

Value execInput(Value callableObj, Value selfObj, Value* attrs, uint8_t inCount) {
    // Check object type
    if (VALUE_TYPE(callableObj) != BUILTIN_CALLABLE) runtimeError("Input Object is not callable");
    // Get callable
    callable* c = VALUE_CALLABLE_VALUE(callableObj);
    // Check callable in count
    if (c->in != -1 && c->in != inCount) runtimeError("Inplace input count does not match callable input count");
    // Determine is callable is method
    bool isMethod = IS_METHOD(callableObj);
    Value result = INTERNAL_NULL_VAL;
    if (IS_C_CALLABLE(c)) { // Built-in C function
        result = c->cFunc(selfObj, attrs, inCount);
        // Check output
        if (VALUE_CALLABLE_VALUE(callableObj)->out != 0 && IS_INTERNAL_NULL(result)) runtimeError("No return object for non-void callable");
        return result;
    } else { // Chunk function
        // Load onto stack
        if (isMethod) STACK_PUSH(selfObj);
        for (uint8_t i=0; i<inCount; i++) STACK_PUSH(attrs[i]);
        // Create local scope
        uint16_t dataSectionSize = c->func->localRefArraySize;
        Value* dataSecPtr = newLocalScope(dataSectionSize, isMethod ? inCount+1 : inCount);
        Value* beforeCallStackTop = vm->stackTop;
        // Execute
        execChunk(c->func, dataSecPtr);
        // Check output
        if (c->out != 0) {
            if (vm->stackTop != beforeCallStackTop+1) runtimeError("No return object for non-void callable");
            result = STACK_POP();
        }
        // Remove local scope
        popLocalScope(dataSectionSize);
        return result;
    }
}

void execInplace(Value callableObj, uint8_t inCount) {
    // Check object type
    if (VALUE_TYPE(callableObj) != BUILTIN_CALLABLE) runtimeError("Input Object is not callable");
    // Get callable
    callable* c = VALUE_CALLABLE_VALUE(callableObj);
    // Check callable in count
    if (c->in != -1 && c->in != inCount) runtimeError("Inplace input count does not match callable input count");
    // Determine is callable is method
    bool isMethod = IS_METHOD(callableObj);
    // Execute
    if (IS_C_CALLABLE(c)) { // C callable
        Value result;
        if (isMethod) {
            result = c->cFunc(*((vm->stackTop - inCount) - 1), vm->stackTop - inCount, inCount);
        } else {
            result = c->cFunc(INTERNAL_NULL_VAL, vm->stackTop - inCount, inCount);
        }
        if (c->out != 0 && IS_INTERNAL_NULL(result)) runtimeError("No return object for non-void callable");
        // Shift stack
        vm->stackTop -= isMethod ? inCount+2 : inCount+1;
        if (c->out != 0) STACK_PUSH(result);
    } else { // Chunk callable
        // Create local scope
        uint16_t dataSectionSize = c->func->localRefArraySize;
        Value* dataSecPtr = newLocalScope(dataSectionSize, isMethod ? inCount+1 : inCount);
        Value* beforeCallStackTop = vm->stackTop;
        // Execute
        execChunk(c->func, dataSecPtr);
        // Check output
        Value result;
        if (c->out != 0) {
            if (vm->stackTop != beforeCallStackTop+1) runtimeError("No return object for non-void callable");
            result = STACK_POP();
        }
        // Remove local ref table
        popLocalScope(dataSectionSize);

        if (c->out == 0) {
            // Remove callable object
            vm->stackTop -= 1;
        } else {
            // Replace with result
            *(vm->stackTop-1) = result;
        }
    }
}

static inline double payloadNumBinaryOp(double val1, double val2, OpCode op) {
    switch (op) {
        case OP_ADD: return val1 + val2;
        case OP_SUB: return val1 - val2;
        case OP_MUL: return val1 * val2;
        case OP_DIV: return val1 / val2;
        case OP_MOD: return fmod(val1, val2);
        case OP_POW: return pow(val1, val2);
        default:
            runtimeError("Invalid binary operation type");
    }
}

static inline Value payloadNumBinaryComp(double val1, double val2, OpCode op) {
    switch (op) {
        case OP_LESS: return (val1 < val2) ? BOOL_VAL(true) : BOOL_VAL(false);
        case OP_MORE: return (val1 > val2) ? BOOL_VAL(true) : BOOL_VAL(false);
        case OP_LESS_EQUAL: return (val1 <= val2) ? BOOL_VAL(true) : BOOL_VAL(false);
        case OP_MORE_EQUAL: return (val1 >= val2) ? BOOL_VAL(true) : BOOL_VAL(false);
        case OP_EQUAL: return (val1 == val2) ? BOOL_VAL(true) : BOOL_VAL(false);
        default:
            runtimeError("Invalid binary operation type");
    }
}

void execChunk(Chunk* chunk, Value* dataSection) {
    if (chunk == NULL) runtimeError("Chunk is NULL");
    uint64_t* ip = chunk->code;
    Value* constants = (Value*) chunk->constants->data;
    Value* globalRefArray = vm->globalRefArray;
    Value* localRefArray = dataSection;
    callable** functionArray = vm->functionArray;

    // Add ip to stack
    *ipStackTop++ = &ip;

    while (1) {
        // Fetch instruction
        uint64_t line = *ip++;
#ifdef DEBUG_PRINT_VM_STACK
        printf("- Executing operation : \n");
        printInstr(line, chunk);
        printf("\nCycle count: %u\n\n", cycleCount);
        cycleCount++;
#endif
        // Parse instruction
        OpCode op = (uint8_t)(line & 0xFF);
        switch (op) {
            case OP_CONSTANT: {
                STACK_PUSH(CONST_REF(GET_BYTE(1)));
                break;
            }
            case OP_GET_ATTR: {
                Value attrName = CONST_REF(GET_BYTE(1));
                if (VALUE_TYPE(attrName) != BUILTIN_STR) runtimeError("Attribute name is not a string");
                Value obj = STACK_POP();
                Value attrObj = getAttr(obj, VALUE_STR_VALUE(attrName));
                // Insert new object
                STACK_PUSH(attrObj);
                break;
            }
            case OP_GET_ATTR_CALL: {
                Value attrName = CONST_REF(GET_BYTE(1));
                if (VALUE_TYPE(attrName) != BUILTIN_STR) runtimeError("Attribute name is not a string");
                Value obj = STACK_POP();
                Value attrObj = getAttr(obj, VALUE_STR_VALUE(attrName));
                // Insert new object
                STACK_PUSH(attrObj);
                // Reinsert self
                STACK_PUSH(obj);
                break;
            }
            case OP_GET_GLOBAL_REF_ATTR: {
                // Push attribute object
                Value retrievedObj = GLOBAL_REF(GET_WORD(1));
                if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Global reference not found");
                STACK_PUSH(retrievedObj);
                break;
            }
            case OP_GET_LOCAL_REF_ATTR: {
                // Push attribute object
                Value retrievedObj = LOCAL_REF(GET_WORD(1));
                if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Global reference not found");
                STACK_PUSH(retrievedObj);
                break;
            }
            case OP_GET_COMBINED_REF_ATTR: {
                // Get local array index
                // Get local ref
                Value retrievedObj = LOCAL_REF(GET_WORD(1));
                if (IS_INTERNAL_NULL(retrievedObj)) { // Search in global ref array
                    retrievedObj = GLOBAL_REF(GET_WORD(3));
                }
                if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Local reference not found");
                // Push attribute object
                STACK_PUSH(retrievedObj);
                break;
            }
            case OP_SET_GLOBAL_REF_ATTR: {
                // Get special assignment
                specialAssignment sa = GET_BYTE(3);
                uint16_t globalIndex = GET_WORD(1);
                if (sa != ASSIGNMENT_NONE) {
                    Value retrievedObj = GLOBAL_REF(globalIndex);
                    if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Reference not found");
                    GLOBAL_REF(globalIndex) = performValueModification(sa, retrievedObj, STACK_POP());
                } else {
                    GLOBAL_REF(globalIndex) = STACK_POP();
                }
                break;
            }
            case OP_SET_LOCAL_REF_ATTR: {
                uint16_t localIndex = GET_WORD(1);
                // Get special assignment
                specialAssignment sa = GET_BYTE(3);
                Value val = STACK_POP();
                if (sa != ASSIGNMENT_NONE) {
                    // Try local
                    Value retrievedObj = LOCAL_REF(localIndex);
                    if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Reference not found");
                    // Check if we are modifying a number
                    if (VALUE_TYPE(val) == VAL_NUMBER && VALUE_TYPE(retrievedObj) == VAL_NUMBER) {
                        switch (sa) {
                            case ASSIGNMENT_ADD:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) += VALUE_NUMBER_VALUE(val); break;
                            case ASSIGNMENT_SUB:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) -= VALUE_NUMBER_VALUE(val); break;
                            case ASSIGNMENT_MUL:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) *= VALUE_NUMBER_VALUE(val); break;
                            case ASSIGNMENT_DIV:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) /= VALUE_NUMBER_VALUE(val); break;
                            case ASSIGNMENT_MOD:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) = fmod(VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)), VALUE_NUMBER_VALUE(val)); break;
                            case ASSIGNMENT_POWER:
                                VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) = pow(VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)), VALUE_NUMBER_VALUE(val)); break;
                            default: {
                                runtimeError("Unknown special assignment");
                            }
                        }
                    } else { // Modify object
                        LOCAL_REF(localIndex) = performValueModification(sa, retrievedObj, val);
                    }
                } else { // Assign
                    LOCAL_REF(localIndex) = val;
                }
                break;
            }
            case OP_SET_COMBINED_REF_ATTR: {
                uint16_t localIndex = GET_WORD(1);
                uint16_t globalIndex = GET_WORD(3);
                // Get special assignment
                specialAssignment sa = GET_BYTE(5);
                Value val = STACK_POP();
                // If we are modifying a reference
                if (sa != ASSIGNMENT_NONE) {
                    // Try local
                    Value retrievedObj = LOCAL_REF(localIndex);
                    if (!IS_INTERNAL_NULL(retrievedObj)) { // Modify local
                        // Check if we are modifying a number
                        if (VALUE_TYPE(val) == VAL_NUMBER && VALUE_TYPE(retrievedObj) == VAL_NUMBER) {
                            switch (sa) {
                                case ASSIGNMENT_ADD:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) += VALUE_NUMBER_VALUE(val); break;
                                case ASSIGNMENT_SUB:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) -= VALUE_NUMBER_VALUE(val); break;
                                case ASSIGNMENT_MUL:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) *= VALUE_NUMBER_VALUE(val); break;
                                case ASSIGNMENT_DIV:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) /= VALUE_NUMBER_VALUE(val); break;
                                case ASSIGNMENT_MOD:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) = fmod(VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)), VALUE_NUMBER_VALUE(val)); break;
                                case ASSIGNMENT_POWER:
                                    VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)) = pow(VALUE_NUMBER_VALUE(LOCAL_REF(localIndex)), VALUE_NUMBER_VALUE(val)); break;
                                default: {
                                    runtimeError("Unknown special assignment");
                                }
                            }
                        } else { // Modify object
                            LOCAL_REF(localIndex) = performValueModification(sa, retrievedObj, val);
                        }
                    } else { // Try global
                        retrievedObj = GLOBAL_REF(globalIndex);
                        if (IS_INTERNAL_NULL(retrievedObj)) runtimeError("Local and global reference not found");
                        GLOBAL_REF(globalIndex) = performValueModification(sa, retrievedObj, val);
                    }
                } else { // Assign
                    LOCAL_REF(localIndex) = val;
                }
                break;
            }
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
            case OP_POW: {
                captureType leftType = GET_NIBBLE(2);
                captureType rightType = GET_NIBBLE(3);
                uint8_t localAddrSlot = 2;
                Value rightObj = INTERNAL_NULL_VAL;
                double rightVal;
                bool rightIsNum = false;
                switch (rightType) {
                    case CAPTURE_NONE: {
                        rightObj = STACK_POP();
                        if (VALUE_TYPE(rightObj) == VAL_NUMBER) {
                            rightIsNum = true;
                            rightVal = VALUE_NUMBER_VALUE(rightObj);
                        }
                        break;
                    }
                    case CAPTURE_PAYLOAD: {
                        rightVal = GET_DWORD(4);
                        rightIsNum = true;
                        break;
                    }
                    case CAPTURE_VARIABLE: {
                        rightObj = LOCAL_REF(GET_WORD(localAddrSlot));
                        if (VALUE_TYPE(rightObj) == VAL_NUMBER) {
                            rightIsNum = true;
                            rightVal = VALUE_NUMBER_VALUE(rightObj);
                        }
                        localAddrSlot += 2;
                        break;
                    }
                    default:
                        runtimeError("Unknown right capture type");
                }
                Value leftObj = INTERNAL_NULL_VAL;
                double leftVal;
                bool leftIsNum = false;
                switch (leftType) {
                    case CAPTURE_NONE: {
                        leftObj = STACK_POP();
                        if (VALUE_TYPE(leftObj) == VAL_NUMBER) {
                            leftIsNum = true;
                            leftVal = VALUE_NUMBER_VALUE(leftObj);
                        }
                        break;
                    }
                    case CAPTURE_PAYLOAD: {
                        leftVal = GET_DWORD(4);
                        leftIsNum = true;
                        break;
                    }
                    case CAPTURE_VARIABLE: {
                        leftObj = LOCAL_REF(GET_WORD(localAddrSlot));
                        if (VALUE_TYPE(leftObj) == VAL_NUMBER) {
                            leftIsNum = true;
                            leftVal = VALUE_NUMBER_VALUE(leftObj);
                        }
                        break;
                    }
                    default:
                        runtimeError("Unknown left capture type");
                }
                if (rightIsNum && leftIsNum) {
                    STACK_PUSH(NUMBER_VAL(payloadNumBinaryOp(leftVal,rightVal, op)));
                } else {
                    if (IS_INTERNAL_NULL(rightObj)) rightObj = NUMBER_VAL(rightVal);
                    if (IS_INTERNAL_NULL(leftObj)) leftObj = NUMBER_VAL(leftVal);
                    STACK_PUSH(binaryOperation(leftObj, rightObj, op));
                }
                break;
            }
            case OP_LESS:
            case OP_MORE:
            case OP_LESS_EQUAL:
            case OP_MORE_EQUAL:
            case OP_EQUAL: {
                captureType leftType = GET_NIBBLE(2);
                captureType rightType = GET_NIBBLE(3);
                uint8_t localAddrSlot = 2;
                Value rightObj = INTERNAL_NULL_VAL;
                double rightVal;
                bool rightIsNum = false;
                switch (rightType) {
                    case CAPTURE_NONE: {
                        rightObj = STACK_POP();
                        if (VALUE_TYPE(rightObj) == VAL_NUMBER) {
                            rightIsNum = true;
                            rightVal = VALUE_NUMBER_VALUE(rightObj);
                        }
                        break;
                    }
                    case CAPTURE_PAYLOAD: {
                        rightVal = GET_DWORD(4);
                        rightIsNum = true;
                        break;
                    }
                    case CAPTURE_VARIABLE: {
                        rightObj = LOCAL_REF(GET_WORD(localAddrSlot));
                        if (VALUE_TYPE(rightObj) == VAL_NUMBER) {
                            rightIsNum = true;
                            rightVal = VALUE_NUMBER_VALUE(rightObj);
                        }
                        localAddrSlot += 2;
                        break;
                    }
                    default:
                        runtimeError("Unknown right capture type");
                }
                Value leftObj = INTERNAL_NULL_VAL;
                double leftVal;
                bool leftIsNum = false;
                switch (leftType) {
                    case CAPTURE_NONE: {
                        leftObj = STACK_POP();
                        if (VALUE_TYPE(leftObj) == VAL_NUMBER) {
                            leftIsNum = true;
                            leftVal = VALUE_NUMBER_VALUE(leftObj);
                        }
                        break;
                    }
                    case CAPTURE_PAYLOAD: {
                        leftVal = GET_DWORD(4);
                        leftIsNum = true;
                        break;
                    }
                    case CAPTURE_VARIABLE: {
                        leftObj = LOCAL_REF(GET_WORD(localAddrSlot));
                        if (VALUE_TYPE(leftObj) == VAL_NUMBER) {
                            leftIsNum = true;
                            leftVal = VALUE_NUMBER_VALUE(leftObj);
                        }
                        break;
                    }
                    default:
                        runtimeError("Unknown left capture type");
                }
                if (rightIsNum && leftIsNum) {
                    STACK_PUSH(payloadNumBinaryComp(leftVal,rightVal, op));
                } else {
                    if (IS_INTERNAL_NULL(rightObj)) rightObj = NUMBER_VAL(rightVal);
                    if (IS_INTERNAL_NULL(leftObj)) leftObj = NUMBER_VAL(leftVal);
                    STACK_PUSH(binaryOperation(leftObj, rightObj, op));
                }
                break;
            }
            case OP_NEGATE:
                STACK_PUSH(unaryOperation(STACK_POP(), "_ng"));
                break;
            case OP_NOT: {
                Value obj = STACK_POP();
                if (VALUE_TYPE(obj) != VAL_BOOL) runtimeError("Object is not a boolean");
                STACK_PUSH(!VALUE_BOOL_VALUE(obj) ? BOOL_VAL(true) : BOOL_VAL(false));
                break;
            }
            case OP_AND: {
                Value obj2 = STACK_POP();
                Value obj1 = STACK_POP();
                if (VALUE_TYPE(obj1) != VAL_BOOL || VALUE_TYPE(obj2) != VAL_BOOL) runtimeError("Object is not a boolean");
                STACK_PUSH(VALUE_BOOL_VALUE(obj1) && VALUE_BOOL_VALUE(obj2) ? BOOL_VAL(true) : BOOL_VAL(false));
                break;
            }
            case OP_OR: {
                Value obj2 = STACK_POP();
                Value obj1 = STACK_POP();
                if (VALUE_TYPE(obj1) != VAL_BOOL || VALUE_TYPE(obj2) != VAL_BOOL) runtimeError("Object is not a boolean");
                STACK_PUSH(VALUE_BOOL_VALUE(obj1) || VALUE_BOOL_VALUE(obj2) ? BOOL_VAL(true) : BOOL_VAL(false));
                break;
            }
            case OP_JUMP: {
                int16_t jumpInc = GET_WORD(1);
                ip--;
                ip += jumpInc;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                Value condition = STACK_POP();
                if (VALUE_TYPE(condition) != VAL_BOOL) runtimeError("Condition is not a boolean");
                if (!VALUE_BOOL_VALUE(condition)) {
                    int16_t jumpInc = GET_WORD(1);
                    ip--;
                    ip += jumpInc;
                }
                break;
            }
            case OP_RETURN:
                // Pop ip from stack
                ipStackTop--;
                return ;
            case OP_RETURN_NONE: {
                STACK_PUSH(NONE_VAL);
                // Pop ip from stack
                ipStackTop--;
                return ;
            }
            case OP_IS: {
                Value obj2 = STACK_POP();
                Value obj1 = STACK_POP();
                STACK_PUSH(areValuesEqual(obj1, obj2) ? BOOL_VAL(true) : BOOL_VAL(false));
                break;
            }
            case OP_GET_SELF:
                STACK_PUSH(LOCAL_REF(0));
                break;
            case OP_GET_INDEX_REF: {
                // Get objects
                Value indexObj = STACK_POP();
                Value targetObj = STACK_POP();
                STACK_PUSH(objGetIndexRef(targetObj, indexObj));
                break;
            }
            case OP_SET_INDEX_REF: {
                // Get special assignment
                specialAssignment sa = GET_BYTE(1);
                // Get Value
                Value value = STACK_POP();
                // Get index and target objects
                Value index = STACK_POP();
                Value target = STACK_POP();
                if (sa != ASSIGNMENT_NONE) {
                    indexSpecialAssignment(sa, target, index, value);
                } else {
                    // Check index is num
                    if (VALUE_TYPE(index) != VAL_NUMBER) runtimeError("Index is not a num");
                    // Get index set method
                    Value indexSetMethod = getAttr(target, "set");
                    // Prepare input array
                    Value inputs[2] = {index, value};
                    // Execute index set method
                    execInput(indexSetMethod, target, inputs, 2);
                }
                break;
            }
            case OP_SET_ATTR: {
                // Get attribute name
                Value attrName = CONST_REF(GET_BYTE(1));
                // Check if attribute name is a string
                if (VALUE_TYPE(attrName) != BUILTIN_STR) runtimeError("Attribute name is not a string");
                // Get special assignment
                specialAssignment sa = GET_BYTE(2);
                // Get Value and target objects
                Value value = STACK_POP();
                Value target = STACK_POP();
                if (IS_SYSTEM_DEFINED_TYPE(target.type)) runtimeError("Unable to set attribute on system defined type");
                if (sa != ASSIGNMENT_NONE) {
                    attrSpecialAssignment(sa, target, VALUE_STR_VALUE(attrName), value);
                } else {
                    strValInsert(target.obj->primValue.afterDefAttributes, VALUE_STR_VALUE(attrName), value);
                }
                break;
            }
            case OP_EXEC_FUNCTION_ENFORCE_RETURN:
            case OP_EXEC_FUNCTION_IGNORE_RETURN: {
                // Get call parameters
                uint8_t attrCount = GET_BYTE(1);
                callable* targetCallable = functionArray[GET_WORD(2)];
                // Check callable output count
                if (op == OP_EXEC_FUNCTION_ENFORCE_RETURN && targetCallable->out == 0) runtimeError("Callable has no output");
                if (IS_C_CALLABLE(targetCallable)) {
                    Value result = targetCallable->cFunc(INTERNAL_NULL_VAL, vm->stackTop - attrCount, attrCount);
                    if (targetCallable->out != 0 && IS_INTERNAL_NULL(result)) runtimeError("No return object for non-void callable");
                    vm->stackTop -= attrCount;
                    if (targetCallable->out != 0) STACK_PUSH(result);
                } else {
                    uint16_t dataSectionSize = targetCallable->func->localRefArraySize;
                    Value* dataSecPtr = newLocalScope(dataSectionSize, attrCount);
                    execChunk(targetCallable->func,dataSecPtr);
                    if (op == OP_EXEC_FUNCTION_ENFORCE_RETURN) {
                        *dataSecPtr = STACK_POP();
                        vm->stackTop -= (dataSectionSize - 1);
                    } else {
                        vm->stackTop -= dataSectionSize;
                    }
                    vm->localScopeCount--;
                }
                break;
            }
            case OP_EXEC_METHOD_ENFORCE_RETURN:
            case OP_EXEC_METHOD_IGNORE_RETURN: {
                // Get input count
                uint8_t inputCount = GET_BYTE(1);
                // Get callable object
                Value callableObj = *(vm->stackTop-(inputCount+1));
                // If callable position is self, get callable from previous position
                if (VALUE_TYPE(callableObj) != BUILTIN_CALLABLE) callableObj = *(vm->stackTop-(inputCount+2));
                // Check if callable
                if (VALUE_TYPE(callableObj) != BUILTIN_CALLABLE) runtimeError("Object is not callable");
                // Check if callable has output for enforce return
                if (VALUE_CALLABLE_VALUE(callableObj)->out == 0 && op == OP_EXEC_METHOD_ENFORCE_RETURN) runtimeError("Callable has no output");
                // Execute callable
                execInplace(callableObj, inputCount);
                // Ignore return object if necessary
                if (VALUE_CALLABLE_VALUE(callableObj)->out == 1 && op == OP_EXEC_METHOD_IGNORE_RETURN) vm->stackTop--;
                break;
            }
            case OP_INIT: {
                // Read class name, find class
                uint16_t classID = GET_WORD(1);
                objClass* objectClass = classArray[classID];
                initFuncType classInitType = objectClass->initType;
                if (classInitType == NONE_INIT_TYPE) runtimeError("Inappropriate class initialization type for 'new' operation");
                // Create object and push to stack
                Value newObj = OBJECT_VAL(createRuntimeObj(objectClass), objectClass->classID);
                // Push object for current frame reference
                STACK_PUSH(newObj);
                // Push init method
                STACK_PUSH(objectClass->initFunc);
                // Push self for next frame reference
                STACK_PUSH(newObj);
                break;
            }
            case OP_GET_PARENT_INIT: {
                Value selfObj = LOCAL_REF(0);
                // Get parent init method
                objClass* currClass = VALUE_CLASS(selfObj);
                if (currClass->parentClass == NULL) runtimeError("Called pInit on object of class with no parent");
                initFuncType pClassInitType = currClass->parentClass->initType;
                if (pClassInitType == NONE_INIT_TYPE) runtimeError("Inappropriate parent class initialization type");
                // Push parent init method
                STACK_PUSH(currClass->parentClass->initFunc);
                // Push self for next frame reference
                STACK_PUSH(selfObj);
                break;
            }
            default:
                runtimeError("Unknown opcode.");
        }
#ifdef DEBUG_PRINT_VM_STACK
        printStack();
#endif
    }
}

bool compareValue(Value v1, Value v2) {
    Value result = binaryOperation(v1, v2, OP_EQUAL);
    if (VALUE_TYPE(result) != VAL_BOOL) runtimeError("Result of _eq is not a boolean");
    return VALUE_BOOL_VALUE(result);
}

void initVM(Value* globalRefArray, callable** functionArray, uint16_t globalRefCount) {
    vm = (VM*)malloc(sizeof(VM));
    vm->stackTop = vm->stack;
    vm->globalRefArray = globalRefArray;
    vm->functionArray = functionArray;
    vm->globalRefCount = globalRefCount;

    ipStackTop = ipStack;
    isRuntime = true;

#ifdef DEBUG_PRINT_VM_STACK
    cycleCount = 0;
#endif
}

Value unaryOperation(Value obj1, char* op) {
    Value opFunction = ignoreNullGetAttr(obj1, op);
    if (IS_INTERNAL_NULL(opFunction)) runtimeError("No operator function found");
    return execInput(opFunction, obj1, NULL, 0);
}

Value binaryOperation(Value v1, Value v2, OpCode op) {
    char* leftOpStr = NULL;
    char* rightOpStr = NULL;
    switch (op) {
        case OP_ADD: {
            leftOpStr = "_add";
            rightOpStr = "_add";
            break;
        }
        case OP_SUB: {
            leftOpStr = "_sub";
            break;
        }
        case OP_MUL: {
            leftOpStr = "_mul";
            rightOpStr = "_mul";
            break;
        }
        case OP_DIV: {
            leftOpStr = "_div";
            break;
        }
        case OP_MOD: {
            leftOpStr = "_mod";
            break;
        }
        case OP_POW: {
            leftOpStr = "_pow";
            break;
        }
        case OP_EQUAL: {
            leftOpStr = "_eq";
            rightOpStr = "_eq";
            break;
        }
        case OP_LESS: {
            leftOpStr = "_less";
            rightOpStr = "_meq";
            break;
        }
        case OP_MORE: {
            leftOpStr = "_more";
            rightOpStr = "_leq";
            break;
        }
        case OP_LESS_EQUAL: {
            leftOpStr = "_leq";
            rightOpStr = "_more";
            break;
        }
        case OP_MORE_EQUAL: {
            leftOpStr = "_meq";
            rightOpStr = "_less";
            break;
        }
        default:
            runtimeError("Invalid binary operation type");
    }
    Value opFunction = ignoreNullGetAttr(v1, leftOpStr);
    if (!IS_INTERNAL_NULL(opFunction)) {
        return execInput(opFunction, v1, &v2, 1);
    }
    if (rightOpStr != NULL) {
        opFunction = ignoreNullGetAttr(v2, rightOpStr);
        if (!IS_INTERNAL_NULL(opFunction)) {
            return execInput(opFunction, v2, &v1, 1);
        }
    }
    runtimeError("No operator function found");
    return INTERNAL_NULL_VAL;
}

Value performValueModification(specialAssignment sa, Value value, Value modValue) {
    if (VALUE_TYPE(value) == VAL_NUMBER && VALUE_TYPE(modValue) == VAL_NUMBER) {
        switch (sa) {
            case ASSIGNMENT_ADD:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value),VALUE_NUMBER_VALUE(modValue), OP_ADD));
            case ASSIGNMENT_SUB:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value),VALUE_NUMBER_VALUE(modValue), OP_SUB));
            case ASSIGNMENT_MUL:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value),VALUE_NUMBER_VALUE(modValue), OP_MUL));
            case ASSIGNMENT_DIV:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value),VALUE_NUMBER_VALUE(modValue), OP_DIV));
            case ASSIGNMENT_MOD:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value),VALUE_NUMBER_VALUE(modValue), OP_MOD));
            case ASSIGNMENT_POWER:
                return NUMBER_VAL(payloadNumBinaryOp(VALUE_NUMBER_VALUE(value), VALUE_NUMBER_VALUE(modValue), OP_POW));
            default: {
                runtimeError("Unknown special assignment");
                return INTERNAL_NULL_VAL;
            }
        }
    } else {
        switch (sa) {
            case ASSIGNMENT_ADD:
                return binaryOperation(value, modValue, OP_ADD);
            case ASSIGNMENT_SUB:
                return binaryOperation(value, modValue, OP_SUB);
            case ASSIGNMENT_MUL:
                return binaryOperation(value, modValue, OP_MUL);
            case ASSIGNMENT_DIV:
                return binaryOperation(value, modValue, OP_DIV);
            case ASSIGNMENT_MOD:
                return binaryOperation(value, modValue, OP_MOD);
            case ASSIGNMENT_POWER:
                return binaryOperation(value, modValue, OP_POW);
            default: {
                runtimeError("Unknown special assignment");
                return INTERNAL_NULL_VAL;
            }
        }
    }
}

void indexSpecialAssignment(specialAssignment sa , Value target, Value index, Value value) {
    Value retrievedObj = objGetIndexRef(target, index);
    Value modifiedValue = performValueModification(sa, retrievedObj, value); 
    // Get index set method
    Value indexSetMethod = getAttr(target, "set");
    // Prepare input array
    Value inputs[2] = {index, modifiedValue};
    // Execute index set method
    execInput(indexSetMethod, target, inputs, 2);
}

void attrSpecialAssignment(specialAssignment sa, Value target, char* attrName, Value value) {
    // Get attribute original Value
    Value originalAttribute = getAttr(target, attrName); // getAttr is protected from NULL target
    // Modify Value and re-insert as attribute
    Value modifiedValue = performValueModification(sa, originalAttribute, value);
    strValInsert(target.obj->primValue.afterDefAttributes, attrName, modifiedValue);
}

Value objGetIndexRef(Value target, Value index) {
    // Get index object
    if (VALUE_TYPE(index) != VAL_NUMBER) runtimeError("Index object is not num");
    // Get index reference method
    Value indexRefMethod = getAttr(target, "get");
    if (VALUE_CALLABLE_VALUE(indexRefMethod)->out == 0) runtimeError("Index reference method has no output");
    return execInput(indexRefMethod, target, &index, 1);
}

void freeVM() {
    if (vm == NULL) return;
    free(vm->globalRefArray);
    free(vm->functionArray);
    free(vm);
    vm = NULL;
}

void runVM(Value mainFunc, Value attrs, int inCount) {
    // Push main function attributes
    if (inCount != 0) STACK_PUSH(attrs);
    execInplace(mainFunc, inCount);
    isRuntime = false;
}

void defaultPrint(Value obj) {
    if (IS_INTERNAL_NULL(obj)) {
        printf("NULL");
    } else if (VALUE_TYPE(obj) == BUILTIN_CALLABLE) {
        printf("%s object", (VALUE_CALLABLE_TYPE(obj) == method) ? "Method" : "Function");
    } else if (VALUE_TYPE(obj) == VAL_BOOL || VALUE_TYPE(obj) == BUILTIN_STR || VALUE_TYPE(obj) == VAL_NUMBER) {
        printPrimitiveValue(obj);
    } else {
        printf("%s object", VALUE_CLASS(obj)->className);
    }
}

void printGlobalRefArray() {
    printf("[");
    for (uint16_t i = 0; i < vm->globalRefCount; i++) {
        // Find if print func is defined for the object
        Value obj = vm->globalRefArray[i];
        defaultPrint(obj);
        if (i != vm->globalRefCount -1) printf(", ");
    }
    printf("]");
}

void printStack() {
    printf("\nRuntime Stack:\n");
    // Get stack start index for first local scope
    Value* currObj = vm->stack;
    uint16_t count = 0;
    while (currObj != vm->stackTop) {
        printf("[%u]: ", count++);
        defaultPrint(*currObj);
        printf("\n");
        currObj++;
    }
    printf("Global Ref Array: ");
    printGlobalRefArray();
    printf("\n");
}
