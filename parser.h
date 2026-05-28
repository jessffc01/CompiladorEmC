#ifndef PARSER_H
#define PARSER_H

#include <math.h>
#include "ast.h"

// ================= PROTÓTIPOS =================
// ================= ERRO =================
void reportError(const char* msg, int linha, int coluna);
// ================= CONTROLE =================
Token look();
void advance();
void synchronize();
void match(TokenTipo esperado);
// ================= ESTRUTURAS PRINCIPAIS =================
ASTNode* program();
ASTNode* class();    // Lê uma classe (ou lista de classes)
ASTNode* feature();  // Lê métodos ou atributos
ASTNode* formal();   // Lê os parâmetros de um método
ASTNode* expr();     // O nosso grande trator principal
ASTNode* args();     // Lê as listas separadas por vírgula

// ================= EXPRESSÕES (A Escada Matemática) =================
ASTNode* expr_atrib();
ASTNode* expr_not();
ASTNode* expr_rel();
ASTNode* expr_arit();
ASTNode* term();
ASTNode* factor();
ASTNode* dispatch();
ASTNode* primary();

#endif