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

const char* getParent(const char* className);

/* ==========================================
      VERIFICA SE CHILD É SUBTIPO DE PARENT
========================================== */

int isSubtype(const char* child, const char* parent);

/* ==========================================
         MENOR ANCESTRAL COMUM
========================================== */

const char* leastCommonAncestor(const char* type1, const char* type2);

/* ==========================================
        VERIFICADOR DE ITEM JÁ EXISTENTE
========================================== */

int searchItem(const char* nome, Symbol* tabela_simbolos, NodeType tipo, char* classeOrigem);

/* ==========================================
        VERIFICADOR DE VALIDADE DE CLASSE PAI
========================================== */

int verifyParent(const char* pai);

/* ==========================================
        ANALISADOR SEMÂNTICO
========================================== */
int checkProgram(ASTNode* node);

void checkFeatures(ASTNode* node, char* classeOrigem);

const char* checkExpr(ASTNode* node, char* classeOrigem);

#endif