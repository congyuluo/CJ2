//
// Created by congyu on 7/28/23.
//

#ifndef CJ_2_TOKENIZER_H
#define CJ_2_TOKENIZER_H

#include <stdbool.h>

#include "refManager.h"

#define TOKEN_TYPE(t) t->type
#define TOKEN_VALUE(t) t->value

typedef enum tokenType {
    // Punctuation
    LEFT_PARENTHESES,
    RIGHT_PARENTHESES,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMMA,
    DOT,
    SEMICOLON,

    // Arithmetic operators
    PLUS,
    PLUS_EQUAL,
    MINUS,
    MINUS_EQUAL,
    MULTIPLY,
    MULTIPLY_EQUAL,
    DIVIDE,
    DIVIDE_EQUAL,
    MOD, // Modulo operation
    MOD_EQUAL, // Modulo assignment operation
    EQUAL, // Equal to
    DOUBLE_EQUAL, // Double equal to (==)

    // Logical operators
    DOUBLE_AND, // Double AND (&&)
    DOUBLE_OR, // Double OR (||)
    NOT, // Not operation

    // Other operators
    COLON, // Colon
    CARET, // Caret (power operation or bitwise XOR, depending on your language)
    CARET_EQUAL, // Caret assignment
    DICT_PREFIX, // d{
    SET_PREFIX, // s{

    // Comparison operators
    LESS, // Less than
    LESS_EQUAL, // Less than or equal to
    MORE, // More than
    MORE_EQUAL, // More than or equal to

    // Data types
    STRING,
    NUMBER,

    // Keywords
    KEYWORD_IF,
    KEYWORD_ELIF,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_FOR,
    KEYWORD_BREAK,
    KEYWORD_CONTINUE,
    KEYWORD_RETURN,
    KEYWORD_VOID,
    KEYWORD_TRUE,
    KEYWORD_FALSE,
    KEYWORD_IS,
    KEYWORD_NONE,
    KEYWORD_CLASS,
    KEYWORD_FUNCTION,
    KEYWORD_SELF,
    KEYWORD_NEW,
    KEYWORD_PARENT_INIT,
    KEYWORD_INIT,
    KEYWORD_GLOBAL,
    KEYWORD_INCLUDE,

    // Identifier
    IDENTIFIER
} tokenType;

extern const char *keywords[];
extern tokenType keywordTypes[];

typedef struct token token;

struct token {
    tokenType type;
    char* value;
    unsigned int line;
    unsigned int index;
    unsigned int sourceIndex;
    token* prevToken;
    token* nextToken;
};

typedef struct tokenizer {
    char* sourceStack[INCLUDE_STACK_SIZE];
    uint32_t sourceStackCount;
    uint32_t totalSourceCount;
    char* currChar;
    unsigned int currLine;
    unsigned int currIndex;
    token* currToken;
    token* startToken;
    refTable* sourceTable;
} tokenizer;

char* loadFile(const char* sourcePath);

void initTokenizer(char* sourcefile, char* sourceName);
void freeTokenizer();

token* createToken(tokenType type, char* value, unsigned int line, unsigned int index);
void freeToken(token* t);

token* nextToken();
refTable* tokenize();

bool isAssignmentOperator(tokenType ty);

bool isAssignmentStatement();



#endif //CJ_2_TOKENIZER_H
