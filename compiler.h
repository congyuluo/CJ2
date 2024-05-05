//
// Created by congyu on 7/29/23.
//

#ifndef CJ_2_COMPILER_H
#define CJ_2_COMPILER_H

#include "object.h"
#include "refManager.h"
#include "tokenizer.h"

void expression(bool enforceReturn);

typedef void (*ParseFn)(bool enforceReturn);

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_EXPONENT,    // ^
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef enum captureType {
    CAPTURE_NONE,
    CAPTURE_PAYLOAD,
    CAPTURE_VARIABLE
} captureType;

void parsePrecedence(Precedence precedence, bool enforceReturn);

typedef struct preLinkedCallNode preLinkedCallNode;

struct preLinkedCallNode {
    preLinkedCallNode* next;
    uint64_t command;
    token* callToken;
};

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

void grouping(bool enforceReturn);
void methodCall(bool enforceReturn);
void newObject(bool enforceReturn);
void parentInit(bool enforceReturn);
void list(bool enforceReturn);
void set(bool enforceReturn);
void string(bool enforceReturn);
void dot(bool enforceReturn);
void binary(bool enforceReturn);
void unary(bool enforceReturn);
void dictionary(bool enforceReturn);
void number(bool enforceReturn);
void literal(bool enforceReturn);
void getVar(bool enforceReturn);
void getIndexRef(bool enforceReturn);

// Statement parsing
void statement();
void assignment();
void returnStatement();
void ifStatement();
void whileStatement();
void forStatement();

// Declaration parsing
void defClass();
Value defMethod(bool isVoidReturn, bool isInit);
void defFunction(bool isVoidReturn);

Value compile(refTable* GRTable, refTable* globalClassTable, runtimeList* GRList, callable*** functionArray, Value** globalArray, uint32_t* globalArraySize);

#endif //CJ_2_COMPILER_H
