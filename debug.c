//
// Created by congyu on 7/18/23.
//

#include "debug.h"
#include "errors.h"
#include "runtimeDS.h"
#include "compiler.h"

#define GET_NIBBLE(data, shift) ((uint8_t)(((data) >> ((shift) * 4)) & 0xF))
#define GET_BYTE(data, shift)  ((uint8_t) (((data) >> ((shift) * 8)) & 0xFF))
#define GET_WORD(data, shift) ((uint16_t)(((data) >> ((shift) * 8)) & 0xFFFF))
#define GET_DWORD(data, shift) ((uint32_t)(((data) >> ((shift) * 8)) & 0xFFFFFFFF))

void printConstOp(char* name, Chunk* c, uint64_t line) {
    printf("%s", name);
}

void printConstOpWithPayload(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    captureType leftType = GET_NIBBLE(line, 2);
    captureType rightType = GET_NIBBLE(line, 3);
    uint8_t localAddrSlot = 2;
    printf("    Right Op: ");
    switch (rightType) {
        case CAPTURE_NONE:
            printf("No payload");
            break;
        case CAPTURE_PAYLOAD:
            printf("const -> %d", GET_DWORD(line, 4));
            break;
        case CAPTURE_VARIABLE: {
            printf("var -> %u", GET_WORD(line, localAddrSlot));
            localAddrSlot += 2;
            break;
        }
        default:
            parsingError(0, 0, 0, "Disassembler: Unknown right constant payload opcode\n");
    }
    printf("\n    Left Op: ");
    switch (leftType) {
        case CAPTURE_NONE:
            printf("No payload");
            break;
        case CAPTURE_PAYLOAD:
            printf(" const -> %d", GET_DWORD(line, 4));
            break;
        case CAPTURE_VARIABLE: {
            printf(" var -> %u", GET_WORD(line, localAddrSlot));
            break;
        }
        default:
            parsingError(0, 0, 0, "Disassembler: Unknown left constant payload opcode\n");
    }
}

void printSingleOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    Var1 -> ");
    DSPrintValue(c->constants->data[GET_BYTE(line, 1)]);
    printf(" (constant #%u)", GET_BYTE(line, 1));
}

void printSingleNewOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    ClassID -> %u", GET_WORD(line, 1));
}

void printSingleRefArrayOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    RefArrayIndex -> ");
    printf("%u", GET_WORD(line, 1));
}

void printDoubleRefArrayOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    LocalRefArrayIndex -> ");
    printf("%u", GET_WORD(line, 1));
    printf("\n    GlobalRefArrayIndex -> ");
    printf("%u", GET_WORD(line, 3));
}

void printExecOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    NumAttr -> ");
    printf("%u", GET_BYTE(line, 1));
}

void printPrelinkedExecOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    NumAttr -> ");
    printf("%u", GET_BYTE(line, 1));
    printf("\n    PrelinkedIndex -> ");
    printf("%u", GET_WORD(line, 2));
}

void printSpecialAssign(specialAssignment sa) {
    switch (sa) {
        case ASSIGNMENT_NONE:
            printf("No special assignment");
            break;
        case ASSIGNMENT_ADD:
            printf("Addition assignment (+=)");
            break;
        case ASSIGNMENT_SUB:
            printf("Subtraction assignment (-=)");
            break;
        case ASSIGNMENT_MUL:
            printf("Multiplication assignment (*=)");
            break;
        case ASSIGNMENT_DIV:
            printf("Division assignment (/=)");
            break;
        case ASSIGNMENT_MOD:
            printf("Modulo assignment ");
            break;
        case ASSIGNMENT_POWER:
            printf("Power assignment (^=)");
            break;
        default:
            printf("Unknown special assignment");
            break;
    }
}

void printConstOpSpecialAssign(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    Var1 -> ");
    printSpecialAssign(GET_BYTE(line, 1));
}

void printSingleOpSpecialAssign(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    Var1 -> ");
    DSPrintValue(c->constants->data[GET_BYTE(line, 1)]);
    printf(" (constant #%u)", GET_BYTE(line, 1));
    printf("\n    Var2 -> ");
    printSpecialAssign(GET_BYTE(line, 2));
}

void printSingleRefArraySpecialAssign(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    RefArrayIndex -> ");
    printf("%u", GET_WORD(line, 1));
    printf("\n    Var2 -> ");
    printSpecialAssign(GET_BYTE(line, 3));
}

void printDoubleRefArraySpecialAssign(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    printf("    LocalRefArrayIndex -> ");
    printf("%u", GET_WORD(line, 1));
    printf("\n    GlobalRefArrayIndex -> ");
    printf("%u", GET_WORD(line, 3));
    printf("\n    Var3 -> ");
    printSpecialAssign(GET_BYTE(line, 5));
}

void printJumpOp(char* name, Chunk* c, uint64_t line) {
    printf("%s\n", name);
    int16_t jumpInc = GET_WORD(line, 1);
    printf("    Line Inc[%d]", jumpInc);
}

void printInstr(uint64_t line, Chunk* c) {
    OpCode op = (uint8_t)(line & 0xFF);
    switch(op) {
        case OP_CONSTANT: printSingleOp("OP_CONSTANT", c, line); break;
        case OP_ADD: printConstOpWithPayload("OP_ADD", c, line); break;
        case OP_SUB: printConstOpWithPayload("OP_SUB", c, line); break;
        case OP_MUL: printConstOpWithPayload("OP_MUL", c, line); break;
        case OP_DIV: printConstOpWithPayload("OP_DIV", c, line); break;
        case OP_MOD: printConstOpWithPayload("OP_MOD", c, line); break;
        case OP_LESS: printConstOpWithPayload("OP_LESS", c, line); break;
        case OP_MORE: printConstOpWithPayload("OP_MORE", c, line); break;
        case OP_LESS_EQUAL: printConstOpWithPayload("OP_LESS_EQUAL", c, line); break;
        case OP_MORE_EQUAL: printConstOpWithPayload("OP_MORE_EQUAL", c, line); break;
        case OP_NEGATE: printConstOp("OP_NEGATE", c, line); break;
        case OP_NOT: printConstOp("OP_NOT", c, line); break;
        case OP_EQUAL: printConstOpWithPayload("OP_EQUAL", c, line); break;
        case OP_AND: printConstOp("OP_AND", c, line); break;
        case OP_OR: printConstOp("OP_OR", c, line); break;
        case OP_POW: printConstOpWithPayload("OP_POW", c, line); break;
        case OP_IS: printConstOp("OP_IS", c, line); break;
        case OP_GET_SELF: printConstOp("OP_GET_SELF", c, line); break;
        case OP_GET_INDEX_REF: printConstOp("OP_GET_INDEX_REF", c, line); break;
        case OP_GET_GLOBAL_REF_ATTR: printSingleRefArrayOp("OP_GET_GLOBAL_REF_ATTR", c, line); break;
        case OP_GET_LOCAL_REF_ATTR: printSingleRefArrayOp("OP_GET_LOCAL_REF_ATTR", c, line); break;
        case OP_GET_COMBINED_REF_ATTR: printDoubleRefArrayOp("OP_GET_COMBINED_REF_ATTR", c, line); break;
        case OP_GET_ATTR: printSingleOp("OP_GET_ATTR", c, line); break;
        case OP_GET_ATTR_CALL: printSingleOp("OP_GET_ATTR_CALL", c, line); break;
        case OP_SET_INDEX_REF: printConstOpSpecialAssign("OP_SET_INDEX_REF", c, line); break;
        case OP_SET_GLOBAL_REF_ATTR: printSingleRefArraySpecialAssign("OP_SET_GLOBAL_REF_ATTR", c, line); break;
        case OP_SET_LOCAL_REF_ATTR: printSingleRefArraySpecialAssign("OP_SET_LOCAL_REF_ATTR", c, line); break;
        case OP_SET_COMBINED_REF_ATTR: printDoubleRefArraySpecialAssign("OP_SET_COMBINED_REF_ATTR", c, line); break;
        case OP_SET_ATTR: printSingleOpSpecialAssign("OP_SET_ATTR", c, line); break;
        case OP_EXEC_FUNCTION_ENFORCE_RETURN: printPrelinkedExecOp("OP_EXEC_FUNCTION_ENFORCE_RETURN", c, line); break;
        case OP_EXEC_FUNCTION_IGNORE_RETURN: printPrelinkedExecOp("OP_EXEC_FUNCTION_IGNORE_RETURN", c, line); break;
        case OP_EXEC_METHOD_ENFORCE_RETURN: printExecOp("OP_EXEC_METHOD_ENFORCE_RETURN", c, line); break;
        case OP_EXEC_METHOD_IGNORE_RETURN: printExecOp("OP_EXEC_METHOD_IGNORE_RETURN", c, line); break;
        case OP_INIT: printSingleNewOp("OP_INIT", c, line); break;
        case OP_GET_PARENT_INIT: printConstOp("OP_GET_PARENT_INIT", c, line); break;
        case OP_RETURN: printConstOp("OP_RETURN", c, line); break;
        case OP_RETURN_NONE: printConstOp("OP_RETURN_NONE", c, line); break;
        case OP_JUMP: printJumpOp("OP_JUMP", c, line); break;
        case OP_JUMP_IF_FALSE: printJumpOp("OP_JUMP_IF_FALSE", c, line); break;
        default:
            runtimeError("Disassembler: Unknown opcode\n");
    }
}

void printObjArray(valueArray* array) {
    printf("valueArray: count=%d, capacity=%d\n", array->count, array->capacity);
    for (int i = 0; i < array->count; i++) {
        printf("%d -> ", i);
        DSPrintValue(array->data[i]);
        if (i < array->count - 1) {
            printf("\n");
        }
    }
}

void printChunk(Chunk* c) {
    printf("Chunk: count=%d, capacity=%d\n", c->count, c->capacity);
    for (int i=0; i<c->count; i++) {
        // Print address in hex
//        printf("%p ", &(c->code[i]));
        printf("%d: ", i);
        printInstr(c->code[i], c);
        printf("\n    (line: %u, index: %u)\n", c->lines[i], c->indices[i]);
    }
    printf("\nConstants:\n");
    printObjArray(c->constants);
    printf("\n");
}

void printToken(token* t) {
    if (t == NULL) {
        printf("Token is NULL.");
        return;
    }

    printf("Type: ");
    switch(t->type) {
        // Punctuation
        case LEFT_PARENTHESES: printf("LEFT_PARENTHESES"); break;
        case RIGHT_PARENTHESES: printf("RIGHT_PARENTHESES"); break;
        case LEFT_BRACKET: printf("LEFT_BRACKET"); break;
        case RIGHT_BRACKET: printf("RIGHT_BRACKET"); break;
        case LEFT_BRACE: printf("LEFT_BRACE"); break;
        case RIGHT_BRACE: printf("RIGHT_BRACE"); break;
        case COMMA: printf("COMMA"); break;
        case DOT: printf("DOT"); break;
        case SEMICOLON: printf("SEMICOLON"); break;

            // Arithmetic operators
        case PLUS: printf("PLUS"); break;
        case PLUS_EQUAL: printf("PLUS_EQUAL"); break;
        case MINUS: printf("MINUS"); break;
        case MINUS_EQUAL: printf("MINUS_EQUAL"); break;
        case MULTIPLY: printf("MULTIPLY"); break;
        case MULTIPLY_EQUAL: printf("MULTIPLY_EQUAL"); break;
        case DIVIDE: printf("DIVIDE"); break;
        case DIVIDE_EQUAL: printf("DIVIDE_EQUAL"); break;
        case MOD: printf("MOD"); break;
        case MOD_EQUAL: printf("MOD_EQUAL"); break;
        case EQUAL: printf("EQUAL"); break;
        case DOUBLE_EQUAL: printf("DOUBLE_EQUAL"); break;

            // Logical operators
        case DOUBLE_AND: printf("DOUBLE_AND"); break;
        case DOUBLE_OR: printf("DOUBLE_OR"); break;
        case NOT: printf("NOT"); break;

            // Other operators
        case COLON: printf("COLON"); break;
        case CARET: printf("CARET"); break;
        case CARET_EQUAL: printf("CARET_EQUAL"); break; // Added
        case DICT_PREFIX: printf("DICT_PREFIX"); break; // Added
        case SET_PREFIX: printf("SET_PREFIX"); break; // Added

            // Comparison operators
        case LESS: printf("LESS"); break;
        case LESS_EQUAL: printf("LESS_EQUAL"); break;
        case MORE: printf("MORE"); break;
        case MORE_EQUAL: printf("MORE_EQUAL"); break;

            // Data types
        case STRING: printf("STRING"); break;
        case NUMBER: printf("NUMBER"); break;

            // Keywords
        case KEYWORD_IF: printf("KEYWORD_IF"); break;
        case KEYWORD_ELIF: printf("KEYWORD_ELIF"); break;
        case KEYWORD_ELSE: printf("KEYWORD_ELSE"); break;
        case KEYWORD_WHILE: printf("KEYWORD_WHILE"); break;
        case KEYWORD_FOR: printf("KEYWORD_FOR"); break;
        case KEYWORD_BREAK: printf("KEYWORD_BREAK"); break;
        case KEYWORD_CONTINUE: printf("KEYWORD_CONTINUE"); break;
        case KEYWORD_RETURN: printf("KEYWORD_RETURN"); break;
        case KEYWORD_VOID: printf("KEYWORD_VOID"); break;
        case KEYWORD_TRUE: printf("KEYWORD_TRUE"); break;
        case KEYWORD_FALSE: printf("KEYWORD_FALSE"); break;
        case KEYWORD_IS: printf("KEYWORD_IS"); break;
        case KEYWORD_NONE: printf("KEYWORD_NONE"); break;
        case KEYWORD_CLASS: printf("KEYWORD_CLASS"); break;
        case KEYWORD_FUNCTION: printf("KEYWORD_FUNCTION"); break;
        case KEYWORD_SELF: printf("KEYWORD_SELF"); break;
        case KEYWORD_NEW: printf("KEYWORD_NEW"); break;
        case KEYWORD_PARENT_INIT: printf("KEYWORD_PARENT_INIT"); break;
        case KEYWORD_INIT: printf("KEYWORD_INIT"); break;
        case KEYWORD_GLOBAL: printf("KEYWORD_GLOBAL"); break;
        case KEYWORD_INCLUDE: printf("KEYWORD_INCLUDE"); break;

        // Identifiers
        case IDENTIFIER: printf("IDENTIFIER"); break;

        default: printf("UNKNOWN TOKEN"); break;
    }
    if (t->value != NULL) printf(", Value: \"%s\"", t->value);
    printf(", Source #%u", t->sourceIndex);

    printf(", [%u, %u]", t->line, t->index);
}
