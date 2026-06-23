#include "parser.h"
#include "symtab.h"
#include <stdio.h>
#include <string.h>
#ifndef SEMANTIC_H
#define SEMANTIC_H

/*
    ==========================================
           ESTRUTURA SIMPLIFICADA
    ==========================================

    Exemplo de hierarquia:

    Object
      |
    Animal
     /  \
   Dog  Cat

*/


/* ==========================================
        PROCURA PAI DE UMA CLASSE
========================================== */

char* getParent(char* className);

/* ==========================================
      VERIFICA SE CHILD É SUBTIPO DE PARENT
========================================== */

int isSubtype(char* child, char* parent);

/* ==========================================
         MENOR ANCESTRAL COMUM
========================================== */

char* leastCommonAncestor(char* type1, char* type2);

/* ==========================================
        VERIFICADOR DE ITEM JÁ EXISTENTE
========================================== */

int searchItem(char* nome, Symbol* tabela_simbolos, NodeType tipo_node, char* classeOrigem, ASTNode* args, char* tipo_retorno);

/* ==========================================
        VERIFICADOR DE VALIDADE DE CLASSE PAI
========================================== */

int verifyParent(char* pai, int linha_pai, int col_pai);

/* ==========================================
        ANALISADOR SEMÂNTICO
========================================== */
int checkProgram(ASTNode* node);

void checkFeatures(ASTNode* node, char* classeOrigem);

char* checkExpr(ASTNode* node, char* classeOrigem);

#endif