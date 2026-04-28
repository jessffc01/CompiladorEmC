#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"

#define MAX_ERRORS 100

FILE *arquivo;
Token currentToken;

Token nextToken;
int looked = 0;

char* errors[MAX_ERRORS];
int errorCount = 0;

// ================= ERRO =================
void reportError(const char* msg, int linha, int coluna) {
    if (errorCount < MAX_ERRORS) {
        char buffer[200];
        snprintf(buffer, sizeof(buffer),
                 "Erro na linha %d, coluna %d: %s",
                 linha, coluna, msg);
        errors[errorCount++] = strdup(buffer);
    }
}

// ================= CONTROLE =================
Token look() {
    if (!looked) {
        nextToken = getNextToken(arquivo);
        looked = 1;
        while (currentToken.tipo == COMENTARIO) {
            currentToken = getNextToken(arquivo);
        }
    }
    return nextToken;
}

void advance() {
    if (looked) {
        currentToken = nextToken;
        looked = 0;
    }
    else{
        currentToken = getNextToken(arquivo);
    }

    while (currentToken.tipo == COMENTARIO) {
        currentToken = getNextToken(arquivo);
    }
}

void synchronize() {
    while (currentToken.tipo != SEMICOLON &&
           currentToken.tipo != RBRACE &&
           currentToken.tipo != EOF_TOKEN) {
        advance();
    }

    if (currentToken.tipo == SEMICOLON) {
        advance();
    }
}

void match(TokenTipo esperado) {
    if (currentToken.tipo == esperado) {
        advance();
    } else {
        reportError("Token inesperado",
                    currentToken.location.linha,
                    currentToken.location.coluna);
        synchronize();
    }
}

// ================= PROTÓTIPOS =================
void program();
void class();
void feature();
void formal();
void expr();
void args();

// EXPRESSÕES
void expr_atrib();
void expr_not();
void expr_rel();
void expr_arit();
void term();
void factor();
void dispatch();
void primary();

// ================= PROGRAMA =================
void program() {
    while (currentToken.tipo == CLASS) {
        class();
    }
}

// ================= CLASS =================
void class() {
    match(CLASS);
    match(IDENTIFICADOR);

    if (currentToken.tipo == INHERITS) {
        match(INHERITS);
        match(IDENTIFICADOR);
    }

    match(LBRACE);

    while (currentToken.tipo == IDENTIFICADOR) {
        feature();
    }

    match(RBRACE);
    match(SEMICOLON);
}

// ================= FORMAL =================
void formal() {
    match(IDENTIFICADOR);
    match(COLON);
    match(IDENTIFICADOR);
}

// ================= FEATURE =================
void feature() {
    match(IDENTIFICADOR);

    if (currentToken.tipo == LPAREN) {
        // método
        match(LPAREN);

        if (currentToken.tipo == IDENTIFICADOR) {
            formal();
            while (currentToken.tipo == COMMA) {
                match(COMMA);
                formal();
            }
        }

        match(RPAREN);
        match(COLON);
        match(IDENTIFICADOR);

        match(LBRACE);
        expr();
        match(RBRACE);
    } else {
        // atributo
        match(COLON);
        match(IDENTIFICADOR);

        if (currentToken.tipo == ATRIBUI) {
            match(ATRIBUI);
            expr();
        }
    }

    match(SEMICOLON);
}

// ================= EXPRESSÕES =================
void expr() {

    // IF
    if (currentToken.tipo == IF) {
        match(IF);
        expr();
        match(THEN);
        expr();
        match(ELSE);
        expr();
        match(FI);
    }

    // WHILE
    else if (currentToken.tipo == WHILE) {
        match(WHILE);
        expr();
        match(LOOP);
        expr();
        match(POOL);
    }

    // LET
    else if(currentToken.tipo == LET){
        match(LET);
        match(IDENTIFICADOR);
        match(COLON);
        match(IDENTIFICADOR);

        if (currentToken.tipo == ATRIBUI) {
            match(ATRIBUI);
            expr();
        }

        while (currentToken.tipo == COMMA) {
            match(COMMA);
            match(IDENTIFICADOR);
            match(COLON);
            match(IDENTIFICADOR);

            if (currentToken.tipo == ATRIBUI) {
                match(ATRIBUI);
                expr();
            }
        }
        match(IN);
        expr();
    }

    // CASE
    else if (currentToken.tipo == CASE) {
        match(CASE);
        expr();
        match(OF);

        do{
            match(IDENTIFICADOR);
            match(COLON);
            match(IDENTIFICADOR);
            match(AVALIA);
            expr();
            match(SEMICOLON);
        }while (currentToken.tipo == IDENTIFICADOR);

        match(ESAC);
    }

    // BLOCO
    else if (currentToken.tipo == LBRACE) {
        match(LBRACE);

        do{
            expr();
            match(SEMICOLON);
        } while (currentToken.tipo != RBRACE && currentToken.tipo != EOF_TOKEN);

        match(RBRACE);
    }

    // NEW
    else if (currentToken.tipo == NEW) {
        match(NEW);
        match(IDENTIFICADOR);
    }

    else {
        expr_atrib(); //Começa a trabalhar com operadores
    }
}

//================= ATRIBUIÇÃO =================
void expr_atrib(){
    if (currentToken.tipo == IDENTIFICADOR && look().tipo == ATRIBUI) {
        match(IDENTIFICADOR);
        match(ATRIBUI);
        expr_atrib();
        return;
    }
    expr_not();
}

//================= NOT =================
void expr_not(){
    while(currentToken.tipo == NOT){
        match(NOT);
    }
    expr_rel();
}

// ================= RELACIONAL =================
void expr_rel() {
    expr_arit();

    while (currentToken.tipo == MENOR ||
        currentToken.tipo == MENOROUIGUAL ||
        currentToken.tipo == MAIOR ||
        currentToken.tipo == MAIOROUIGUAL ||
        currentToken.tipo == IGUAL) {

        advance();
        expr_arit();
    }
}

// ================= ARITMÉTICA =================
void expr_arit() {
    term();

    while (currentToken.tipo == MAIS ||
           currentToken.tipo == MENOS) {

        advance();
        term();
    }
}

void term() {
    factor();

    while (currentToken.tipo == MULTIPLICACAO ||
           currentToken.tipo == DIVISAO) {

        advance();
        factor();
    }
}

void factor() {
    if(currentToken.tipo == ISVOID) {
        match(ISVOID);
        factor();
    }
    else if(currentToken.tipo == COMPLEMENTO){
        match(COMPLEMENTO);
        factor();
    }
    else{
        dispatch();
    }
}

void dispatch(){
    primary();

    if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        args();
        match(RPAREN);
    }

    while(currentToken.tipo == DOT || currentToken.tipo == ARROBA) {
        if (currentToken.tipo == ARROBA) {
            match(ARROBA);
            match(IDENTIFICADOR);
        }
        match(DOT);
        match(IDENTIFICADOR);
        match(LPAREN);
        args();
        match(RPAREN);
    }
}

void primary(){
    if (currentToken.tipo == NUMERO) {
        match(NUMERO);
    }
    else if (currentToken.tipo == IDENTIFICADOR) {
        match(IDENTIFICADOR);
    }
    else if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        expr();
        match(RPAREN);
    }
    else if (currentToken.tipo == STRING) {
        match(STRING);
    }
    else if (currentToken.tipo == TRUE) {
        match(TRUE);
    }
    else if (currentToken.tipo == FALSE) {
        match(FALSE);
    }
    else {
        reportError("Fator inválido",
                    currentToken.location.linha,
                    currentToken.location.coluna);
        synchronize();
    }
}

// ================= ARGUMENTOS =================
void args() {
    if (currentToken.tipo == RPAREN) return;

    expr();

    while (currentToken.tipo == COMMA) {
        match(COMMA);
        expr();
    }
}

// ================= MAIN =================
int main() {
    arquivo = fopen("arquivo.cl", "r");

    if (!arquivo) {
        printf("Erro ao abrir arquivo\n");
        return 1;
    }

    advance();
    program();

    if (errorCount > 0 || currentToken.tipo != EOF_TOKEN) {
        for (int i = 0; i < errorCount; i++) {
            printf("%s\n", errors[i]);
            free(errors[i]);
        }
        if(currentToken.tipo != EOF_TOKEN){
            printf("Erro: tokens restantes\n");
        }
    }
    else{
        printf("Parsing realizado com sucesso!\n");
    }

    fclose(arquivo);
    return 0;
}