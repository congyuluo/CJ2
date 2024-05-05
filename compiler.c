//
// Created by congyu on 7/29/23.
//

#include <math.h>
#include <string.h>

#include "compiler.h"
#include "errors.h"
#include "vm.h"
#include "debug.h"
#include "runtimeDS.h"
#include "builtinClasses.h"
#include "objectManager.h"
#include "objClass.h"


// Increment current token
#define INC_TOKEN() (currentToken = nextToken())
#define WRITEOP_CURRENT_CHUNK(op, line, index, sourceIndex) writeOp(currentChunk, op, line, index, sourceIndex)
#define CURR_CHUNK_INDEX currentChunk->count

#define GET_BYTE(data, shift)  ((uint8_t) (((data) >> ((shift) * 8)) & 0xFF))
#define GET_WORD(data, shift) ((uint16_t)(((data) >> ((shift) * 8)) & 0xFFFF))
#define SET_BOTTOM_8_BITS(data, byteVal)  (((data) & 0xFFFFFFFFFFFFFF00) | (uint64_t)(byteVal))


refTable* globalRefTable = NULL;
runtimeList* globalRefList = NULL;
refTable* globalClassRefTable = NULL;
refTable* prelinkedFuncTable = NULL;
refTable* globalDeclTable = NULL;

Chunk* currentChunk = NULL;
refTable* currentLocalRefTable = NULL;
uint32_t* GAsize = NULL;

token* currentToken;

runtimeList* chunkArray = NULL;

// Flag for emitted function methodCall in current statement
bool emittedCall;

// Flag for current callable return type
bool isVoidReturnChunk;

// Flag for method chunk
bool isMethodChunk;
// Flag for method init
bool isInitMethodChunk;

// Temporary placeholder jump list
uint16_t* continueJumpList;
uint16_t* breakJumpList;
uint8_t continueJumpIndex;
uint8_t breakJumpIndex;

// Chunk set local ref arrays
uint16_t chunkSetIndexArray[512];
uint16_t chunkSetIndexArrayIndex;

// Optimization for left hand size binary number operation
captureType capturedOperand;
int32_t capturedValue;

// Function linking
preLinkedCallNode* preLinkedCallHead = NULL;

// Compiler Constant Hash
runtimeDict* compilerConstantHash;

// Increment and check for NULL (end of file)
void incCheckNull() {
    token* prevToken = currentToken;
    INC_TOKEN();
    if (currentToken == NULL) compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Unexpected end of file");
}

// Check current token type
void checkType(tokenType type, char* msg) {
    if (TOKEN_TYPE(currentToken) != type) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, msg);
}

// Increment and check current token type
void incCheckType(tokenType type, char* msg) {
    incCheckNull();
    checkType(type, msg);
}

// Gets the previous token, checks for null previous token
token* getPrevToken() {
    if (currentToken->prevToken == NULL) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Null previous token");
    return currentToken->prevToken;
}

void setCurrentChunk(Chunk* chunk) {
    if (currentChunk != NULL || currentLocalRefTable != NULL) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Unclosed chunk or refTable");
    currentChunk = chunk;
    currentLocalRefTable = createRefTable(LOCAL_REF_TABLE_INIT_SIZE);
    chunkSetIndexArrayIndex = 0;
}

void clearCurrentChunk() {
#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    // Print local ref table
    printRefTable(currentLocalRefTable);
    printf("\n");
    // Print chunkSetIndexArray
    printf("Chunk Set Index Array [%u]: ", chunkSetIndexArrayIndex);
    for (int i=0; i<chunkSetIndexArrayIndex; i++) printf("%d ", chunkSetIndexArray[i]);
    printf("\n");
#endif
    uint32_t localRefSize = currentLocalRefTable->numEntries;
#ifdef DEBUG_PRINT_LOCAL_REF_TABLE
    printf("Local ref table: ");
    printRefTable(currentLocalRefTable);
    printf("\n");
#endif
    // Free local ref table
    freeRefTable(currentLocalRefTable);
    currentLocalRefTable = NULL;
    // Compiler optimization for invalid local get operations
    int* localIndexArray = malloc(sizeof(int) * localRefSize);
    int frontShift = 0;
    for (int i=0; i<localRefSize; i++) {
        localIndexArray[i] = -1;
        for (int j=0; j<chunkSetIndexArrayIndex; j++) {
            if (chunkSetIndexArray[j] == i) {
                localIndexArray[i] = i - frontShift;
                break;
            }
        }
        if (localIndexArray[i] == -1) frontShift++;
    }
#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    printf("localIndexArray [%d]: ", localRefSize);
    for (int i=0; i<localRefSize; i++) printf("%d ", localIndexArray[i]);
    printf("\n");
#endif
    // Iterate chunk and check for invalid local get operations
    for (int i=0; i<currentChunk->count; i++) {
        OpCode op = (uint8_t)(currentChunk->code[i] & 0xFF);
        if (op == OP_GET_COMBINED_REF_ATTR) {
            uint16_t localIndex = (uint16_t)((currentChunk->code[i] >> (1 * 8)) & 0xFFFF);
            if (localIndexArray[localIndex] == -1) {
                // Change opcode to OP_GET_GLOBAL_REF_ATTR
                currentChunk->code[i] = SET_BOTTOM_8_BITS(currentChunk->code[i], OP_GET_GLOBAL_REF_ATTR);
                // Get global ref index
                uint16_t globalIndex = (uint16_t)((currentChunk->code[i] >> (3 * 8)) & 0xFFFF);
                // Clear the next 16 bits after the 8-bit opcode
                currentChunk->code[i] &= ~(0xFFFFULL << 8);
                // Set the next 16 bits to the new mapped index
                currentChunk->code[i] |= ((uint64_t)globalIndex << 8);
            } else {
                // Clear the next 16 bits after the 8-bit opcode
                currentChunk->code[i] &= ~(0xFFFFULL << 8);
                // Set the next 16 bits to the new mapped index
                currentChunk->code[i] |= ((uint64_t)localIndexArray[localIndex] << 8);
            }
        } else if (op == OP_SET_COMBINED_REF_ATTR) {
            uint16_t localIndex = (uint16_t)((currentChunk->code[i] >> (1 * 8)) & 0xFFFF);
            // Clear the next 16 bits after the 8-bit opcode
            currentChunk->code[i] &= ~(0xFFFFULL << 8);
            // Set the next 16 bits to the new mapped index
            currentChunk->code[i] |= ((uint64_t)localIndexArray[localIndex] << 8);
        }
    }
    // Free
    free(localIndexArray);
    // Set chunk local ref array size
    currentChunk->localRefArraySize = localRefSize - frontShift;
    // Calculate jump lengths
    for (int i=0; i<currentChunk->count; i++) {
        uint64_t* currentCode = &currentChunk->code[i];
        OpCode op = (uint8_t)(currentChunk->code[i] & 0xFF);
        if (op == OP_JUMP || op == OP_JUMP_IF_FALSE) {
            uint16_t jumpAddr = GET_WORD(*currentCode, 1);
            int16_t jumpAddrDiff = (int16_t) (jumpAddr - i);
            // Clear the next 16 bits after the 8-bit opcode
            *currentCode &= ~(0xFFFFULL << 8);
            // Set the next 16 bits to the new mapped index
            *currentCode |= ((int64_t)jumpAddrDiff << 8);
        }
    }
#ifdef DEBUG_PRINT_CHUNK_AFTER_CREATION
    printf("\nAfter Compiler Optimization: \n");
    printChunk(currentChunk);
    printf("Local Array size: %d\n", currentChunk->localRefArraySize);
#endif
    currentChunk = NULL;
}

Value createStringConst(char* str) {
    // Create lookup string key
    // Check if string is already in constant hashString
    Value resultVal = dictStrGet(compilerConstantHash, str);
    if (!IS_INTERNAL_NULL(resultVal)) return resultVal;
    // Create builtin string object
    Value builtinVal = OBJECT_VAL(createConstStringObject(str), BUILTIN_STR);
    // Add to constant hashString
    dictInsertElement(compilerConstantHash, builtinVal, INTERNAL_NULL_VAL);
    return builtinVal;
}

uint16_t getGlobalRefIndex(char* identifier) {
    if (globalRefTable == NULL || globalRefList == NULL) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Null global refTable or refList");
    if (refTableContains(globalRefTable, identifier)) return getRefIndex(globalRefTable, identifier);
    // Add to refTable
    addGlobalReference(globalRefTable, globalRefList, INTERNAL_NULL_VAL, identifier);
    return getRefIndex(globalRefTable, identifier);
}

void patchContinueJumps(uint16_t jumpAddr) {
    for (int i=0; i<continueJumpIndex; i++) patchJump(currentChunk, continueJumpList[i], jumpAddr);
}

void patchBreakJumpsAtCurrent() {
    for (int i=0; i<breakJumpIndex; i++) patchJumpAtCurrent(currentChunk, breakJumpList[i]);
}

ParseRule rules[] = {
        // Punctuation
        [LEFT_PARENTHESES]    = {grouping, methodCall, PREC_CALL},
        [RIGHT_PARENTHESES]   = {NULL,     NULL,   PREC_NONE},
        [LEFT_BRACKET]        = {list, getIndexRef, PREC_CALL},
        [RIGHT_BRACKET]       = {NULL,     NULL,   PREC_NONE},
        [LEFT_BRACE]          = {NULL, NULL, PREC_NONE},
        [RIGHT_BRACE]         = {NULL,     NULL,   PREC_NONE},
        [COMMA]               = {NULL,     NULL,   PREC_NONE},
        [DOT]                 = {NULL,     dot,    PREC_CALL},
        [SEMICOLON]           = {NULL,     NULL,   PREC_NONE},

        // Arithmetic operators
        [PLUS]                = {NULL,     binary, PREC_TERM},
        [PLUS_EQUAL]          = {NULL,     NULL,   PREC_NONE},
        [MINUS]               = {unary,     binary, PREC_TERM},
        [MINUS_EQUAL]         = {NULL,     NULL,   PREC_NONE},
        [MULTIPLY]            = {NULL,     binary, PREC_FACTOR},
        [MULTIPLY_EQUAL]      = {NULL,     NULL,   PREC_NONE},
        [DIVIDE]              = {NULL,     binary, PREC_FACTOR},
        [DIVIDE_EQUAL]        = {NULL,     NULL,   PREC_NONE},
        [MOD]                 = {NULL,     binary, PREC_FACTOR},
        [MOD_EQUAL]           = {NULL,     NULL,   PREC_NONE},
        [EQUAL]               = {NULL,     NULL,   PREC_NONE},
        [DOUBLE_EQUAL]        = {NULL,     binary, PREC_EQUALITY},

        // Logical operators
        [DOUBLE_AND]          = {NULL,     binary,   PREC_AND},
        [DOUBLE_OR]           = {NULL,     binary,    PREC_OR},
        [NOT]                 = {unary,    NULL,   PREC_NONE},

        // Other operators
        [COLON]               = {NULL,     NULL,   PREC_NONE},
        [CARET]               = {NULL,     binary, PREC_EXPONENT},
        [CARET_EQUAL]         = {NULL,     NULL,   PREC_NONE},
        [DICT_PREFIX]         = {dictionary,NULL,   PREC_CALL},
        [SET_PREFIX]          = {set,     NULL,   PREC_CALL},

        // Comparison operators
        [LESS]                = {NULL,     binary, PREC_COMPARISON},
        [LESS_EQUAL]          = {NULL,     binary, PREC_COMPARISON},
        [MORE]                = {NULL,     binary, PREC_COMPARISON},
        [MORE_EQUAL]          = {NULL,     binary, PREC_COMPARISON},

        // Data types
        [STRING]              = {string,   NULL,   PREC_NONE},
        [NUMBER]              = {number,   NULL,   PREC_NONE},

        // Keywords
        [KEYWORD_IF]          = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_ELIF]        = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_ELSE]        = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_WHILE]       = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_FOR]         = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_BREAK]       = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_CONTINUE]    = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_RETURN]      = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_VOID]        = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_TRUE]        = {literal,  NULL,   PREC_NONE},
        [KEYWORD_FALSE]       = {literal,  NULL,   PREC_NONE},
        [KEYWORD_IS]          = {NULL,     binary, PREC_COMPARISON},
        [KEYWORD_NONE]        = {literal,  NULL,   PREC_NONE},
        [KEYWORD_CLASS]       = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_FUNCTION]    = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_SELF]        = {getVar, NULL,   PREC_NONE},
        [KEYWORD_NEW]         = {newObject, NULL,   PREC_NONE},
        [KEYWORD_PARENT_INIT] = {parentInit,     NULL,   PREC_CALL},
        [KEYWORD_INIT]        = {NULL,     NULL,   PREC_NONE},
        [KEYWORD_GLOBAL]      = {unary,    NULL,   PREC_UNARY},

        // Identifier
        [IDENTIFIER]          = {getVar, NULL,   PREC_NONE},
};

ParseRule* getRule(tokenType type) {
    return &rules[type];
}

uint8_t parseCommaSequence(tokenType endToken) {
    uint8_t argCount = 0;
    while (TOKEN_TYPE(currentToken) != endToken) { // Init list with items
        expression(true);
        argCount++;
        // Check for comma
        if (TOKEN_TYPE(currentToken) == COMMA) {
            incCheckNull();
            if (TOKEN_TYPE(currentToken) == endToken) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected expression after ','");
        } else{
            if (TOKEN_TYPE(currentToken) != endToken) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ',' between arguments");
        }
    }
    return argCount;
}

void grouping(bool enforceReturn) {
    expression(enforceReturn);
    if (TOKEN_TYPE(currentToken) != RIGHT_PARENTHESES) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ')'");
    incCheckNull();
}

void methodCall(bool enforceReturn) {
    token* callToken = getPrevToken();
    uint8_t argCount = parseCommaSequence(RIGHT_PARENTHESES);
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(enforceReturn ? OP_EXEC_METHOD_ENFORCE_RETURN : OP_EXEC_METHOD_IGNORE_RETURN, callToken->line, callToken->index, callToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
    emittedCall = true;
}

void newObject(bool enforceReturn) {
    token* newToken = getPrevToken();
    checkType(IDENTIFIER, "Expected class name");
    WRITEOP_CURRENT_CHUNK(OP_INIT, newToken->line, newToken->index, newToken->sourceIndex);
    writeChunk16(currentChunk, getRefIndex(globalClassRefTable, TOKEN_VALUE(currentToken)));
    // Load in arguments
    incCheckType(LEFT_PARENTHESES, "Expected '(' after class name");
    incCheckNull();
    token* callToken = getPrevToken();
    uint8_t argCount = parseCommaSequence(RIGHT_PARENTHESES);
    incCheckNull();

    WRITEOP_CURRENT_CHUNK(OP_EXEC_METHOD_IGNORE_RETURN, callToken->line, callToken->index, callToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
}

void parentInit(bool enforceReturn) {
    if (!isInitMethodChunk) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Parent init can only be called in init method");
    WRITEOP_CURRENT_CHUNK(OP_GET_PARENT_INIT, currentToken->line, currentToken->index, currentToken->sourceIndex);
    incCheckNull();
    methodCall(enforceReturn);
}

void list(bool enforceReturn) {
    // Create list object
    token* listToken = getPrevToken();
    WRITEOP_CURRENT_CHUNK(OP_INIT, listToken->line, listToken->index, listToken->sourceIndex);
    writeChunk16(currentChunk, getRefIndex(globalClassRefTable, "list"));
    // Collect arguments
    uint8_t argCount = parseCommaSequence(RIGHT_BRACKET);
    // Skip right bracket
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(OP_EXEC_METHOD_IGNORE_RETURN, listToken->line, listToken->index, listToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
}

void set(bool enforceReturn) {
    // Create list object
    token* setToken = getPrevToken();
    WRITEOP_CURRENT_CHUNK(OP_INIT, setToken->line, setToken->index, setToken->sourceIndex);
    writeChunk16(currentChunk, getRefIndex(globalClassRefTable, "set"));
    // Collect arguments
    uint8_t argCount = parseCommaSequence(RIGHT_BRACE);
    // Skip right bracket
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(OP_EXEC_METHOD_IGNORE_RETURN, setToken->line, setToken->index, setToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
}

void string(bool enforceReturn) {
    token* stringToken = getPrevToken();
    WRITEOP_CURRENT_CHUNK(OP_CONSTANT, stringToken->line, stringToken->index, stringToken->sourceIndex);
    writeValConstant(currentChunk, createStringConst(TOKEN_VALUE(stringToken)));
}

void dot(bool enforceReturn) {
    token* dotToken = getPrevToken();
    if (TOKEN_TYPE(currentToken) != IDENTIFIER) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected identifier after '.'");
    Value attrName = createStringConst(TOKEN_VALUE(currentToken));
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(TOKEN_TYPE(currentToken) == LEFT_PARENTHESES ? OP_GET_ATTR_CALL : OP_GET_ATTR, dotToken->line, dotToken->index, dotToken->sourceIndex);
    writeValConstant(currentChunk, attrName);
}

void binary(bool enforceReturn) {
    // Check if compiler optimization for left hand binary number operation available
    captureType leftCapture = capturedOperand;
    int32_t capturedLeftValue;
    if (leftCapture != CAPTURE_NONE) {
        // Store captured Value
        capturedLeftValue = capturedValue;
        // Reset
        capturedOperand = CAPTURE_NONE;
        capturedValue = 0;
    }

    token* prevToken = getPrevToken();
    ParseRule* rule = getRule(TOKEN_TYPE(prevToken));
    parsePrecedence((Precedence) (rule->precedence + 1), true);

    captureType rightCapture = capturedOperand;
    int32_t capturedRightValue;
    if (rightCapture != CAPTURE_NONE) {
        // Store captured Value
        capturedRightValue = capturedValue;
        // Reset
        capturedOperand = CAPTURE_NONE;
        capturedValue = 0;
    }

    switch (TOKEN_TYPE(prevToken)) {
        case PLUS: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL((double)capturedLeftValue + (double)capturedRightValue));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_ADD, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case MINUS: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL((double)capturedLeftValue - (double)capturedRightValue));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_SUB, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case MULTIPLY: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL((double)capturedLeftValue * (double)capturedRightValue));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_MUL, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case DIVIDE: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL((double)capturedLeftValue / (double)capturedRightValue));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_DIV, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case MOD: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL(fmod((double)capturedLeftValue, (double)capturedRightValue)));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_MOD, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case LESS: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, BOOL_VAL(leftCapture < rightCapture));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_LESS, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case MORE: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, BOOL_VAL(leftCapture > rightCapture));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_MORE, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case LESS_EQUAL: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, BOOL_VAL(leftCapture <= rightCapture));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_LESS_EQUAL, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case MORE_EQUAL: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, BOOL_VAL(leftCapture >= rightCapture));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_MORE_EQUAL, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case DOUBLE_EQUAL: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, BOOL_VAL(leftCapture == rightCapture));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_EQUAL, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case DOUBLE_AND: {
            WRITEOP_CURRENT_CHUNK(OP_AND, prevToken->line, prevToken->index, prevToken->sourceIndex);
            if (leftCapture != CAPTURE_NONE && rightCapture != CAPTURE_NONE) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Captured Value during 'and' operation");
            break;
        }
        case DOUBLE_OR: {
            WRITEOP_CURRENT_CHUNK(OP_OR, prevToken->line, prevToken->index, prevToken->sourceIndex);
            if (leftCapture != CAPTURE_NONE && rightCapture != CAPTURE_NONE) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Captured Value during 'or' operation");
            break;
        }
        case CARET: {
            if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) {
                WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
                writeValConstant(currentChunk, NUMBER_VAL(pow(capturedLeftValue, capturedRightValue)));
                return;
            } else {
                WRITEOP_CURRENT_CHUNK(OP_POW, prevToken->line, prevToken->index, prevToken->sourceIndex);
            }
            break;
        }
        case KEYWORD_IS: {
            WRITEOP_CURRENT_CHUNK(OP_IS, prevToken->line, prevToken->index, prevToken->sourceIndex);
            if (leftCapture != CAPTURE_NONE && rightCapture != CAPTURE_NONE) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Captured Value during 'is' operation");
            break;
        }
        default:
            compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Unhandled binary type.");
    }

    if (leftCapture == CAPTURE_PAYLOAD && rightCapture == CAPTURE_PAYLOAD) { // Constant folding
        compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Compiler optimization for constant folding not implemented");
    } else {
        uint8_t localAddrSlotIndex = 2;
        // Write right capture type
        writeChunk4(currentChunk, (uint8_t) leftCapture);
        // Write left capture type
        writeChunk4(currentChunk, (uint8_t) rightCapture);
        // Write right capture Value
        if (rightCapture == CAPTURE_PAYLOAD) {
            currentChunk->code[currentChunk->count-1] |= ((uint64_t)capturedRightValue << 32);
        } else if (rightCapture == CAPTURE_VARIABLE) {
            currentChunk->code[currentChunk->count-1] |= ((uint64_t)capturedRightValue << (localAddrSlotIndex * 8));
            localAddrSlotIndex += 2;
        }
        // Write left capture Value
        if (leftCapture == CAPTURE_PAYLOAD) {
            currentChunk->code[currentChunk->count-1] |= ((uint64_t)capturedLeftValue << 32);
        } else if (leftCapture == CAPTURE_VARIABLE) {
            currentChunk->code[currentChunk->count-1] |= ((uint64_t)capturedLeftValue << (localAddrSlotIndex * 8));
        }
    }
}

void unary(bool enforceReturn) {
    token* prevToken = getPrevToken();

    if (TOKEN_TYPE(prevToken) == KEYWORD_GLOBAL) {
        if (TOKEN_TYPE(currentToken) != IDENTIFIER) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected identifier after 'global'");
        WRITEOP_CURRENT_CHUNK(OP_GET_GLOBAL_REF_ATTR, prevToken->line, prevToken->index, prevToken->sourceIndex);
        writeChunk16(currentChunk, getGlobalRefIndex(TOKEN_VALUE(currentToken)));
        incCheckNull();
        return;
    }

    parsePrecedence(PREC_UNARY, true);

    switch (TOKEN_TYPE(prevToken)) {
        case NOT:
            WRITEOP_CURRENT_CHUNK(OP_NOT, prevToken->line, prevToken->index, prevToken->sourceIndex); break;
        case MINUS:
            WRITEOP_CURRENT_CHUNK(OP_NEGATE, prevToken->line, prevToken->index, prevToken->sourceIndex); break;
        default:
            compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Unhandled unary type.");
    }
}

void dictionary(bool enforceReturn) {
    // Create list object
    token* dictToken = getPrevToken();
    WRITEOP_CURRENT_CHUNK(OP_INIT, dictToken->line, dictToken->index, dictToken->sourceIndex);
    writeChunk16(currentChunk, getRefIndex(globalClassRefTable, "dict"));
    // Collect arguments
    uint8_t argCount = 0;
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) { // Init list with items
        expression(true);
        checkType(COLON, "Expected ':' after key");
        incCheckNull();
        expression(true);
        argCount+=2;
        // Check for comma
        if (TOKEN_TYPE(currentToken) == COMMA) {
            incCheckNull();
            if (TOKEN_TYPE(currentToken) == RIGHT_BRACE) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected expression after ','");
        }
    }
    // Skip right bracket
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(OP_EXEC_METHOD_IGNORE_RETURN, dictToken->line, dictToken->index, dictToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
}

bool isValidCapturingNeighbor(tokenType type) {
    switch (type) {
        case PLUS:
        case MINUS:
        case MULTIPLY:
        case DIVIDE:
        case MOD:
        case LESS:
        case MORE:
        case LESS_EQUAL:
        case MORE_EQUAL:
        case DOUBLE_EQUAL:
        case CARET:
            return true;
        default:
            return false;
    }
}

void number(bool enforceReturn) {
    token* numberToken = getPrevToken();
    double value = strtod(TOKEN_VALUE(numberToken), NULL);
    // Compiler optimization for left hand binary number operation
#ifdef OPTIMIZE_CONST_PAYLOAD
    bool isInt = fmod(value, (double) 1) == 0;
    if (value >= INT32_MIN && value <= INT32_MAX && isInt) {
        token* priorToken = numberToken->prevToken;
        if (isValidCapturingNeighbor(TOKEN_TYPE(currentToken)) || (priorToken != NULL && isValidCapturingNeighbor(TOKEN_TYPE(priorToken)))) {
            capturedOperand = CAPTURE_PAYLOAD;
            capturedValue = (int32_t)value;
            return;
        }
    }
#endif
    WRITEOP_CURRENT_CHUNK(OP_CONSTANT, numberToken->line, numberToken->index, numberToken->sourceIndex);
    writeValConstant(currentChunk, NUMBER_VAL(value));
}

void literal(bool enforceReturn) {
    token* prevToken = getPrevToken();
    WRITEOP_CURRENT_CHUNK(OP_CONSTANT, prevToken->line, prevToken->index, prevToken->sourceIndex);
    switch (TOKEN_TYPE(prevToken)) {
        case KEYWORD_TRUE:
            writeValConstant(currentChunk, BOOL_VAL(true));
            break;
        case KEYWORD_FALSE:
            writeValConstant(currentChunk, BOOL_VAL(false));
            break;
        case KEYWORD_NONE: {
            writeValConstant(currentChunk, NONE_VAL);
            break;
        }
        default:
            compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Unhandled literal type.");
    }
}

void functionCall(bool enforceReturn) {
    token* functionToken = getPrevToken();
    incCheckNull();
    token* callToken = getPrevToken();
    uint8_t argCount = parseCommaSequence(RIGHT_PARENTHESES);
    incCheckNull();
    WRITEOP_CURRENT_CHUNK(enforceReturn ? OP_EXEC_FUNCTION_ENFORCE_RETURN : OP_EXEC_FUNCTION_IGNORE_RETURN, callToken->line, callToken->index, callToken->sourceIndex);
    writeChunk8(currentChunk, argCount);
    uint16_t functionIndex = getRefIndex(prelinkedFuncTable, TOKEN_VALUE(functionToken));
    writeChunk16(currentChunk, functionIndex);
    // Add to prelinked call list
    preLinkedCallNode* node = malloc(sizeof(preLinkedCallNode));
    node->command = currentChunk->code[currentChunk->count-1];
    node->callToken = functionToken;
    node->next = preLinkedCallHead;
    preLinkedCallHead = node;
    // Set flag
    emittedCall = true;
}

void getVar(bool enforceReturn) {
    token* prevToken = getPrevToken();
    switch (TOKEN_TYPE(prevToken)) {
        case IDENTIFIER: {
            if (TOKEN_TYPE(currentToken) == LEFT_PARENTHESES) { // Execute function
                functionCall(enforceReturn);
            } else { // Get local ref
#ifdef OPTIMIZE_CONST_PAYLOAD
                // If not a global ref, check if compiler optimization for left and right hand binary number operation available
                if (!refTableContains(globalDeclTable, TOKEN_VALUE(prevToken))) {
                    token* priorToken = prevToken->prevToken;
                    if (isValidCapturingNeighbor(TOKEN_TYPE(currentToken)) || (priorToken != NULL && isValidCapturingNeighbor(TOKEN_TYPE(priorToken)))) {
                        capturedOperand = CAPTURE_VARIABLE;
                        capturedValue = getRefIndex(currentLocalRefTable, TOKEN_VALUE(prevToken));
                        return;
                    }
                }
#endif
                WRITEOP_CURRENT_CHUNK(OP_GET_COMBINED_REF_ATTR, prevToken->line, prevToken->index, prevToken->sourceIndex);
                // Write local ref array index
                writeChunk16(currentChunk, getRefIndex(currentLocalRefTable, TOKEN_VALUE(prevToken)));
                // Write global ref array index
                writeChunk16(currentChunk, getGlobalRefIndex(TOKEN_VALUE(prevToken)));
            }
            break;
        }
        case KEYWORD_SELF: {
            if (!isMethodChunk) compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Self can only be used in method");
            WRITEOP_CURRENT_CHUNK(OP_GET_SELF, prevToken->line, prevToken->index, prevToken->sourceIndex);
            break;
        }
        default:
            compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Unhandled getVar type.");
    }

}

void getIndexRef(bool enforceReturn) {
    expression(true);
    if (TOKEN_TYPE(currentToken) != RIGHT_BRACKET) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ']'");
    WRITEOP_CURRENT_CHUNK(OP_GET_INDEX_REF, currentToken->line, currentToken->index, currentToken->sourceIndex);
    incCheckNull();
}

void parsePrecedence(Precedence precedence, bool enforceReturn) {
    incCheckNull();
    token* prevToken = getPrevToken();
    ParseFn prefixRule = getRule(TOKEN_TYPE(prevToken))->prefix;
    if (prefixRule == NULL) compilationError(prevToken->line, prevToken->index, prevToken->sourceIndex, "Expected expression.");

    prefixRule(enforceReturn);

    while (precedence <= getRule(TOKEN_TYPE(currentToken))->precedence) {
        incCheckNull();
        ParseFn infixRule = getRule(TOKEN_TYPE(getPrevToken()))->infix;
        if (infixRule == NULL) compilationError(getPrevToken()->line, getPrevToken()->index, getPrevToken()->sourceIndex, "Invalid infix expression.");
        infixRule(enforceReturn);
    }
}

void expression(bool enforceReturn) {
    parsePrecedence(PREC_ASSIGNMENT, enforceReturn);
}

bool isValidBeginOp(OpCode op) {
    switch (op) {
        case OP_GET_GLOBAL_REF_ATTR:
        case OP_GET_COMBINED_REF_ATTR:
        case OP_GET_SELF:
            return true;
        default:
            return false;
    }
}

bool isValidEndOp(OpCode op) {
    switch (op) {
        case OP_GET_GLOBAL_REF_ATTR:
        case OP_GET_COMBINED_REF_ATTR:
        case OP_GET_INDEX_REF:
        case OP_GET_ATTR:
            return true;
        default:
            return false;
    }
}

OpCode swapEndOp(OpCode op) {
    switch (op) {
        case OP_GET_GLOBAL_REF_ATTR: return OP_SET_GLOBAL_REF_ATTR;
        case OP_GET_COMBINED_REF_ATTR: return OP_SET_COMBINED_REF_ATTR;
        case OP_GET_INDEX_REF: return OP_SET_INDEX_REF;
        case OP_GET_ATTR: return OP_SET_ATTR;
        default:
            compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Invalid end operation swap");
            return OP_RETURN;
    }
}

uint8_t opArgCount(OpCode op) {
    switch (op) {
        case OP_GET_GLOBAL_REF_ATTR: return 2;
        case OP_GET_COMBINED_REF_ATTR: return 4;
        case OP_GET_INDEX_REF: return 0;
        case OP_GET_ATTR: return 1;
        default:
            compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Invalid end operation swap");
            return 0;
    }
}

specialAssignment toSpecialAssignType(tokenType token) {
    switch (token) {
        case PLUS_EQUAL:
            return ASSIGNMENT_ADD;
        case MINUS_EQUAL:
            return ASSIGNMENT_SUB;
        case MULTIPLY_EQUAL:
            return ASSIGNMENT_MUL;
        case DIVIDE_EQUAL:
            return ASSIGNMENT_DIV;
        case MOD_EQUAL:
            return ASSIGNMENT_MOD;
        case CARET_EQUAL:
            return ASSIGNMENT_POWER;
        case EQUAL:
            return ASSIGNMENT_NONE; // You may return something different here if EQUAL is not considered a special assignment
        default: {
            compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Invalid special assignment type");
            // Handle error or return a default Value if the token does not represent a special assignment
            return ASSIGNMENT_NONE;
        }
    }
}


void assignment() {
    // Note Chunk Starting Index
    uint32_t chunkStartIndex = currentChunk->count;
    // Parse left hand side
    expression(true);
    // Get beginning and end operations
    uint64_t beginLine = currentChunk->code[chunkStartIndex];
    OpCode beginOp = (uint8_t)(beginLine & 0xFF);
    uint64_t endLine = currentChunk->code[currentChunk->count-1];
    OpCode endOp = (uint8_t)(endLine & 0xFF);
    // Check beginning and end operations
    if (!(isValidBeginOp(beginOp) && isValidEndOp(endOp))) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Invalid left hand side of assignment");
    // Rewind by one instruction
    currentChunk->count -= 1;
    // Parse right hand side
    if (!isAssignmentOperator(TOKEN_TYPE(currentToken))) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Internal error: assignment token mismatch");
    token* assignmentToken = currentToken;
    tokenType assignmentType = TOKEN_TYPE(assignmentToken);
    incCheckNull();
    expression(true);
    // Modify line
    endLine = SET_BOTTOM_8_BITS(endLine, swapEndOp(endOp));
    uint8_t argCount = opArgCount(endOp);
    uint8_t specialAssignment = toSpecialAssignType(assignmentType);
    endLine |= ((uint64_t)specialAssignment << ((argCount + 1)*8));
    if (endOp == OP_GET_COMBINED_REF_ATTR) {
        // Add to chunk set index for compiler optimization
        chunkSetIndexArray[chunkSetIndexArrayIndex++] = (uint16_t)((endLine >> (1 * 8)) & 0xFFFF);
    }
    // Write to chunk
    writeLine(currentChunk, endLine, assignmentToken->line, assignmentToken->index, assignmentToken->sourceIndex);
}

void returnStatement() {
    token* returnToken = currentToken;
    // Skip return token
    incCheckNull();
    // Check for return type
    if (isVoidReturnChunk) {
        if (TOKEN_TYPE(currentToken) != SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ';' after return `statement` for void callable");
        // Add return instruction
        WRITEOP_CURRENT_CHUNK(OP_RETURN, returnToken->line, returnToken->index, returnToken->sourceIndex);
    } else {
        if (TOKEN_TYPE(currentToken) == SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected expression after return statement for non-void callable");
        // Parse expression
        expression(true);
        WRITEOP_CURRENT_CHUNK(OP_RETURN, returnToken->line, returnToken->index, returnToken->sourceIndex);
    }
}

void elseStatement() {
    incCheckType(LEFT_BRACE, "Expected '{' after else");
    incCheckNull();
    // Parse else body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();
    // Load function body
    checkType(RIGHT_BRACE, "Expected '}' after if or elif body");
    incCheckNull();
}

void ifStatement() {
    token* ifToken = currentToken;
    // Check formatting
    incCheckType(LEFT_PARENTHESES, "Expected '(' after 'if' or 'elif'");
    incCheckNull();
    // Parse condition
    expression(true);
    // Check formatting
    checkType(RIGHT_PARENTHESES, "Expected ')' after condition");
    incCheckType(LEFT_BRACE, "Expected '{' after condition");
    incCheckNull();
    // Create jump
    uint16_t jumpNextConditionChunkIndex = writeJump(currentChunk, OP_JUMP_IF_FALSE, ifToken->line, ifToken->index, ifToken->sourceIndex);
    // Parse if body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();
    // Load function bodyc
    checkType(RIGHT_BRACE, "Expected '}' after if or elif body");
    incCheckNull();
    // Check for elif or else
    switch (TOKEN_TYPE(currentToken)) {
        case KEYWORD_ELIF:
        case KEYWORD_ELSE: {
            // Create end of body jump to skip else body
            uint16_t jumpEndChunkIndex = writeJump(currentChunk, OP_JUMP, ifToken->line, ifToken->index, ifToken->sourceIndex);
            // Patch the condition jump
            patchJumpAtCurrent(currentChunk, jumpNextConditionChunkIndex);
            if (TOKEN_TYPE(currentToken) == KEYWORD_ELIF) {
                ifStatement();
            } else {
                elseStatement();
            }
            // Patch the end of body jump
            patchJumpAtCurrent(currentChunk, jumpEndChunkIndex);
            break;
        }
        default:
            // Patch jump to end of if statement
            patchJumpAtCurrent(currentChunk, jumpNextConditionChunkIndex);
            break;
    }
}

void whileStatement() {
    token* whileToken = currentToken;
    // Check formatting
    incCheckType(LEFT_PARENTHESES, "Expected '(' after 'while'");
    incCheckNull();

    // Note expression start location
    uint16_t expressionStartChunkIndex = CURR_CHUNK_INDEX;

    // Parse condition
    expression(true);

    // Add jump if false instruction
    uint16_t jumpEndChunkIndex = writeJump(currentChunk, OP_JUMP_IF_FALSE, whileToken->line, whileToken->index, whileToken->sourceIndex);

    // Check formatting
    checkType(RIGHT_PARENTHESES, "Expected ')' after condition");
    incCheckType(LEFT_BRACE, "Expected '{' after condition");
    incCheckNull();

    // Store previous jump list pointers
    uint16_t* prevContinueJumpList = continueJumpList;
    uint16_t* prevBreakJumpList = breakJumpList;
    uint8_t prevContinueJumpIndex = continueJumpIndex;
    uint8_t prevBreakJumpIndex = breakJumpIndex;

    // Create new jump list pointers
    continueJumpList = malloc(sizeof(uint16_t) * CONTINUE_JUMP_LIST_INIT_SIZE);
    breakJumpList = malloc(sizeof(uint16_t) * BREAK_JUMP_LIST_INIT_SIZE);
    continueJumpIndex = 0;
    breakJumpIndex = 0;

    // Parse while body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();

    // Load function bodyc
    checkType(RIGHT_BRACE, "Expected '}' after while body");

    // Add jump back instruction
    writeJumpBack(currentChunk, OP_JUMP, expressionStartChunkIndex, currentToken->line, currentToken->index, currentToken->sourceIndex);
    incCheckNull();

    // Patch break & continue jumps
    patchBreakJumpsAtCurrent();
    patchContinueJumps(expressionStartChunkIndex);

    // Free current jump list pointers
    free(continueJumpList);
    free(breakJumpList);

    // Replace previous jump list pointers
    continueJumpList = prevContinueJumpList;
    breakJumpList = prevBreakJumpList;
    continueJumpIndex = prevContinueJumpIndex;
    breakJumpIndex = prevBreakJumpIndex;

    // Patch jump
    patchJumpAtCurrent(currentChunk, jumpEndChunkIndex);
}

void standardStatement() {
    if (isAssignmentStatement()) {
        assignment();
    } else {
        token* lineStartToken = currentToken;
        // Reset flag
        emittedCall = false;
        expression(false);
        // Check if flag is raised
        if (!emittedCall) compilationError(lineStartToken->line, lineStartToken->index, lineStartToken->sourceIndex, "Non-return, function methodCall, or assignment statement");
    }
}

void forStatement() {
    token* forToken = currentToken;
    // Check formatting
    incCheckType(LEFT_PARENTHESES, "Expected '(' after 'for'");
    incCheckNull();
    // Pre-loop statement
    standardStatement();
    // Check formatting
    checkType(SEMICOLON, "Expected ';' after pre-loop statement");
    incCheckNull();
    // Note expression start location
    uint16_t expressionStartChunkIndex = CURR_CHUNK_INDEX;
    // Parse condition
    expression(true);
    // Check formatting
    checkType(SEMICOLON, "Expected ';' after condition");
    incCheckNull();
    // Add jump if false instruction
    uint16_t jumpEndChunkIndex = writeJump(currentChunk, OP_JUMP_IF_FALSE, forToken->line, forToken->index, forToken->sourceIndex);
    // Store post-loop statement start chunk index
    uint16_t postLoopChunkIndex = CURR_CHUNK_INDEX;
    // Parse post-loop statement
    standardStatement();
    // Check formatting
    checkType(RIGHT_PARENTHESES, "Expected ')' after post-loop statement");
    incCheckType(LEFT_BRACE, "Expected '{' after post-loop statement");
    incCheckNull();
    // Crop post-loop chunk
    Chunk* postLoopChunk = cropChunk(currentChunk, postLoopChunkIndex);

    // Store previous jump list pointers
    uint16_t* prevContinueJumpList = continueJumpList;
    uint16_t* prevBreakJumpList = breakJumpList;
    uint8_t prevContinueJumpIndex = continueJumpIndex;
    uint8_t prevBreakJumpIndex = breakJumpIndex;

    // Create new jump list pointers
    continueJumpList = malloc(sizeof(uint16_t) * CONTINUE_JUMP_LIST_INIT_SIZE);
    breakJumpList = malloc(sizeof(uint16_t) * BREAK_JUMP_LIST_INIT_SIZE);
    continueJumpIndex = 0;
    breakJumpIndex = 0;

    // Parse for body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();
    incCheckNull();

    // Patch continue jumps
    patchContinueJumps(CURR_CHUNK_INDEX);

    // Copy post-loop chunk
    copyChunk(currentChunk, postLoopChunk);
    // Free post-loop chunk
    freeChunk(postLoopChunk);
    // Add jump back instruction
    writeJumpBack(currentChunk, OP_JUMP, expressionStartChunkIndex, currentToken->line, currentToken->index, currentToken->sourceIndex);

    // Patch break jumps
    patchBreakJumpsAtCurrent();

    // Patch jump
    patchJumpAtCurrent(currentChunk, jumpEndChunkIndex);

    // Free current jump list pointers
    free(continueJumpList);
    free(breakJumpList);

    // Replace previous jump list pointers
    continueJumpList = prevContinueJumpList;
    breakJumpList = prevBreakJumpList;
    continueJumpIndex = prevContinueJumpIndex;
    breakJumpIndex = prevBreakJumpIndex;
}

void breakStatement() {
    token* breakToken = currentToken;
    if (continueJumpList == NULL || breakJumpList == NULL) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Break statement outside of loop");
    incCheckNull();
    if (breakJumpIndex >= BREAK_JUMP_LIST_INIT_SIZE) compilationError(breakToken->line, breakToken->index, breakToken->sourceIndex, "Break statement overflow within loop");
    // Add jump to break list
    breakJumpList[breakJumpIndex++] = writeJump(currentChunk, OP_JUMP, breakToken->line, breakToken->index, breakToken->sourceIndex);
}

void continueStatement() {
    token* continueToken = currentToken;
    if (continueJumpList == NULL || breakJumpList == NULL) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Continue statement outside of loop");
    incCheckNull();
    if (continueJumpIndex >= CONTINUE_JUMP_LIST_INIT_SIZE) compilationError(continueToken->line, continueToken->index, continueToken->sourceIndex, "Continue statement overflow within loop");
    // Add jump to continue list
    continueJumpList[continueJumpIndex++] = writeJump(currentChunk, OP_JUMP, continueToken->line, continueToken->index, continueToken->sourceIndex);
}

void statement() {
    switch (TOKEN_TYPE(currentToken)) {
        case KEYWORD_IF:
            ifStatement();
            break;
        case KEYWORD_WHILE:
            whileStatement();
            break;
        case KEYWORD_FOR:
            forStatement();
            break;
        case KEYWORD_BREAK: {
            breakStatement();
            if (TOKEN_TYPE(currentToken) != SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ';'");
            incCheckNull();
            break;
        }
        case KEYWORD_CONTINUE: {
            continueStatement();
            if (TOKEN_TYPE(currentToken) != SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ';'");
            incCheckNull();
            break;
        }
        case KEYWORD_RETURN: {
            returnStatement();
            if (TOKEN_TYPE(currentToken) != SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ';'");
            incCheckNull();
            break;
        }
        default: {
            standardStatement();
            if (TOKEN_TYPE(currentToken) != SEMICOLON) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected ';'");
            incCheckNull();
        }
    }
}

Value defMethod(bool isVoidReturn, bool isInit) {
#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    char* funcName;
    if (isInit) {
        funcName = "init";
    } else {
        checkType(IDENTIFIER, "Expected method name");
        funcName = TOKEN_VALUE(currentToken);
    }
#endif
    incCheckType(LEFT_PARENTHESES, "Expected '(' after method name");
    incCheckNull();

    // Create new chunk
    Chunk* methodChunk = createChunk();
    uint8_t inCount = 0;

    // Set current chunk
    setCurrentChunk(methodChunk);
    // Set method flag
    isMethodChunk = true;
    // Set init method flag
    isInitMethodChunk = isInit;
    // Set return type
    isVoidReturnChunk = isVoidReturn;

    // Add self reference
    chunkSetIndexArray[chunkSetIndexArrayIndex++] = getRefIndex(currentLocalRefTable, "self");

    // Load arguments
    while (TOKEN_TYPE(currentToken) == IDENTIFIER) {
        // Create local reference
        chunkSetIndexArray[chunkSetIndexArrayIndex++] = getRefIndex(currentLocalRefTable, TOKEN_VALUE(currentToken));
        // Increment token
        incCheckNull();
        // Check for comma
        if (TOKEN_TYPE(currentToken) == COMMA) incCheckType(IDENTIFIER, "Expected identifier after ','");
        inCount++;
    }
    // Check for format
    checkType(RIGHT_PARENTHESES, "Expected ')' after method arguments");
    incCheckType(LEFT_BRACE, "Expected '{' after method arguments");
    incCheckNull();

    // Create new object
    Value methodObject = OBJECT_VAL(createConstCallableObject(CREATE_CHUNK_METHOD(inCount, isVoidReturn ? 0 : 1, methodChunk)), BUILTIN_CALLABLE);

    // Add to chunk list
    listAddElement(chunkArray, methodObject);

    // Parse function body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();

    // Load function bodyc
    checkType(RIGHT_BRACE, "Expected '}' after method body");

    // Add return instruction
    if (!isVoidReturn) { // Non-Void return
        WRITEOP_CURRENT_CHUNK(OP_RETURN_NONE, currentToken->line, currentToken->index, currentToken->sourceIndex);
    } else {
        WRITEOP_CURRENT_CHUNK(OP_RETURN, currentToken->line, currentToken->index, currentToken->sourceIndex);
    }

    // Clear init method flag
    isInitMethodChunk = false;
    // Clear method flag
    isMethodChunk = false;

#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    printf("\nMethod \"%s\" defined (in: %d, out: %d):\n", funcName, inCount, isVoidReturn ? 0 : 1);
    printChunk(methodChunk);
#endif

    // Clear current chunk
    clearCurrentChunk();

    incCheckNull();
    return methodObject;
}

void defClass() {
    // Skip class token
    incCheckType(IDENTIFIER, "Expected class name");
    char* className = TOKEN_VALUE(currentToken);

    incCheckNull();
    uint32_t pClassID;
    bool hasParent = false;
    // Determine if class is a subclass
    if (TOKEN_TYPE(currentToken) == LEFT_PARENTHESES) {
        incCheckType(IDENTIFIER, "Expected parent class name");
        // Get parent class
        pClassID = getRefIndex(globalClassRefTable,TOKEN_VALUE(currentToken));
        incCheckType(RIGHT_PARENTHESES, "Expected ')' after parent class name");
        incCheckNull();
        hasParent = true;
    }
    // Check for opening brace
    if (TOKEN_TYPE(currentToken) != LEFT_BRACE) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected '{' after class name");
    incCheckNull();
    // Check for init method
    checkType(KEYWORD_VOID, "Init method must be void");
    incCheckNull();
    if (TOKEN_TYPE(currentToken) != KEYWORD_INIT) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Expected init method");
    Value initMethod = defMethod(true, true);

    // Create class
    objClass* currClass = createClass(className, getRefIndex(globalClassRefTable,className), initMethod, hasParent ? classArray[pClassID] : NULL, CHUNK_FUNC_INIT_TYPE);

    while (TOKEN_TYPE(currentToken) == KEYWORD_VOID || TOKEN_TYPE(currentToken) == IDENTIFIER) {
        bool isVoidReturn = TOKEN_TYPE(currentToken) == KEYWORD_VOID;
        if (TOKEN_TYPE(currentToken) == KEYWORD_VOID) incCheckType(IDENTIFIER, "Expected method name");
        char* methodName = TOKEN_VALUE(currentToken);
        Value methodObject = defMethod(isVoidReturn, false);
        CLASS_ADD_ATTR(currClass, methodName, methodObject);
    }

    checkType(RIGHT_BRACE, "Expected '}' after class body");

#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    printf("\nClass \"%s\" defined:\n", className);
    printObjClass((Value) currClass);
#endif
}

void defFunction(bool isVoidReturn) {
    incCheckType(IDENTIFIER, "Expected function name");
    char* funcName = TOKEN_VALUE(currentToken);

    incCheckType(LEFT_PARENTHESES, "Expected '(' after function name");
    incCheckNull();

    // Create new chunk
    Chunk* functionChunk = createChunk();
    uint8_t inCount = 0;

    // Set current chunk
    setCurrentChunk(functionChunk);
    // Set return type
    isVoidReturnChunk = isVoidReturn;

    // Load arguments
    if (strcmp(funcName, "main") == 0) {
        // Main function
        if (TOKEN_TYPE(currentToken) == IDENTIFIER) {
            if (strcmp(TOKEN_VALUE(currentToken), "inArgs") != 0) compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "main function can only have one argument: \"inArgs\" or no arguments");
            // Create local reference
            chunkSetIndexArray[chunkSetIndexArrayIndex++] = getRefIndex(currentLocalRefTable, TOKEN_VALUE(currentToken));
            // Increment token
            incCheckNull();
            inCount++;
        }
    } else {
        // Other functions
        while (TOKEN_TYPE(currentToken) == IDENTIFIER) {
            // Here for each of the arguments, we create a local reference, frame will be shifted by the appropriate amount
            // therefore popping and pushing will not be necessary
            // Create local reference
            chunkSetIndexArray[chunkSetIndexArrayIndex++] = getRefIndex(currentLocalRefTable, TOKEN_VALUE(currentToken));
            // Increment token
            incCheckNull();
            // Check for comma
            if (TOKEN_TYPE(currentToken) == COMMA) incCheckType(IDENTIFIER, "Expected identifier after ','");
            inCount++;
        }
    }
    // Check for format
    checkType(RIGHT_PARENTHESES, "Expected ')' after function arguments");
    incCheckType(LEFT_BRACE, "Expected '{' after function arguments");
    incCheckNull();

    // Create new object
    Value functionObject = OBJECT_VAL(CREATE_BUILTIN_CHUNK_FUNCTION_OBJECT(functionChunk, inCount, isVoidReturn ? 0 : 1), BUILTIN_CALLABLE);
    // Add to chunk list
    listAddElement(chunkArray, functionObject);

    // Add function object to global reference table
    addGlobalReference(globalRefTable, globalRefList, functionObject, funcName);

    // Parse function body
    while (TOKEN_TYPE(currentToken) != RIGHT_BRACE) statement();

    // Load function body
    checkType(RIGHT_BRACE, "Expected '}' after function body");

    // Add return instruction
    if (!isVoidReturn) { // Non-void return
        WRITEOP_CURRENT_CHUNK(OP_RETURN_NONE, currentToken->line, currentToken->index, currentToken->sourceIndex);
    } else {
        WRITEOP_CURRENT_CHUNK(OP_RETURN, currentToken->line, currentToken->index, currentToken->sourceIndex);
    }


#ifdef DEBUG_PRINT_PRIOR_TO_OPTIMIZATION
    printf("\nFunction \"%s\" defined (in: %d, out: %d):\n", funcName, inCount, isVoidReturn ? 0 : 1);
    printChunk(currentChunk);
#endif
    // Clear current chunk
    clearCurrentChunk();
}

callable** checkPrelinkedCall() {
    // Allocate function array
    callable** functionArray = (callable**) malloc(sizeof(callable*) * prelinkedFuncTable->numEntries);
    for (int i = 0; i < prelinkedFuncTable->numEntries; i++) functionArray[i] = NULL;
    // Iterate through prelinked function linkedlist
    preLinkedCallNode* currNode = preLinkedCallHead;
    while (currNode != NULL) {
        uint64_t command = currNode->command;
        token* callToken = currNode->callToken;
        Value targetFunc = listGetElement(globalRefList, getGlobalRefIndex(callToken->value));
        // Check if function exists
        if (IS_INTERNAL_NULL(targetFunc)) compilationError(callToken->line, callToken->index, callToken->sourceIndex, "Undefined function");
        if (VALUE_TYPE(targetFunc) != BUILTIN_CALLABLE) compilationError(callToken->line, callToken->index, callToken->sourceIndex, "Object is not callable");
        if (VALUE_CALLABLE_VALUE(targetFunc)->in != -1 && VALUE_CALLABLE_VALUE(targetFunc)->in != GET_BYTE(command, 1)) compilationError(callToken->line, callToken->index, callToken->sourceIndex, "Incorrect number of arguments");
        // Check if already written in function array
        uint16_t functionIndex = GET_WORD(command, 2);
        if (functionArray[functionIndex] == NULL) functionArray[functionIndex] = VALUE_CALLABLE_VALUE(targetFunc);
        // Free node
        preLinkedCallNode* nextNode = currNode->next;
        free(currNode);
        currNode = nextNode;
    }
    return functionArray;
}

Value* compactGlobalRefTable() {
    // globalRefTable, globalRefList, chunkArray
    int32_t* grMap = (int32_t*) malloc(sizeof(int32_t) * globalRefList->size);
    for (uint32_t i=0; i<globalRefList->size; i++) grMap[i] = -1;
    // Iterate chunk array
    for (uint32_t i=0; i<chunkArray->size; i++) {
        Chunk* currChunk = chunkArray->list[i].obj->primValue.call->func;
        uint64_t* currLine = currChunk->code;
        for (uint32_t j=0; j<currChunk->count; j++) {
            OpCode op = (uint8_t)(*currLine & 0xFF);
            if (op == OP_GET_GLOBAL_REF_ATTR || op == OP_SET_GLOBAL_REF_ATTR) {
                uint16_t globalRefIndex = (uint16_t)((*currLine >> (1 * 8)) & 0xFFFF);
                grMap[globalRefIndex] = 1;
            }
            // Increment line
            currLine++;
        }
    }
    // Recalculate compacted global ref table indices
    uint32_t count = 0;
    for (uint32_t i=0; i<globalRefList->size; i++) if (grMap[i] == 1) grMap[i] = count++;
    // Compact global ref table
    Value* newGRArray = (Value*) malloc(sizeof(Value) * count);
    *GAsize = count;
    for (uint32_t i=0; i<globalRefList->size; i++) {
        if (grMap[i] != -1) newGRArray[grMap[i]] = globalRefList->list[i];
    }
    // Update chunk
    for (uint32_t i=0; i<chunkArray->size; i++) {
        Chunk* currChunk = chunkArray->list[i].obj->primValue.call->func;
        uint64_t *currLine = currChunk->code;
        for (uint32_t j=0; j<currChunk->count; j++) {
            OpCode op = (uint8_t)(*currLine & 0xFF);
            if (op == OP_GET_COMBINED_REF_ATTR || op == OP_SET_COMBINED_REF_ATTR) {
                uint16_t globalRefIndex = (uint16_t)((*currLine >> (3 * 8)) & 0xFFFF);
                // Determine if global ref is in compacted global ref table
                if (grMap[globalRefIndex] == -1) {
                    // Change to get local
                    if (op == OP_GET_COMBINED_REF_ATTR) {
                        *currLine = SET_BOTTOM_8_BITS(*currLine, OP_GET_LOCAL_REF_ATTR);
                    } else {
                        *currLine = SET_BOTTOM_8_BITS(*currLine, OP_SET_LOCAL_REF_ATTR);
                        // Get special assignment
                        specialAssignment sa = GET_BYTE(*currLine, 5);
                        // Clear byte after initial 24 bite
                        *currLine &= ~(0xFFULL << 24);
                        // Write to line after initial 24 bits
                        *currLine |= ((uint64_t)sa << 24);
                    }
                } else {
                    // Change to new index
                    // Clear the next 16 bite after initial 24 bits
                    *currLine &= ~(0xFFFFULL << 24);
                    // Set the next 16 bits to the new mapped index
                    *currLine |= ((uint64_t)grMap[globalRefIndex] << 24);
                }
            } else if (op == OP_GET_GLOBAL_REF_ATTR || op == OP_SET_GLOBAL_REF_ATTR) {
                // Change to new index
                uint16_t globalRefIndex = (uint16_t)((*currLine >> (1 * 8)) & 0xFFFF);
                // Clear the next 16 bits after the 8-bit opcode
                *currLine &= ~(0xFFFFULL << 8);
                // Set the next 16 bits to the new mapped index
                *currLine |= ((uint64_t)grMap[globalRefIndex] << 8);
            }
            // Increment line
            currLine++;
        }
#ifdef DEBUG_PRINT_CHUNK_AFTER_GLOBAL_OPTIMIZATION
        printf("\nAfter global Optimization: \n");
        printChunk(currChunk);
#endif
    }
#ifdef DEBUG_PRINT_CHUNK_AFTER_CREATION
    printf("\nGlobal array after optimization size[%u]: \n[", count);
    for (uint32_t i=0; i<count; i++) {
        DSPrintValue(newGRArray[i]);
        if (i != count-1) printf(", ");
    }
    printf("] \n");
#endif
    free(grMap);

    return newGRArray;
}

// Returns the index of main function
Value compile(refTable* GRTable, refTable* globalClassTable, runtimeList* GRList, callable*** functionArray, Value** globalArray, uint32_t* globalArraySize) {
    // Tokenize source and build global reference table
    globalDeclTable = tokenize();

    // Create global reference
    globalRefTable = GRTable;
    // Create global class reference
    globalClassRefTable = globalClassTable;
    // Create global reference list
    globalRefList = GRList;
    // Set method flag
    isMethodChunk = false;
    // Set init method flag
    isInitMethodChunk = false;

    // Initialize jump lists
    continueJumpList = NULL;
    breakJumpList = NULL;
    continueJumpIndex = 0;
    breakJumpIndex = 0;

    // Initialize chunk set array index
    chunkSetIndexArrayIndex = 0;

    // Initialize left hand binary capture
    capturedOperand = CAPTURE_NONE;
    capturedValue = 0;

    // Initialize constant hashString
    compilerConstantHash = createRuntimeDict(RUNTIME_DICT_INIT_SIZE);

    // Initialize function table
    prelinkedFuncTable = createRefTable(GLOBAL_REF_TABLE_INIT_SIZE);

    // Initialize chunk array
    chunkArray = createRuntimeList(RUNTIME_LIST_INIT_SIZE);

    while (1) {
        INC_TOKEN();
        if (currentToken == NULL) break;
        switch (TOKEN_TYPE(currentToken)) {
            case KEYWORD_CLASS: {
                defClass();
                break;
            }
            case KEYWORD_FUNCTION: {
                defFunction(false);
                break;
            }
            case KEYWORD_VOID: {
                incCheckType(KEYWORD_FUNCTION, "Expected 'function' after 'void'");
                defFunction(true);
                break;
            }
            case KEYWORD_INCLUDE: {
                incCheckNull(IDENTIFIER, "Expected identifier after 'include'");
                break;
            }
            default:
                compilationError(currentToken->line, currentToken->index, currentToken->sourceIndex, "Unexpected token");
        }
    }

    bool mainFound = refTableContains(globalRefTable, "main");
    Value mainFunc = INTERNAL_NULL_VAL;
    if (mainFound) mainFunc = globalRefList->list[getGlobalRefIndex("main")];

    *functionArray = checkPrelinkedCall();

#ifdef DEBUG_PRINT_PRELINKED_FUNC_LIST
    printf("Prelinked function table: \n");
    printRefTable(prelinkedFuncTable);
    printf("\n");
#endif

    // Check if all classes has been defined
    for (uint32_t i=0; i<globalClassTable->numEntries; i++) {
        if (classArray[i] == NULL) compilationError(0, 0, 0, "Undefined class");
    }

    // Set total amount of class
    setTotalClassCount(globalClassTable->numEntries);

    // Compact global reference
    GAsize = globalArraySize;
    *globalArray = compactGlobalRefTable();

    // Create chunk array and attach to error handler
    Chunk** ca = (Chunk**) malloc(sizeof(Chunk*) * chunkArray->size);
    for (uint32_t i=0; i<chunkArray->size; i++) {
        ca[i] = chunkArray->list[i].obj->primValue.call->func;
    }
    attachChunkArray(ca, chunkArray->size);

    freeRuntimeList(chunkArray);
    freeRefTable(globalRefTable);
    freeRefTable(globalClassTable);
    freeRefTable(prelinkedFuncTable);
    freeRefTable(globalDeclTable);
    freeRuntimeDict(compilerConstantHash);
    freeTokenizer();

    if (!mainFound) compilationError(0, 0, 0, "No main function found");

    printf("Compilation Successful\n\n");
    return mainFunc;
}