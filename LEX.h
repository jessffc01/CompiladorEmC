#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#ifndef LEX_H
#define LEX_H

extern int linha;
extern int coluna;

typedef enum {
    NUMERO,
    IDENTIFICADOR,
    MAIS,
    MENOS,
    DIVISAO,
    MULTIPLICACAO,
    MENOROUIGUAL,
    MAIOROUIGUAL,
    MAIOR,
    MENOR,
    IGUAL,
    ATRIBUI,
    CLASS,
    INHERITS,
    IF,
    THEN,
    ELSE,
    FI,
    WHILE,
    LOOP,
    POOL,
    LET,
    IN,
    CASE,
    OF,
    ESAC,
    NEW,
    ISVOID,
    NOT,
    TRUE,
    FALSE,
    LBRACE,
    RBRACE,
    LPAREN, 
    RPAREN, 
    SEMICOLON,
    COLON, 
    DOT, 
    COMMA, 
    ARROBA,
    STRING,
    EOF_TOKEN,
    COMENTARIO
} TokenTipo;

typedef struct {
    int linha;
    int coluna;
} TokenLocation;

extern const char* strTipos[];

typedef struct {
    char *palavra;
    TokenTipo tipo;
} Keyword;

extern Keyword keywords[];

typedef struct {
    TokenTipo tipo;
    char lexema[100];
    TokenLocation location;
} Token;

typedef struct TokenList {
    Token token;
    struct TokenList* next;
} TokenList;

TokenTipo verificaPalavraReservada(char *lexema);

Token getNextToken(FILE *arquivo);

TokenList* new_item(Token t);

TokenList* push_back(TokenList* head, Token t);

void free_list(TokenList* head);

#endif