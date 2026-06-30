#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

/*
 * Gera o programa Bril completo a partir da raiz da AST.
 *
 * Pré-condição: checkProgram(raiz) já deve ter sido executada sem erros.
 * O codegen consulta a tabela semântica para reconhecer métodos herdados e
 * os métodos básicos de Object, IO e String que não possuem NODE_METODO na
 * AST do usuário.
 */
void iniciar_geracao(ASTNode *raiz);

/*
 * Visitor público do gerador. Normalmente iniciar_geracao() é a função
 * utilizada pelo main; gerar_codigo() é exposta para testes pontuais.
 */
void gerar_codigo(ASTNode *no);

/*
 * Gera identificadores únicos alocados dinamicamente:
 *   gerar_temporario() -> "v1", "v2", ...
 *   gerar_label("if_true") -> ".if_true_1", ...
 *
 * Quem chamar essas funções diretamente deve liberar o retorno com free().
 */
char *gerar_temporario(void);
char *gerar_label(const char *prefixo);

#endif /* CODEGEN_H */
