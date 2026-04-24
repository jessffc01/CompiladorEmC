#include <stdio.h>
#include <stdlib.h>
#include "lex.h"

FILE *arquivo;
Token currentToken;
// 🔹 Avança para o próximo token
void advance() {
    currentToken = getNextToken(arquivo);
    // ignora comentários
    while (currentToken.tipo == COMENTARIO) {
        currentToken = getNextToken(arquivo);
    }
}
// 🔹 Valida token esperado
void match(TokenTipo esperado) {
    if (currentToken.tipo == esperado) {
        advance();
    } else {
        printf("Erro sintático! Esperado: %d, Recebido: %d\n",
               esperado, currentToken.tipo);
        exit(1);
    }
}
//void args();

void class(){
    if (currentToken.tipo == CLASS){
        match(CLASS);
        match(IDENTIFICADOR);

        if(currentToken.tipo == INHERITS){
            match(INHERITS);
            match(IDENTIFICADOR);
        }

        match(LBRACE);
        if(currentToken.tipo == IDENTIFICADOR){
            feature();
        }
        match(RBRACE);
    }
}

void feature(){
    match(IDENTIFICADOR);
    if(currentToken.tipo == LPAREN){
        match(LPAREN);
        if(currentToken.tipo == IDENTIFICADOR){
            match(IDENTIFICADOR);
        }
    }
}

void expr() {
    // 🔸 IF
    if (currentToken.tipo == IF) {
        match(IF);
        expr();
        match(THEN);
        expr();
        match(ELSE);
        expr();
        match(FI);
    }

    // 🔸 WHILE
    else if (currentToken.tipo == WHILE) {
        match(WHILE);
        expr();
        match(LOOP);
        expr();
        match(POOL);
    }

    // 🔸 LET
    else if (currentToken.tipo == LET) {
        match(LET);

        match(IDENTIFICADOR);
        match(COLON);
        match(IDENTIFICADOR); // tipo simplificado

        if (currentToken.tipo == ATRIBUI) {
            match(ATRIBUI);
            expr();
        }

        match(IN);
        expr();
    }

    // 🔸 NEW
    else if (currentToken.tipo == NEW) {
        match(NEW);
        match(IDENTIFICADOR);
    }

    // 🔸 NOT
    else if (currentToken.tipo == NOT) {
        match(NOT);
        expr();
    }

    // 🔸 IDENTIFICADOR → variável ou chamada
    else if (currentToken.tipo == IDENTIFICADOR) {
        match(IDENTIFICADOR);

        // chamada de método
        if (currentToken.tipo == LPAREN) {
            match(LPAREN);
            args();
            match(RPAREN);
        }
    }

    // 🔸 número
    else if (currentToken.tipo == NUMERO) {
        match(NUMERO);
    }

    // 🔸 string
    else if (currentToken.tipo == STRING) {
        match(STRING);
    }

    // 🔸 TRUE / FALSE
    else if (currentToken.tipo == TRUE) {
        match(TRUE);
    }
    else if (currentToken.tipo == FALSE) {
        match(FALSE);
    }

    // 🔸 parênteses
    else if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        expr();
        match(RPAREN);
    }

    else {
        printf("Erro: expressão inválida\n");
        exit(1);
    }
}

void args() {

    if (currentToken.tipo == RPAREN) {
        return; // vazio
    }

    expr();

    while (currentToken.tipo == COMMA) {
        match(COMMA);
        expr();
    }
}
/*void factor();
void term();
void expr_arit();
void factor() {
    if (currentToken.tipo == NUMERO) {
        match(NUMERO);
    }
    else if (currentToken.tipo == IDENTIFICADOR) {
        match(IDENTIFICADOR);
    }
    else if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        expr_arit();
        match(RPAREN);
    }
    else {
        printf("Erro em factor\n");
        exit(1);
    }
}
    void term() {
    factor();

    while (currentToken.tipo == MULTIPLICACAO ||
           currentToken.tipo == DIVISAO) {

        if (currentToken.tipo == MULTIPLICACAO)
            match(MULTIPLICACAO);
        else
            match(DIVISAO);

        factor();
    }
}
    void expr_arit() {
    term();

    while (currentToken.tipo == MAIS ||
           currentToken.tipo == MENOS) {

        if (currentToken.tipo == MAIS)
            match(MAIS);
        else
            match(MENOS);

        term();
    }
}
*/
int main() {
    arquivo = fopen("arquivo.cl", "r");
    if (!arquivo) {
        printf("Erro ao abrir arquivo\n");
        return 1;
    }
    advance(); // primeiro token
    expr();    // começa pelo nível mais alto
    if (currentToken.tipo == EOF_TOKEN) {
        printf("Parsing realizado com sucesso!\n");
    } else {
        printf("Erro: tokens restantes\n");
    }
    fclose(arquivo);
    return 0;
}