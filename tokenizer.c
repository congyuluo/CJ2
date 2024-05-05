//
// Created by congyu on 7/28/23.
//

#include "tokenizer.h"
#include "errors.h"
#include "common.h"
#include "debug.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_UPPER_ALPHA(c) ((c) >= 'A' && (c) <= 'Z')
#define IS_LOWER_ALPHA(c) ((c) >= 'a' && (c) <= 'z')
#define IS_ALPHA(c) (IS_UPPER_ALPHA(c) || IS_LOWER_ALPHA(c) || (c) == '_')

#define CHECK_ASSIGN(nextChar, tokenTypeIfNext, tokenTypeIfSingle) { \
    if (NEXT_CHAR == (nextChar)) { \
        INC_CHAR(); \
        t = CREATE_TOKEN(tokenTypeIfNext, NULL); \
    } else { \
        t = CREATE_TOKEN(tokenTypeIfSingle, NULL); \
    } \
}

#define CREATE_TOKEN(type, value) createToken(type, value, Tokenizer->currLine, Tokenizer->currIndex)
#define CURR_CHAR (*Tokenizer->currChar)
#define NEXT_CHAR *(Tokenizer->currChar + 1)
#define INC_LINE() Tokenizer->currLine++
#define INC_CHAR() do { \
  Tokenizer->currChar++; \
  Tokenizer->currIndex++; \
} while(0)

// List of keywords
const char *keywords[] = {
        "if",
        "elif",
        "else",
        "while",
        "for",
        "break",
        "continue",
        "return",
        "void",
        "true",
        "false",
        "is",
        "none",
        "class",
        "function",
        "self",
        "new",
        "pInit",
        "init",
        "global",
        "or",
        "and",
        "not",
        "include",
};

// List of keyword token types
tokenType keywordTypes[] = {
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
        DOUBLE_OR,
        DOUBLE_AND,
        NOT,
        KEYWORD_INCLUDE,
};

tokenizer* Tokenizer;

token* checkAssignRaiseError(char nextChar, tokenType tokenTypeIfNext) {
    if (NEXT_CHAR == nextChar) {
        INC_CHAR();
        token* t = CREATE_TOKEN(tokenTypeIfNext, NULL);
        INC_CHAR();
        return t;
    } else { \
        parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Invalid expression");
        return NULL;
    }
}

char* loadFile(const char* sourcePath) {
    FILE* file = fopen(sourcePath, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", sourcePath);
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // Allocate memory for the entire file
    char* buffer = (char*) malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", sourcePath);
        return NULL;
    }

    // Read the file into the buffer
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", sourcePath);
        return NULL;
    }

    // Null-terminate the buffer
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

void initTokenizer(char* sourcefile, char* sourceName) {
    Tokenizer = (tokenizer*)malloc(sizeof(tokenizer));
    Tokenizer->sourceStack[0] = sourcefile;
    Tokenizer->sourceStackCount = 1;
    Tokenizer->totalSourceCount = 0;
    Tokenizer->currToken = NULL;
    Tokenizer->startToken = NULL;
    Tokenizer->sourceTable = createRefTable(GLOBAL_REF_TABLE_INIT_SIZE);
    getRefIndex(Tokenizer->sourceTable, sourceName);
}

void freeTokenizer() {
    token* t = Tokenizer->startToken;
    while (t != NULL) {
        token* next = t->nextToken;
        freeToken(t);
        t = next;
    }
    free(Tokenizer);
}

token* createToken(tokenType type, char* value, unsigned int line, unsigned int index) {
    token* t = (token*)malloc(sizeof(token));
    t->type = type;
    t->value = value;
    t->line = line;
    t->index = index;
    t->sourceIndex = Tokenizer->totalSourceCount - 1;
    t->prevToken = NULL;
    t->nextToken = NULL;

    if (Tokenizer->startToken == NULL) {
        Tokenizer->startToken = t;
    } else {
        token* lastToken = Tokenizer->startToken;
        while (lastToken->nextToken != NULL) {
            lastToken = lastToken->nextToken;
        }
        lastToken->nextToken = t;
        t->prevToken = lastToken;
    }
    return t;
}

void freeToken(token* t) {
    if (t->value != NULL) free(t->value);
    free(t);
}

token* nextNumberToken() { // Called when CURR_CHAR location is at first digit
    token* t = NULL;
    unsigned int startingIndex = Tokenizer->currIndex;
    char* startingChar = Tokenizer->currChar;
    // Handle negative numbers
    if (CURR_CHAR == '-') INC_CHAR();
    // Accept only one decimal point
    bool hasDecimal = false;
    while (IS_DIGIT(CURR_CHAR) || CURR_CHAR == '.') {
        if (CURR_CHAR == '.') {
            // Raise error if we already have a decimal
            if (hasDecimal) parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Invalid number");
            // If the next character is not a digit, break and return the number before the decimal
            if (!IS_DIGIT(NEXT_CHAR)) break;
            // Otherwise, we have a decimal
            hasDecimal = true;
        }
        INC_CHAR();
    }
    // Copy number to length
    unsigned int length = Tokenizer->currIndex - startingIndex;
    char* str = (char*)malloc(sizeof(char) * (length + 1));
    for (int i = 0; i < length; i++) str[i] = *(startingChar + i);
    str[length] = '\0';
    t = createToken(NUMBER, str, Tokenizer->currLine, startingIndex);
    return t;
}

token* nextIdentifierToken() { // Called when CURR_CHAR location is at first letter
    char buf[IDENTIFIER_BUFFER_SIZE]; // Buffer for the identifier
    int idx = 0;
    bool isIdentifier = false;
    unsigned int startingIndex = Tokenizer->currIndex;

    // Collect characters until we hit a non-alphabetic character
    while ((IS_ALPHA(CURR_CHAR) || IS_DIGIT(CURR_CHAR) || CURR_CHAR == '_') && idx < sizeof(buf) - 1) {
        // Check if we have an identifier ahead of comparing string
        if (IS_DIGIT(CURR_CHAR) || CURR_CHAR == '_') isIdentifier = true;
        buf[idx++] = CURR_CHAR;
        INC_CHAR(); // increment to next char
    }
    buf[idx] = '\0'; // null terminate the string
    // If already an identifier, return identifier token
    if (isIdentifier) return createToken(IDENTIFIER, strdup(buf), Tokenizer->currLine, startingIndex);
    // Check if the string matches a keyword
    for (int i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++) {
        if (strcmp(buf, keywords[i]) == 0) {
            // If it's a keyword, return a token of the keyword's type
            return createToken(keywordTypes[i], NULL, Tokenizer->currLine, startingIndex);
        }
    }
    // Determined as an identifier
    return createToken(IDENTIFIER, strdup(buf), Tokenizer->currLine, startingIndex);
}

token* nextStringToken() { // Called when CURR_CHAR location is at first string character
    token* t = NULL;
    unsigned int startingIndex = Tokenizer->currIndex;
    char* startingChar = Tokenizer->currChar;
    unsigned int specialCharCount = 0;
    while (CURR_CHAR != '\0' && CURR_CHAR != '\n' && CURR_CHAR != '"') {
        // Handle escape characters
        if (CURR_CHAR == '\\') {
            INC_CHAR();
            if (!(CURR_CHAR == 'n' || CURR_CHAR == 't' || CURR_CHAR == '"')) parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Invalid escape character");
            specialCharCount++;
        }
        INC_CHAR();
    }
    if (CURR_CHAR !=  '"') parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "String not closed");
    // Copy string to length
    unsigned int length = (Tokenizer->currIndex - startingIndex) - specialCharCount;
    char* str = (char*)malloc(sizeof(char) * (length + 1));
    char* sourcePtr = startingChar;
    for (int i = 0; i < length; i++) {
        // Handle escape characters
        if (*sourcePtr == '\\') {
            sourcePtr++;
            switch (*sourcePtr) {
                case 'n':
                    str[i] = '\n';
                    break;
                case 't':
                    str[i] = '\t';
                    break;
                case '"':
                    str[i] = '"';
                    break;
            }
        } else {
            str[i] = *sourcePtr;
        }
        sourcePtr++;
    }
    str[length] = '\0';
    t = createToken(STRING, str, Tokenizer->currLine, startingIndex);
    return t;
}

void skipWhiteSpace() {
    while (CURR_CHAR == ' ' || CURR_CHAR == '\t' || CURR_CHAR == '\n') {
        if (CURR_CHAR == '\n') {
            INC_LINE();
            Tokenizer->currChar++;
            Tokenizer->currIndex = 0;
        } else {
            INC_CHAR();
        }
    }
}

token* tokenizeNext() {
    token* t = NULL;
    // Skip whitespace
    skipWhiteSpace();
    // Skip comments
    if (CURR_CHAR == '#') {
        while (CURR_CHAR != '\n' && CURR_CHAR != '\0') INC_CHAR();
        return tokenizeNext();
    }
    // Match set/dict prefix
    if ((CURR_CHAR == 'd' || CURR_CHAR == 's') && NEXT_CHAR == '{') {
        tokenType type = (CURR_CHAR == 'd' ? DICT_PREFIX : SET_PREFIX);
        INC_CHAR();
        INC_CHAR();
        return CREATE_TOKEN(type, NULL);
    }
    // Match number
    if (IS_DIGIT(CURR_CHAR)) return nextNumberToken();
    // Match identifier
    if (IS_ALPHA(CURR_CHAR)) return nextIdentifierToken();
    // Other tokens
    switch (CURR_CHAR) {
        case '"': {
            INC_CHAR();
            t = nextStringToken();
            INC_CHAR();
            return t;
        }
        case '(': t = CREATE_TOKEN(LEFT_PARENTHESES, NULL); break;
        case ')': t = CREATE_TOKEN(RIGHT_PARENTHESES, NULL); break;
        case '[': t = CREATE_TOKEN(LEFT_BRACKET, NULL); break;
        case ']': t = CREATE_TOKEN(RIGHT_BRACKET, NULL); break;
        case '{': t = CREATE_TOKEN(LEFT_BRACE, NULL); break;
        case '}': t = CREATE_TOKEN(RIGHT_BRACE, NULL); break;
        case ',': t = CREATE_TOKEN(COMMA, NULL); break;
        case '.': t = CREATE_TOKEN(DOT, NULL); break;
        case ';': t = CREATE_TOKEN(SEMICOLON, NULL); break;
        case '+': CHECK_ASSIGN('=', PLUS_EQUAL, PLUS) break;
        case '-': {
            if (IS_DIGIT(NEXT_CHAR)) {
                return nextNumberToken();
            } else {
                CHECK_ASSIGN('=', MINUS_EQUAL, MINUS)
            }
            break;
            }

        case '*': CHECK_ASSIGN('=', MULTIPLY_EQUAL, MULTIPLY) break;
        case '/': CHECK_ASSIGN('=', DIVIDE_EQUAL, DIVIDE) break;
        case '%': CHECK_ASSIGN('=', MOD_EQUAL, MOD) break;
        case '&': return checkAssignRaiseError('&', DOUBLE_AND);
        case '|': return checkAssignRaiseError('|', DOUBLE_OR);
        case '!': t = CREATE_TOKEN(NOT, NULL); break;
        case ':': t = CREATE_TOKEN(COLON, NULL); break;
        case '^': CHECK_ASSIGN('=', CARET_EQUAL, CARET) break;
        case '<': CHECK_ASSIGN('=', LESS_EQUAL, LESS) break;
        case '>': CHECK_ASSIGN('=', MORE_EQUAL, MORE) break;
        case '=': CHECK_ASSIGN('=', DOUBLE_EQUAL, EQUAL) break;
        case '\0': return NULL;

        default: parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Unhandled current character");
    }

    INC_CHAR();
    return t;
}

token* nextToken() {
    token* result = Tokenizer->currToken;
    // Increment to next token
    if (result != NULL) Tokenizer->currToken = Tokenizer->currToken->nextToken;
#ifdef DEBUG_PRINT_TOKENS
    printf("PrevToken: ");
    if (result == NULL) {
        printf("Token is NULL");
    } else {
        printToken(result->prevToken);
    }
    printf(", CurrToken: ");
    printToken(result);
    printf("\n");
#endif
    return result;
}

refTable* tokenize() {
    refTable* globalDeclTable = createRefTable(GLOBAL_REF_TABLE_INIT_SIZE);
    // Continue tokenizing until we reach the end of the source stack
    while (Tokenizer->sourceStackCount > 0) {
        // Setup source
        Tokenizer->currChar = Tokenizer->sourceStack[--Tokenizer->sourceStackCount];
        Tokenizer->currLine = 0;
        Tokenizer->currIndex = 0;
        Tokenizer->totalSourceCount++;
        // Tokenize
        token* temp = tokenizeNext();
        while (temp != NULL) {
            token* prev = temp;
            temp = tokenizeNext();
            if (TOKEN_TYPE(prev) == KEYWORD_GLOBAL) {
                if (TOKEN_TYPE(temp) != IDENTIFIER) parsingError(prev->line, prev->index, Tokenizer->totalSourceCount-1, "Invalid global statement");
                // Add to global declaration table
                getRefIndex(globalDeclTable, temp->value);
            } else if (TOKEN_TYPE(prev) == KEYWORD_INCLUDE) {
                if (TOKEN_TYPE(temp) != IDENTIFIER) parsingError(prev->line, prev->index, Tokenizer->totalSourceCount-1, "Expected identifier after include");
                // Add to open source and add to source list
                if (!refTableContains(Tokenizer->sourceTable, TOKEN_VALUE(temp))) {
                    char* newSource = loadFile(TOKEN_VALUE(temp));
                    if (newSource == NULL) parsingError(prev->line, prev->index, Tokenizer->totalSourceCount-1, "Could not load file");
                    Tokenizer->sourceStack[Tokenizer->sourceStackCount++] = newSource;
                    // Attach to error handler
                    attachSource(newSource, TOKEN_VALUE(temp));
                    // Add to source table
                    getRefIndex(Tokenizer->sourceTable, TOKEN_VALUE(temp));
                }
            }
        }
    }
    // Free source table
    freeRefTable(Tokenizer->sourceTable);
    // Set init token
    Tokenizer->currToken = Tokenizer->startToken;
    return globalDeclTable;
}

bool isAssignmentOperator(tokenType ty) {
    switch (ty) {
        case PLUS_EQUAL:
        case MINUS_EQUAL:
        case MULTIPLY_EQUAL:
        case DIVIDE_EQUAL:
        case MOD_EQUAL:
        case CARET_EQUAL:
        case EQUAL:
            return true;
        default:
            return false;
    }
}

bool isAssignmentStatement() {
    // Check for invalid tokenizer state
    if (Tokenizer->currToken == NULL) parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Uninitialized tokenizer");
    if (Tokenizer->currToken->prevToken == NULL) parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "No previous token");
    // Find the first token of the statement
    token* t = Tokenizer->currToken->prevToken;
    // Check if the first token is assignment operator
    if (isAssignmentOperator(TOKEN_TYPE(t))) parsingError(t->line, t->index, Tokenizer->totalSourceCount-1, "No left hand side of assignment");
    // Initial increment
    t = t->nextToken;
    while (t != NULL && TOKEN_TYPE(t) != SEMICOLON) {
        // Check if the current token is assignment operator
        if (isAssignmentOperator(TOKEN_TYPE(t))) return true;
        // Increment
        t = t->nextToken;
    }
    if (t == NULL) parsingError(Tokenizer->currLine, Tokenizer->currIndex, Tokenizer->totalSourceCount-1, "Unfinished statement");
    return false;
}
