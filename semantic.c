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
                /*if(item->info.smb_classe.confirmado == 0){
                    item->info.smb_classe.confirmado = 1;
                    return 0;
                }*/
                sm_errors += 1;
                return 1;
                break;
        
            case SYM_ATRIBUTO:
                sameClass = strcmp(classeOrigem, item->info.smb_atributo.classOrigem);
                if(sameClass != 0){
                    return 0;
                }
                /*else if(sameClass == 0 && item->info.smb_atributo.confirmado == 0){
                    item->info.smb_atributo.confirmado = 1;
                    return 0;
                }*/
                
                sm_errors += 1;
                return 1;
                break;

            case SYM_METODO:
                sameClass = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
                if(sameClass != 0){
                    return 0;
                }
                /*else if(item->info.smb_metodo.confirmado == 0 || tipo_node == NODE_DISPATCH_IMPLICITO || tipo_node == NODE_DISPATCH_EXPLICITO){
                    Symbol* parametros_it = item->info.smb_metodo.parametros;
                    while(parametros_it != NULL){
                        if(strcmp(parametros_it->info.smb_formal.tipo, args->dados.formal.tipo_parametro) != 0){
                            sm_errors += 1;
                            return 1;    
                        }

                        parametros_it = parametros_it->next;
                        args = args->proximo;
                        if((parametros_it == NULL && args != NULL) || (parametros_it == NULL && args != NULL)){
                            sm_errors += 1;
                            return 1;
                        }
                    }
                    if(item->info.smb_metodo.confirmado == 0){
                        item->info.smb_metodo.tipo_retorno = tipo_retorno;
                        item->info.smb_metodo.confirmado = 1;
                    }
                    return 0;
                }*/
                
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

int verifyParent(char* pai){
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
    //printf("\nChecando classes...\n");
    init_symbols();
    ASTNode* atual = node;

    //Verificar se as classes são válidas e as adicionar à tabela de classes
    while(atual != NULL){
        char* nome = atual->dados.classe.nome_classe;
        int nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL, NULL, NULL);
        int paiValido = verifyParent(atual->dados.classe.nome_pai);
        if(paiValido == 0){
            atual->dados.classe.nome_pai = "Object";
        }
        if(nomeOcupado == 1){
            printf("Erro semântico: Classe '%s' já existe\n", nome); 
        }
        adicionar_symbol_classe(nome, atual->dados.classe.nome_pai);
        atual = atual->proximo;
    }

    //imprimir_tabela(tabela_classes);
    //printf("\n-----Vamos checar as features de cada classe:-----\n");
    atual = node;
    while(atual!= NULL){
    //Verificar as features da classe
        checkFeatures(atual->dados.classe.lista_features, atual->dados.classe.nome_classe);
        atual = atual->proximo;
    }

    return sm_errors;
}

void checkFeatures(ASTNode* node, char* classeOrigem){
    printf("\nChecando features de %s\n", classeOrigem);
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
                    printf("Erro semântico: Atributo '%s' já existe neste escopo.\n", nome); 
                }
                else{
                    adicionar_symbol_atributo(nome, tipoFeature, classeOrigem);
                }
                break;

            case NODE_METODO:

                tipoFeature = atual->dados.metodo.tipo_retorno;
                nomeMetodo = atual->dados.metodo.nome_metodo;
                //printf("\nMetodo %s\n", nomeMetodo);
                nomeOcupado = searchItem(nomeMetodo, tabela_metodos, atual->tipo, classeOrigem, NULL, atual->dados.metodo.tipo_retorno);
                if(nomeOcupado == 1){
                    printf("Erro semantico: Metodo '%s' ja existe neste escopo.\n", nomeMetodo); 
                }
                else{
                    adicionar_symbol_metodo(nomeMetodo, tipoFeature, classeOrigem, atual->dados.metodo.lista_formais);
                }
                break;

            default:
                sm_errors += 1;
                printf("Erro semântico: Tipo de feature inválido.\n");
                break; 
        }
        atual = atual->proximo;
    }

    //imprimir_tabela(tabela_metodos);
    //Verificando inicializações de atributos / corpo de métodos
    //imprimir_tabela(tabela_atributos);
    //printf("\nVerificando expressões das features...\n");
    atual = node;
    while(atual!= NULL){
        switch(atual->tipo){
            case NODE_ATRIBUTO:
                nome = atual->dados.atributo.nome_atributo;
                init = atual->dados.atributo.inicializacao;
                tipoFeature = atual->dados.atributo.tipo_atributo;
                if(init != NULL){
                    printf("\nChecagem do atributo %s\n", nome);
                    char* tipo_init = checkExpr(init, classeOrigem);
                    if(isSubtype(tipo_init, tipoFeature) == 0 && isSubtype(tipoFeature, tipo_init) == 0){
                        sm_errors += 1;
                        printf("Erro semântico: Tipo da expressão (%s) não corresponde ao do atributo (%s).\n", tipo_init, tipoFeature); 
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
                printf("\n Checagem do metodo %s\n", nomeMetodo);
                if(corpo != NULL){
                    //printf("\nCheck method %s\n", nomeMetodo);
                    char* tipo_corpo = checkExpr(corpo, classeOrigem);
                    if(isSubtype(tipo_corpo, tipoFeature) == 0 && isSubtype(tipoFeature, tipo_corpo) == 0){
                        sm_errors += 1;
                        printf("Erro semântico: Tipo '%s' não corresponde ao esperado pelo metodo (%s).\n", tipo_corpo, tipoFeature); 
                    }
                }
                break;
        }
        atual = atual->proximo;
    }
}

char* checkExpr(ASTNode* node, char* classeOrigem) {
    //printf("\nVerificando nova expressão...\n");
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
                printf("Erro semântico: Tipo da atribuição não corresponde ao do atributo.\n");
                sm_errors += 1;
                }
                break;
            case SYM_VAR:
                if(strcmp(sym_var->info.smb_var.tipo, valorType) != 0) {
                    printf("Erro semântico: Tipo da atribuição não corresponde ao da variável.\n");
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

            printf("Erro semântico: "
                   "condição de IF deve ser Bool\n");
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
            char* nome = decl->dados.let_var.nome_variavel;
            char* tipo = decl->dados.let_var.tipo_variavel;
            ASTNode* init = decl->dados.let_var.inicializacao;

            if (init != NULL) {
                char* tipo_init = checkExpr(init, classeOrigem);
                if (isSubtype(tipo_init, tipo) == 0) {
                    printf("Erro semântico: inicialização de '%s' incompatível com tipo '%s'.\n", nome, tipo);
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

            printf("Erro semântico: "
                   "condição de WHILE deve ser Bool\n");
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
    else if (node->tipo == NODE_DISPATCH_IMPLICITO)
    {
        printf("\nDispatch implicito.\n");
        //const char *exprType = checkExpr(node->dados.dispatch.expressao_base, classeOrigem);

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            printf("Erro semântico: método '%s' não encontrado.\n",
                   node->dados.dispatch.nome_metodo);
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
                printf("Erro semântico: argumento incompatível no método '%s'.\n",
                       node->dados.dispatch.nome_metodo);
                sm_errors += 1;
            }
            args = args->proximo;
            formais = formais->next;
        }

        return metodo->info.smb_metodo.tipo_retorno;;
    }

    /* ---------- DISPATCH EXPLÍCITO ---------- */
    else if (node->tipo == NODE_DISPATCH_EXPLICITO)
    {
        printf("\nDispatch explicito.\n");
        char *exprType = checkExpr(node->dados.dispatch.expressao_base, classeOrigem);
        char *targetType = node->dados.dispatch.tipo_estatico;

        if (targetType != NULL && isSubtype(exprType, targetType) == 0)
        {
            printf("Erro semântico: '%s' não é subtipo de '%s' em dispatch explícito.\n",
                   exprType, targetType);
            sm_errors += 1;
        }

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            printf("Erro semântico: método '%s' não encontrado em '%s'.\n",
                   node->dados.dispatch.nome_metodo, targetType);
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
                printf("Erro semântico: argumento incompatível no método '%s'.\n",
                       node->dados.dispatch.nome_metodo);
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
            return node->dados.valor_lexico;
        }
    }

    /* ---------- NOT ---------- */

    else if(node->tipo == NODE_NOT){
        char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        if(strcmp(type_expr, "Bool") < 0){
            printf("Erro semântico: "
                    "operação NOT requer expressão booleana\n");
            sm_errors+= 1;
        }

        return "Bool";
    }

    /* ---------- COMPLEMENTO ---------- */

    else if(node->tipo == NODE_COMPLEMENTO){
        char* type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);
        
        if(strcmp(type_expr, "Int") < 0){
            printf("Erro semântico: "
                    "operação de complemento requer um Inteiro\n");
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
        //printf("\nBuscando identificador!\n");       
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
        printf("Erro semântico: Identificador '%s' não declarado neste escopo.\n", nome);
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
        printf("Aviso: comportamento desconhecido para o NodeType %d\n",
               node->tipo);
        sm_errors += 1;
        return "Object";
    }
}