#include "semantic.h"
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

int sm_errors = 0;

/* ==========================================
            TABELA DE HERANÇA
========================================== */
extern Symbol* tabela_classes;
extern Symbol* tabela_atributos;
extern Symbol* tabela_metodos;
extern Symbol* tabela_variaveis;
/* ==========================================
        PROCURA PAI DE UMA CLASSE
========================================== */

const char* getParent(const char* className) {
    Symbol* atual = tabela_classes;
    while(atual != NULL) {
        if(strcmp(atual->nome, className) == 0) {
            return atual->info.smb_classe.parentName;
        }
    }

    return NULL;
}

/* ==========================================
      VERIFICA SE CHILD É SUBTIPO DE PARENT
========================================== */

int isSubtype(const char* child, const char* parent) {
    if(strcmp(child, parent) == 0) {
        return 1;
    }

    const char* current = child;

    while(current != NULL && strcmp(current, "") != 0) {
        if(strcmp(current, parent) == 0) {
            return 1;
        }

        current = getParent(current);
    }

    sm_errors += 1;
    return 0;
}

/* ==========================================
         MENOR ANCESTRAL COMUM
========================================== */

const char* leastCommonAncestor(const char* type1,
                                const char* type2) {
    const char* current = type1;

    while(current != NULL && strcmp(current, "") != 0) {

        if(isSubtype(type2, current)) {
            return current;
        }

        current = getParent(current);
    }

    return "Object";
}

/* ==========================================
        VERIFICADOR DE ITEM JÁ EXISTENTE
========================================== */

int searchItem(const char* nome, Symbol* tabela_simbolos, NodeType tipo, char* classeOrigem){
    Symbol* item = buscar_simbolo(nome, tabela_simbolos);
    if(item != NULL){
        switch(item->tipo_simbolo){
            case SYM_CLASSE:
                if(item->info.smb_classe.confirmado == 0){
                    item->info.smb_classe.confirmado = 1;
                    return 0;
                }
                sm_errors += 1;
                return 1;
                break;
        
            case SYM_ATRIBUTO:
                int sameClass = strcmp(classeOrigem, item->info.smb_atributo.classOrigem);
                if(sameClass < 0){
                    return 0;
                }
                else if(sameClass == 0 && item->info.smb_atributo.confirmado == 0){
                    item->info.smb_atributo.confirmado = 1;
                    return 0;
                }
                
                sm_errors += 1;
                return 1;
                break;

            case SYM_METODO:
                int sameClass = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
                if(sameClass < 0){
                    return 0;
                }
                else if(sameClass == 0 && item->info.smb_metodo.confirmado == 0){
                    item->info.smb_metodo.confirmado = 1;
                    return 0;
                }
                sm_errors += 1;
                return 1;
                break;

            case SYM_VAR:
                int sameClass = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
                if(sameClass < 0){
                    return 0;
                }
                sm_errors += 1;
                return 1;
                break;
        }
    }
    return 0;
}

/* ==========================================
        VERIFICADOR DE VALIDADE DE CLASSE PAI
========================================== */

int verifyParent(const char* pai){
    if(strcmp(pai, "Int") == 0 || strcmp(pai, "String") == 0 || strcmp(pai, "Bool") == 0){
        printf("Erro semântico: Não é permitido ter herança do tipo %s\n", pai);
        sm_errors += 1;
        return 0;
    }

    Symbol* tabela_it = tabela_classes;
    while(tabela_it != NULL){
        if(strcmp(pai, tabela_it->nome) == 0){
            return 1;
        }
        tabela_it = tabela_it->next;
    }

    printf("Erro semântico: Classe pai não encontrada\n");
    sm_errors += 1;
    return 0;
}

/* ==========================================
        ANALISADOR SEMÂNTICO
========================================== */
int checkProgram(ASTNode* node){
    ASTNode* atual = node;

    //Verificar se as classes são válidas e as adicionar à tabela de classes
    while(atual != NULL){
        char* nome = atual->dados.classe.nome_classe;
        int nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL);
        int paiValido = verifyParent(atual->dados.classe.nome_pai);
        if(paiValido == 0){
            atual->dados.classe.nome_pai = "Object";
        }
        if(nomeOcupado == 1){
            printf("Erro semântico: Classe '%s' já existe\n", nome); 
            int i = 0;
            char strnum[20];
            char* nomeOriginal = strdup(nome);
            while(TRUE){
                sprintf(strnum, "%d", i);
                strcat(nome, strnum);
                nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL);
                if(nomeOcupado == 0){
                    break;
                }
                nome = nomeOriginal;
                i++;
            }
        }
        adicionar_symbol_classe(nome, atual->dados.classe.nome_pai, 1);

        //Verificar as features da classe
        checkFeatures(node->dados.classe.lista_features, nome);

        atual = atual->proximo;
    }

    if(sm_errors == 0){
        return 1;
    }
    else{
        return 0;
    }
}

void checkFeatures(ASTNode* node, char* classeOrigem){
    ASTNode* atual = node;
    int nomeOcupado;
    while(atual != NULL){
        switch(atual->tipo){
            case NODE_ATRIBUTO:
                char* nome = atual->dados.atributo.nome_atributo;
                nomeOcupado = searchItem(nome, tabela_atributos, atual->tipo, classeOrigem);
                if(nomeOcupado == 1){
                    printf("Erro semântico: Atributo '%s' já existe neste escopo.\n", nome); 
                }

                ASTNode* init = atual->dados.atributo.inicializacao;
                char* tipo = atual->dados.atributo.tipo_atributo;
                if(init != NULL){
                    char* tipo_init = checkExpr(init, classeOrigem);
                    if(isSubtype(tipo_init, tipo) == 1){
                        adicionar_symbol_atributo(nome, tipo, classeOrigem, 1);
                    }
                    else{
                        printf("Erro semântico: Tipo da expressão não corresponde ao do atributo.\n", nome); 
                    }
                }
                else{
                    adicionar_symbol_atributo(nome, tipo, classeOrigem, 1);
                }
                
                break;

            case NODE_METODO:
                char* nome = atual->dados.metodo.nome_metodo;
                nomeOcupado = searchItem(nome, tabela_metodos, atual->tipo, classeOrigem);
                if(nomeOcupado == 1){
                    printf("Erro semantico: Metodo '%s' ja existe neste escopo.\n", nome); 
                }

                ASTNode* corpo = atual->dados.metodo.corpo;
                char* tipo = atual->dados.metodo.tipo_retorno;
                if(tipo == "SELF_TYPE"){
                    tipo = classeOrigem;
                }
                if(init != NULL){
                    char* tipo_corpo = checkExpr(corpo, classeOrigem);
                    if(isSubtype(tipo_corpo, tipo) == 1){
                        adicionar_symbol_metodo(nome, tipo, classeOrigem, 1, atual->dados.metodo.lista_formais);
                    }
                    else{
                        printf("Erro semântico: Tipo da expressão '%s' não corresponde ao esperado pelo metodo (%s).\n", tipo_corpo, tipo); 
                    }
                }
                else{
                    adicionar_symbol_metodo(nome, tipo, classeOrigem, 1, atual->dados.metodo.lista_formais);
                }
                break;
            default:
                sm_errors += 1;
                printf("Erro semântico: Tipo de feature inválido.\n");
                break; 
        }
        atual = atual->proximo;
    }
}

const char* checkExpr(ASTNode* node, char* classeOrigem) {
    /* ---------- ATRIBUIÇÃO ---------- */
    if(node->tipo == NODE_ATRIBUICAO){

        const char* valorType = checkExpr(node->dados.atribuicao.valor, classeOrigem);

        return valorType;
    }

    /* ---------- IF ---------- */
    else if(node->tipo == NODE_IF) {

        const char* condType =
            checkExpr(node->dados.no_if.condicao, classeOrigem);

        if(strcmp(condType, "Bool") != 0) {

            printf("Erro semântico: "
                   "condição de IF deve ser Bool\n");
            sm_errors += 1;
        }

        const char* thenType =
            checkExpr(node->dados.no_if.bloco_then, classeOrigem);

        const char* elseType =
            checkExpr(node->dados.no_if.bloco_else, classeOrigem);

        return leastCommonAncestor(thenType, elseType);
    }

    /* ---------- LET ---------- */
        else if(node->tipo == NODE_LET) {

    }

    /* ---------- CASE ---------- */

    else if(node->tipo == NODE_CASE){
    
    }
    /* ---------- WHILE ---------- */

    else if(node->tipo == NODE_WHILE) {
        const char* condType =
            checkExpr(node->dados.no_while.condicao, classeOrigem);

        if(strcmp(condType, "Bool") != 0) {

            printf("Erro semântico: "
                   "condição de WHILE deve ser Bool\n");
            sm_errors += 1;
        }

        checkExpr(node->dados.no_while.condicao, classeOrigem);

        return "Object";
    }

    /* ---------- BLOCO ---------- */

    else if(node->tipo == NODE_BLOCO) {
        const char* lastType = "Object";

        ASTNode* last = node->dados.bloco.lista_comandos;
        while(last->proximo != NULL) {
            last = last->proximo;
        }
        lastType =
            checkExpr(last, classeOrigem);

        return lastType;
    }

    /* ---------- SOMA, SUBTRAÇÃO, MULTIPLICAÇÃO E DIVISÃO ---------- */

    else if(node->tipo == NODE_ARITMETICO) {
        const char* left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        const char* right =
            checkExpr(node->dados.operacao_binaria.direito, classeOrigem);

        if(strcmp(left, "Int") != 0 ||
           strcmp(right, "Int") != 0) {

            printf("Erro semântico: "
                   "operação aritmetica requer Int\n");
            sm_errors += 1;
        }

        return "Int";
    }

    /* ---------- MENOR, MENOR OU IGUAL, OU IGUALDADE ---------- */

    else if(node->tipo == NODE_RELACIONAL) {
        const char* left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        const char* right =
            checkExpr(node->dados.operacao_binaria.direito, classeOrigem);
        
        int leftInt = strcmp(left, "Int");
        int rightInt = strcmp(right, "Int");
        if(node->dados.operacao_binaria.operador == IGUAL){
            int leftStr = strcmp(left, "String");
            int rightStr = strcmp(right, "String");
            int leftBool = strcmp(left, "Bool");
            int rightBool = strcmp(right, "Bool");
            
            if(leftInt != 0 || rightInt != 0 || leftStr != 0 || rightStr != 0 || leftBool != 0 || rightBool != 0){
                if(strcmp(left, right) != 0) {
                    printf("Erro semântico: "
                        "tipos incompatíveis em igualdade\n");
                    sm_errors += 1;
                }
            }
        }
        else{
            if(leftInt != 0 || rightInt != 0) {
                printf("Erro semântico: "
                    "operação menor (ou menor/igual) requer Int\n");
                sm_errors += 1;
            }
        }

        return "Bool";
    }

    /* ---------- DISPATCH IMPLÍCITO ---------- */

    else if(node->tipo == NODE_DISPATCH_IMPLICITO){
    
    }

    /* ---------- DISPATCH EXPLÍCITO ---------- */

    else if(node->tipo == NODE_DISPATCH_EXPLICITO){
    
    }

    /* ---------- NEW ---------- */

    else if(node->tipo == NODE_NEW){
        if(node->dados.valor_lexico = "SELF_TYPE"){
            return classeOrigem;
        }
        else{
            return node->dados.valor_lexico;
        }
    }

    /* ---------- NOT ---------- */

    else if(node->tipo == NODE_NOT){
        const char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        if(strcmp(type_expr, "Bool") < 0){
            printf("Erro semântico: "
                    "operação NOT requer expressão booleana\n");
            sm_errors+= 1;
        }

        return "Bool";
    }

    /* ---------- COMPLEMENTO ---------- */

    else if(node->tipo == NODE_COMPLEMENTO){
    
    }

    /* ---------- ISVOID ---------- */

    else if(node->tipo == NODE_ISVOID){
    
    }



    /* ---------- NÚMERO ---------- */

    else if(node->tipo == NODE_INTEIRO) {
        return "Int";
    }


    /* ---------- BOOLEANOS ---------- */

    else if(node->tipo == NODE_BOOLEANO) {
        return "Bool";
    }


    /* ---------- STRING ---------- */

    else if(node->tipo == NODE_STRING) {
        return "String";
    }


    /* ---------- FALLBACK ---------- */

    else {
        printf("Aviso: tipo desconhecido para %s\n",
               node->tipo);
        sm_errors += 1;
        return "Object";
    }
}