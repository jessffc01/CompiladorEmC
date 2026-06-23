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
extern Scope* escopo_atual;
/* ==========================================
        PROCURA PAI DE UMA CLASSE
========================================== */

char* getParent(char* className) {
    Symbol* atual = tabela_classes;
    while(atual != NULL) {
        if(strcmp(atual->nome, className) == 0) {
            return atual->info.smb_classe.parentName;
        }
        atual = atual->next;
    }

    return NULL;
}

/* ==========================================
      VERIFICA SE CHILD É SUBTIPO DE PARENT
========================================== */

int isSubtype(char* child, char* parent) {
    if(strcmp(child, parent) == 0) {
        return 1;
    }

    char* current = child;

    while(current != NULL && strcmp(current, "") != 0) {
        if(strcmp(current, parent) == 0) {
            return 1;
        }

        current = getParent(current);
    }

    return 0;
}

/* ==========================================
         MENOR ANCESTRAL COMUM
========================================== */

char* leastCommonAncestor(char* type1,  char* type2) {
    char* current = type1;

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

int searchItem(char* nome, Symbol* tabela_simbolos, NodeType tipo_node, char* classeOrigem, ASTNode* args, char* tipo_retorno){
    Symbol* item = buscar_simbolo(nome, tabela_simbolos);
    if(item != NULL){
        int sameClass;
        switch(item->tipo_simbolo){
            case SYM_CLASSE:
                sm_errors += 1;
                return 1;
                break;
        
            case SYM_ATRIBUTO:
                sameClass = strcmp(classeOrigem, item->info.smb_atributo.classOrigem);
                if(sameClass != 0){
                    return 0;
                }
                
                sm_errors += 1;
                return 1;
                break;

            case SYM_METODO:
                sameClass = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
                if(sameClass != 0){
                    return 0;
                }
                
                sm_errors += 1;
                return 1;
                break;

            case SYM_VAR:
                sameClass = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
                if(sameClass != 0){
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

int verifyParent(char* pai, int linha_pai, int col_pai){
    if(strcmp(pai, "Int") == 0 || strcmp(pai, "String") == 0 || strcmp(pai, "Bool") == 0){
        printf("Erro semantico: Nao eh permitido ter heranca do tipo %s. Linha: %d, Col: %d\n", pai, linha_pai, col_pai);
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

    printf("Erro semantico: Classe pai (%s) nao encontrada. Linha: %d, Col: %d\n", pai, linha_pai, col_pai);
    sm_errors += 1;
    return 0;
}

/* ==========================================
        ANALISADOR SEMÂNTICO
========================================== */
int checkProgram(ASTNode* node){
    init_symbols();
    ASTNode* atual = node;

    //Verificar se as classes são válidas e as adicionar à tabela de classes
    while(atual != NULL){
        char* nome = atual->dados.classe.nome_classe;
        int nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL, NULL, NULL);
        int paiValido = verifyParent(atual->dados.classe.nome_pai, atual->dados.classe.linha_pai, atual->dados.classe.coluna_pai);
        if(paiValido == 0){
            atual->dados.classe.nome_pai = "Object";
        }
        if(nomeOcupado == 1){
            printf("Erro semantico: Classe '%s' ja existe. Linha: %d, Col: %d\n", nome, atual->linha, atual->coluna); 
        }
        adicionar_symbol_classe(nome, atual->dados.classe.nome_pai);
        atual = atual->proximo;
    }

    atual = node;
    while(atual!= NULL){
    //Verificar as features da classe
        checkFeatures(atual->dados.classe.lista_features, atual->dados.classe.nome_classe);
        atual = atual->proximo;
    }

    return sm_errors;
}

void checkFeatures(ASTNode* node, char* classeOrigem){
    printf("\nChecando features da classe %s\n", classeOrigem);
    ASTNode* atual = node;
    ASTNode* init;
    ASTNode* corpo;
    int nomeOcupado;
    char* tipoFeature;
    char* nome;
    char* nomeMetodo;

    //Adicionando features às tabelas
    while(atual != NULL){
        switch(atual->tipo){
            case NODE_ATRIBUTO:
                tipoFeature = atual->dados.atributo.tipo_atributo;
                nome = atual->dados.atributo.nome_atributo;
                nomeOcupado = searchItem(nome, tabela_atributos, atual->tipo, classeOrigem, NULL, NULL);
                if(nomeOcupado == 1){
                    printf("Erro semantico: Atributo '%s' ja existe neste escopo. Linha: %d, Col: %d\n", nome, atual->linha, atual->coluna); 
                }
                else{
                    adicionar_symbol_atributo(nome, tipoFeature, classeOrigem);
                }

                if(buscar_simbolo(tipoFeature, tabela_classes) == NULL){
                    atual->dados.atributo.tipo_valido = 0;
                    int linha_tipo = atual->dados.atributo.linha_tipo;
                    int col_tipo = atual->dados.atributo.coluna_tipo;
                    printf("Erro semantico: Tipo '%s' nao existente. Linha: %d, Col: %d\n", tipoFeature, linha_tipo, col_tipo);
                }
                break;
                

            case NODE_METODO:

                tipoFeature = atual->dados.metodo.tipo_retorno;
                nomeMetodo = atual->dados.metodo.nome_metodo;

                nomeOcupado = searchItem(nomeMetodo, tabela_metodos, atual->tipo, classeOrigem, NULL, atual->dados.metodo.tipo_retorno);
                if(nomeOcupado == 1){
                    printf("Erro semantico: Metodo '%s' ja existe neste escopo. Linha: %d, Col: %d\n", nomeMetodo, atual->linha, atual->coluna); 
                }
                else{
                    adicionar_symbol_metodo(nomeMetodo, tipoFeature, classeOrigem, atual->dados.metodo.lista_formais);
                }
                if(buscar_simbolo(tipoFeature, tabela_classes) == NULL){
                    atual->dados.metodo.tipo_valido = 0;
                    int linha_tipo = atual->dados.metodo.linha_tipo;
                    int col_tipo = atual->dados.metodo.coluna_tipo;
                    printf("Erro semantico: Tipo '%s' nao existente. Linha: %d, Col: %d\n", tipoFeature, linha_tipo, col_tipo);
                }
                break;

            default:
                sm_errors += 1;
                printf("Erro semantico: Tipo de feature invalido. Linha: %d, Col: %d\n", atual->linha, atual->coluna);
                break; 
        }
        atual = atual->proximo;
    }

    //Verificando inicializações de atributos / corpo de métodos
    atual = node;
    while(atual!= NULL){
        switch(atual->tipo){
            case NODE_ATRIBUTO:
                nome = atual->dados.atributo.nome_atributo;
                init = atual->dados.atributo.inicializacao;
                tipoFeature = atual->dados.atributo.tipo_atributo;
                if(init != NULL){
        
                    char* tipo_init = checkExpr(init, classeOrigem);
                    if(atual->dados.atributo.tipo_valido != 0){
                        if(isSubtype(tipo_init, tipoFeature) == 0 && isSubtype(tipoFeature, tipo_init) == 0){
                            sm_errors += 1;
                            printf("Erro semantico: Tipo da expressao (%s) nao corresponde ao do atributo (%s). Linha: %d, Col: %d\n", tipo_init, tipoFeature, init->linha, init->coluna); 
                        }
                    }
                }
                break;
            
            case NODE_METODO:
                nomeMetodo = atual->dados.metodo.nome_metodo;
                corpo = atual->dados.metodo.corpo;
                tipoFeature = atual->dados.metodo.tipo_retorno;
                if(strcmp(tipoFeature, "SELF_TYPE") == 0 || strcmp(tipoFeature, "self") == 0){
                    tipoFeature = classeOrigem;
                }
                
                if(corpo != NULL){
                    char* tipo_corpo = checkExpr(corpo, classeOrigem);
                    if(atual->dados.metodo.tipo_valido != 0){
                        if(isSubtype(tipo_corpo, tipoFeature) == 0 && isSubtype(tipoFeature, tipo_corpo) == 0){
                            sm_errors += 1;
                            printf("Erro semantico: Tipo '%s' nao corresponde ao esperado pelo metodo (%s).Linha: %d, Col: %d\n", tipo_corpo, tipoFeature, corpo->linha, corpo->coluna); 
                        }
                    }
                }
                break;
        }
        atual = atual->proximo;
    }
}

char* checkExpr(ASTNode* node, char* classeOrigem) {
    int linha_tipo, col_tipo, linha_expr, col_expr;
    /* ---------- ATRIBUIÇÃO ---------- */
    if(node->tipo == NODE_ATRIBUICAO){
        Scope* escopo_it = escopo_atual;
        Symbol* sym_var;
        while(escopo_it!=NULL){
            sym_var = buscar_simbolo(node->dados.atribuicao.nome_variavel, escopo_atual->tabela_variaveis);
            if(sym_var != NULL) {
                break;
            }
            escopo_it = escopo_it->anterior;
        }
        
        char* valorType = checkExpr(node->dados.atribuicao.valor, classeOrigem);
        
        if(sym_var == NULL){
            sym_var = buscar_simbolo(node->dados.atribuicao.nome_variavel, tabela_atributos);
            if(sym_var == NULL){
                adicionar_symbol_var(node->dados.atribuicao.nome_variavel, valorType, classeOrigem);
            }
        }
        switch(sym_var->tipo_simbolo){
            case SYM_ATRIBUTO:
                if(strcmp(sym_var->info.smb_atributo.tipo, valorType) != 0) {
                    char* atribType = sym_var->info.smb_atributo.tipo;
                    printf("Erro semantico: Tipo da atribuicao (%s) nao corresponde ao do atributo (%s).Linha: %d, Col: %d\n",valorType, atribType, node->linha, node->coluna);
                    sm_errors += 1;
                }
                break;
            case SYM_VAR:
                if(strcmp(sym_var->info.smb_var.tipo, valorType) != 0) {
                    char* varType = sym_var->info.smb_var.tipo;
                    printf("Erro semantico: Tipo da atribuicao (%s) nao corresponde ao da variavel (%s).Linha: %d, Col: %d\n", valorType, varType, node->linha, node->coluna);
                    sm_errors += 1;
                }
                break;
        }
        return valorType;
    }

    /* ---------- IF ---------- */
    else if(node->tipo == NODE_IF) {

        char* condType =
            checkExpr(node->dados.no_if.condicao, classeOrigem);

        if(strcmp(condType, "Bool") != 0) {

            printf("Erro semantico: "
                   "condicao de IF deve ser Bool. Linha: %d, Col: %d\n", node->linha, node->coluna);
            sm_errors += 1;
        }

        char* thenType =
            checkExpr(node->dados.no_if.bloco_then, classeOrigem);

        char* elseType =
            checkExpr(node->dados.no_if.bloco_else, classeOrigem);

        return leastCommonAncestor(thenType, elseType);
    }

    /* ---------- LET ---------- */
    else if (node->tipo == NODE_LET) {
        // Entrar em escopo novo
        push_scope();

        ASTNode* decl = node->dados.no_let.lista_variaveis;
        while (decl != NULL) {
            int validType = 1;
            char* nome = decl->dados.let_var.nome_variavel;
            char* tipo = decl->dados.let_var.tipo_variavel;
            if(buscar_simbolo(tipo, tabela_classes) == NULL){
                validType = 0;
                linha_tipo = decl->dados.let_var.linha_tipo;
                col_tipo = decl->dados.let_var.coluna_tipo;
                printf("Erro semantico: Tipo '%s' nao existe. Linha: %d, Col: %d\n", tipo, linha_tipo, col_tipo);
            }
            ASTNode* init = decl->dados.let_var.inicializacao;

            if (init != NULL) {
                char* tipo_init = checkExpr(init, classeOrigem);
                if (validType != 0 && isSubtype(tipo_init, tipo) == 0) {
                    printf("Erro semantico: inicializacao de '%s' incompativel com tipo '%s'. Linha: %d, Col: %d\n", nome, tipo, init->linha, init->coluna);
                    sm_errors++;
                }
            }

            adicionar_symbol_var(nome, tipo, classeOrigem);
            decl = decl->proximo;
        }

        // Avalia o corpo do LET
        char* result = checkExpr(node->dados.no_let.corpo, classeOrigem);

        // Sai do escopo (variáveis somem)
        pop_scope();

        return result;
    }

    /* ---------- CASE ---------- */
    else if (node->tipo == NODE_CASE)
    {
        push_scope();
        ASTNode *branch = node->dados.no_case.lista_cases;
        char *resultType = checkExpr(node->dados.no_case.expressao_principal, classeOrigem);
        while (branch != NULL)
        {
            char *nome = branch->dados.case_branch.nome_variavel;
            char *tipo = branch->dados.case_branch.tipo_variavel;
            if(isSubtype(tipo, resultType) == 0){
                linha_tipo = branch->dados.case_branch.linha_tipo;
                col_tipo = branch->dados.case_branch.coluna_tipo;
                printf("Erro semantico: Tipo estatico da branch (%s) nao eh subtipo de (%s). Linha: %d, Col: %d\n", tipo, resultType, linha_tipo, col_tipo);
            }
            ASTNode *expr = branch->dados.case_branch.corpo;
            adicionar_symbol_var(nome, tipo, classeOrigem);

            char *branchType = checkExpr(expr, classeOrigem);
            resultType = leastCommonAncestor(resultType, branchType);
            branch = branch->proximo;
        }
        pop_scope();

        return resultType;
    }
    /* ---------- WHILE ---------- */

    else if(node->tipo == NODE_WHILE) {
        char* condType =
            checkExpr(node->dados.no_while.condicao, classeOrigem);

        if(strcmp(condType, "Bool") != 0) {
            int linha_cond = node->dados.no_while.condicao->linha;
            int col_cond = node->dados.no_while.condicao->coluna;
            printf("Erro semantico: "
                   "condicao de WHILE deve ser Bool. Linha: %d, Col: %d\n", linha_cond, col_cond);
            sm_errors += 1;
        }

        checkExpr(node->dados.no_while.condicao, classeOrigem);

        return "Object";
    }

    /* ---------- BLOCO ---------- */

    else if(node->tipo == NODE_BLOCO) {
        char* lastType = "Object";

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
        char* left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        char* right =
            checkExpr(node->dados.operacao_binaria.direito, classeOrigem);

        int linha, col;
        if(strcmp(left, "Int") != 0) {
            linha = node->dados.operacao_binaria.esquerdo->linha;
            col = node->dados.operacao_binaria.esquerdo->coluna;
            printf("Erro semantico: Operador esquerdo de operacao aritmetica possui tipo diferente de Int. Linha: %d, Col: %d\n", linha, col);
            sm_errors += 1;
        }
        else if(strcmp(right, "Int") != 0){
            linha = node->dados.operacao_binaria.direito->linha;
            col = node->dados.operacao_binaria.direito->coluna;
            printf("Erro semantico: Operador direito de operacao aritmetica possui tipo diferente de Int. Linha: %d, Col: %d\n", linha, col);
            sm_errors += 1;
        }

        return "Int";
    }

    /* ---------- MENOR, MENOR OU IGUAL, OU IGUALDADE ---------- */

    else if(node->tipo == NODE_RELACIONAL) {
        char* left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        char* right =
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
                    printf("Erro semantico: "
                        "tipos incompativeis em igualdade. Linha: %d, Col: %d\n", node->linha, node->coluna);
                    sm_errors += 1;
                }
            }
        }
        else{
            if(leftInt != 0) {
                int linha_op = node->dados.operacao_binaria.esquerdo->linha;
                int col_op = node->dados.operacao_binaria.esquerdo->coluna;
                printf("Erro semantico: Operador esquerdo de operacao menor (ou menor/igual) nao eh do tipo Int\n. Linha: %d, Col: %d\n", linha_op, col_op);
                sm_errors += 1;
            }
            else if(rightInt != 0){
                int linha_op = node->dados.operacao_binaria.direito->linha;
                int col_op = node->dados.operacao_binaria.direito->coluna;
                printf("Erro semantico: Operador direito de operacao menor (ou menor/igual) nao eh do tipo Int\n. Linha: %d, Col: %d\n", linha_op, col_op);
                sm_errors += 1;
            }
        }

        return "Bool";
    }

   /* ---------- DISPATCH IMPLÍCITO ---------- */
    else if (node->tipo == NODE_DISPATCH_IMPLICITO)
    {

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            printf("Erro semantico: metodo '%s' nao encontrado. Linha: %d, Col: %d\n", node->dados.dispatch.nome_metodo, node->linha, node->coluna);
            sm_errors += 1;
            return "Object";
        }

        ASTNode *args = node->dados.dispatch.argumentos;
        Symbol *formais = metodo->info.smb_metodo.parametros; // <-- corrigido
        while (args != NULL && formais != NULL)
        {
            char *argType = checkExpr(args, classeOrigem);
            if (isSubtype(argType, formais->info.smb_formal.tipo) == 0)
            {
                printf("Erro semantico: argumento incompativel no metodo '%s'. Linha: %d, Col: %d\n",
                       node->dados.dispatch.nome_metodo, args->linha, args->coluna);
                sm_errors += 1;
            }
            args = args->proximo;
            formais = formais->next;
        }

        return metodo->info.smb_metodo.tipo_retorno;
    }

    /* ---------- DISPATCH EXPLÍCITO ---------- */
    else if (node->tipo == NODE_DISPATCH_EXPLICITO)
    {
        char *exprType = checkExpr(node->dados.dispatch.expressao_base, classeOrigem);
        char *targetType = node->dados.dispatch.tipo_estatico;

        if (targetType != NULL && isSubtype(exprType, targetType) == 0)
        {
            linha_expr = node->dados.dispatch.expressao_base->linha;
            col_expr = node->dados.dispatch.expressao_base->coluna;
            printf("Erro semantico: '%s' nao eh subtipo de '%s' em dispatch explicito. Linha: %d, Col: %d\n",
                   exprType, targetType, linha_expr, col_expr);
            sm_errors += 1;
        }

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            int linha_met = node->dados.dispatch.linha_metodo;
            int col_met = node->dados.dispatch.col_metodo;
            printf("Erro semantico: metodo '%s' nao encontrado em '%s'. Linha: %d, Col: %d\n",
                   node->dados.dispatch.nome_metodo, targetType, linha_met, col_met);
            sm_errors += 1;
            return "Object";
        }

        ASTNode *args = node->dados.dispatch.argumentos;
        Symbol *formais = metodo->info.smb_metodo.parametros; // <-- corrigido
        while (args != NULL && formais != NULL)
        {
            char *argType = checkExpr(args, classeOrigem);
            if (isSubtype(argType, formais->info.smb_formal.tipo) == 0)
            {
                printf("Erro semantico: argumento incompativel no metodo '%s'. Linha: %d, Col: %d\n",
                       node->dados.dispatch.nome_metodo, args->linha, args->coluna);
                sm_errors += 1;
            }
            args = args->proximo;
            formais = formais->next;
        }

        return metodo->info.smb_metodo.tipo_retorno;
    }

    /* ---------- NEW ---------- */

    else if(node->tipo == NODE_NEW){
        if(strcmp(node->dados.valor_lexico, "SELF_TYPE") == 0 || strcmp(node->dados.valor_lexico, "self") == 0){
            return classeOrigem;
        }
        else{
            if(buscar_simbolo(node->dados.valor_lexico, tabela_classes) == NULL){
                printf("Erro semantico: Tipo '%s' nao existe. Linha: %d, Col: %d\n", node->dados.valor_lexico, node->linha, node->coluna);
                sm_errors += 1;
            }
            return node->dados.valor_lexico;
        }
    }

    /* ---------- NOT ---------- */

    else if(node->tipo == NODE_NOT){
        char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        if(strcmp(type_expr, "Bool") < 0){
            linha_expr = node->dados.operacao_unaria.expressao->linha;
            col_expr = node->dados.operacao_unaria.expressao->coluna;
            printf("Erro semantico: "
                    "operacao NOT requer expressao booleana. Linha: %d, Col: %d\n", linha_expr, col_expr);
            sm_errors+= 1;
        }

        return "Bool";
    }

    /* ---------- COMPLEMENTO ---------- */

    else if(node->tipo == NODE_COMPLEMENTO){
        char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);
        
        if(strcmp(type_expr, "Int") < 0){
            linha_expr = node->dados.operacao_unaria.expressao->linha;
            col_expr = node->dados.operacao_unaria.expressao->coluna;
            printf("Erro semantico: "
                    "operacao de complemento requer um Inteiro. Linha: %d, Col: %d\n", linha_expr, col_expr);
            sm_errors+= 1;
        }

        return "Int";
    }

    /* ---------- ISVOID ---------- */

    else if(node->tipo == NODE_ISVOID){
        char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        return "Bool";
    }

    /* ---------- IDENTIFICADOR ---------- */

    else if(node->tipo == NODE_IDENTIFICADOR) {       
        // CORREÇÃO: Usando a nomenclatura exata da union no ast.h
        char* nome = node->dados.valor_lexico; 

        // 1. Regra do 'self'
        if(strcmp(nome, "self") == 0) {
            return classeOrigem; 
        }

        // 2. Procurar na Tabela de Variáveis Locais / Parâmetros
        Scope* escopo_it = escopo_atual;
        while(escopo_it!=NULL){
            Symbol* sym_var = buscar_simbolo(nome, escopo_atual->tabela_variaveis);
            if(sym_var != NULL) {
                return sym_var->info.smb_var.tipo; 
            }
            escopo_it = escopo_it->anterior;
        }

        // 3. Procurar na Tabela de Atributos (da própria classe ou herdados)
        Symbol* sym_attr = buscar_simbolo(nome, tabela_atributos);
        if(sym_attr != NULL) {
            if(isSubtype(classeOrigem, sym_attr->info.smb_atributo.classOrigem) == 1) {
                return sym_attr->info.smb_atributo.tipo;
            }
        }

        // 4. Variável fantasma (Erro!)
        printf("Erro semantico: Identificador '%s' nao declarado neste escopo. Linha: %d, Col: %d\n", nome, node->linha, node->coluna);
        sm_errors += 1;
        
        return "Object"; 
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
        printf("Aviso: comportamento desconhecido para o NodeType %d. Linha: %d, Col: %d\n",
               node->tipo, node->linha, node->coluna);
        sm_errors += 1;
        return "Object";
    }
}