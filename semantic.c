#include "semantic.h"
#include "symtab.h"

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
extern Symbol *tabela_classes;
extern Symbol *tabela_atributos;
extern Symbol *tabela_metodos;
extern Symbol *tabela_variaveis;
/* ==========================================
        PROCURA PAI DE UMA CLASSE
========================================== */

const char *getParent(const char *className)
{
    Symbol *atual = tabela_classes;
    while (atual != NULL)
    {
        if (strcmp(atual->nome, className) == 0)
        {
            return atual->info.smb_classe.parentName;
        }
    }

    return NULL;
}

/* ==========================================
      VERIFICA SE CHILD É SUBTIPO DE PARENT
========================================== */

int isSubtype(const char *child, const char *parent)
{
    if (strcmp(child, parent) == 0)
    {
        return 1;
    }

    const char *current = child;

    while (current != NULL && strcmp(current, "") != 0)
    {
        if (strcmp(current, parent) == 0)
        {
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

const char *leastCommonAncestor(const char *type1,
                                const char *type2)
{
    const char *current = type1;

    while (current != NULL && strcmp(current, "") != 0)
    {

        if (isSubtype(type2, current))
        {
            return current;
        }

        current = getParent(current);
    }

    return "Object";
}

/* ==========================================
        VERIFICADOR DE ITEM JÁ EXISTENTE
========================================== */

int searchItem(const char *nome, Symbol *tabela_simbolos, NodeType tipo, char *classeOrigem)
{
    // Cast para remover o warning de const
    Symbol *item = buscar_simbolo((char *)nome, tabela_simbolos);
    if (item != NULL)
    {
        switch (item->tipo_simbolo)
        {
        case SYM_CLASSE:
        {
            if (item->info.smb_classe.confirmado == 0)
            {
                item->info.smb_classe.confirmado = 1;
                return 0;
            }
            sm_errors += 1;
            return 1;
        }

        case SYM_ATRIBUTO:
        {
            int sameClassAttr = strcmp(classeOrigem, item->info.smb_atributo.classOrigem);
            if (sameClassAttr < 0)
            {
                return 0;
            }
            else if (sameClassAttr == 0 && item->info.smb_atributo.confirmado == 0)
            {
                item->info.smb_atributo.confirmado = 1;
                return 0;
            }
            sm_errors += 1;
            return 1;
        }

        case SYM_METODO:
        {
            int sameClassMet = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
            if (sameClassMet < 0)
            {
                return 0;
            }
            else if (sameClassMet == 0 && item->info.smb_metodo.confirmado == 0)
            {
                item->info.smb_metodo.confirmado = 1;
                return 0;
            }
            sm_errors += 1;
            return 1;
        }

        case SYM_VAR:
        {
            int sameClassVar = strcmp(classeOrigem, item->info.smb_metodo.classOrigem);
            if (sameClassVar < 0)
            {
                return 0;
            }
            sm_errors += 1;
            return 1;
        }
        }
    }
    return 0;
}

/* ==========================================
        VERIFICADOR DE VALIDADE DE CLASSE PAI
========================================== */

int verifyParent(const char *pai)
{
    if (strcmp(pai, "Int") == 0 || strcmp(pai, "String") == 0 || strcmp(pai, "Bool") == 0)
    {
        printf("Erro semântico: Não é permitido ter herança do tipo %s\n", pai);
        sm_errors += 1;
        return 0;
    }

    Symbol *tabela_it = tabela_classes;
    while (tabela_it != NULL)
    {
        if (strcmp(pai, tabela_it->nome) == 0)
        {
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
int checkProgram(ASTNode *node)
{
    ASTNode *atual = node;

    // Verificar se as classes são válidas e as adicionar à tabela de classes
    while (atual != NULL)
    {
        char *nome = atual->dados.classe.nome_classe;
        int nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL);
        int paiValido = verifyParent(atual->dados.classe.nome_pai);
        if (paiValido == 0)
        {
            atual->dados.classe.nome_pai = "Object";
        }
        if (nomeOcupado == 1)
        {
            printf("Erro semântico: Classe '%s' já existe\n", nome);
            int i = 0;
            char strnum[20];
            char *nomeOriginal = strdup(nome);
            while (TRUE)
            {
                sprintf(strnum, "%d", i);
                strcat(nome, strnum);
                nomeOcupado = searchItem(nome, tabela_classes, NODE_CLASSE, NULL);
                if (nomeOcupado == 0)
                {
                    break;
                }
                nome = nomeOriginal;
                i++;
            }
        }
        adicionar_symbol_classe(nome, atual->dados.classe.nome_pai, 1);

        // Verificar as features da classe
        checkFeatures(node->dados.classe.lista_features, nome);

        atual = atual->proximo;
    }

    if (sm_errors == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void checkFeatures(ASTNode *node, char *classeOrigem)
{
    ASTNode *atual = node;
    int nomeOcupado;
    while (atual != NULL)
    {
        switch (atual->tipo)
        {
        case NODE_ATRIBUTO:
        {
            char *nomeAttr = atual->dados.atributo.nome_atributo;
            nomeOcupado = searchItem(nomeAttr, tabela_atributos, atual->tipo, classeOrigem);
            if (nomeOcupado == 1)
            {
                printf("Erro semântico: Atributo '%s' já existe neste escopo.\n", nomeAttr);
            }

            ASTNode *init = atual->dados.atributo.inicializacao;
            char *tipoAttr = atual->dados.atributo.tipo_atributo;
            if (init != NULL)
            {
                const char *tipo_init = checkExpr(init, classeOrigem);
                if (isSubtype(tipo_init, tipoAttr) == 1)
                {
                    adicionar_symbol_atributo(nomeAttr, tipoAttr, classeOrigem, 1);
                }
                else
                {
                    printf("Erro semântico: Tipo da expressão não corresponde ao do atributo '%s'.\n", nomeAttr);
                }
            }
            else
            {
                adicionar_symbol_atributo(nomeAttr, tipoAttr, classeOrigem, 1);
            }
            break;
        }

        case NODE_METODO:
        {
            char *nomeMet = atual->dados.metodo.nome_metodo;
            nomeOcupado = searchItem(nomeMet, tabela_metodos, atual->tipo, classeOrigem);
            if (nomeOcupado == 1)
            {
                printf("Erro semântico: Método '%s' já existe neste escopo.\n", nomeMet);
            }

            ASTNode *corpo = atual->dados.metodo.corpo;
            char *tipoMet = atual->dados.metodo.tipo_retorno;
            if (strcmp(tipoMet, "SELF_TYPE") == 0)
            {
                tipoMet = classeOrigem;
            }

            if (corpo != NULL)
            {
                const char *tipo_corpo = checkExpr(corpo, classeOrigem);
                if (isSubtype(tipo_corpo, tipoMet) == 1)
                {
                    adicionar_symbol_metodo(nomeMet, tipoMet, classeOrigem, 1, atual->dados.metodo.lista_formais);
                }
                else
                {
                    printf("Erro semântico: Tipo da expressão '%s' não corresponde ao esperado pelo método (%s).\n",
                           tipo_corpo, tipoMet);
                }
            }
            else
            {
                adicionar_symbol_metodo(nomeMet, tipoMet, classeOrigem, 1, atual->dados.metodo.lista_formais);
            }
            break;
        }

        default:
        {
            sm_errors++;
            printf("Erro semântico: Tipo de feature inválido.\n");
            break;
        }
        }
        atual = atual->proximo;
    }
}
const char *checkExpr(ASTNode *node, char *classeOrigem)
{
    /* ---------- ATRIBUIÇÃO ---------- */
    if (node->tipo == NODE_ATRIBUICAO)
    {

        const char *valorType = checkExpr(node->dados.atribuicao.valor, classeOrigem);

        return valorType;
    }

    /* ---------- IF ---------- */
    else if (node->tipo == NODE_IF)
    {

        const char *condType =
            checkExpr(node->dados.no_if.condicao, classeOrigem);

        if (strcmp(condType, "Bool") != 0)
        {

            printf("Erro semântico: "
                   "condição de IF deve ser Bool\n");
            sm_errors += 1;
        }

        const char *thenType =
            checkExpr(node->dados.no_if.bloco_then, classeOrigem);

        const char *elseType =
            checkExpr(node->dados.no_if.bloco_else, classeOrigem);

        return leastCommonAncestor(thenType, elseType);
    }

    /* ---------- LET ---------- */
    else if (node->tipo == NODE_LET)
    {
        ASTNode *decl = node->dados.no_let.lista_variaveis;
        while (decl != NULL)
        {
            char *nome = decl->dados.let_var.nome_variavel;
            char *tipo = decl->dados.let_var.tipo_variavel;
            ASTNode *init = decl->dados.let_var.inicializacao;

            if (init != NULL)
            {
                const char *tipo_init = checkExpr(init, classeOrigem);
                if (isSubtype(tipo_init, tipo) == 0)
                {
                    printf("Erro semântico: inicialização de '%s' incompatível com tipo '%s'.\n",
                           nome, tipo);
                    sm_errors++;
                }
            }

            // Adiciona a variável do LET na tabela de símbolos
            adicionar_symbol_var(nome, tipo, classeOrigem);

            decl = decl->proximo;
        }

        // Avalia o corpo do LET
        return checkExpr(node->dados.no_let.corpo, classeOrigem);
    }

    /* ---------- CASE ---------- */
    else if (node->tipo == NODE_CASE)
    {
        ASTNode *branch = node->dados.no_case.lista_cases;
        const char *resultType = "Object";

        while (branch != NULL)
        {
            char *nome = branch->dados.case_branch.nome_variavel;
            char *tipo = branch->dados.case_branch.tipo_variavel;
            ASTNode *expr = branch->dados.case_branch.corpo;

            adicionar_symbol_var(nome, tipo, classeOrigem);

            const char *branchType = checkExpr(expr, classeOrigem);
            resultType = leastCommonAncestor(resultType, branchType);

            branch = branch->proximo;
        }

        return resultType;
    }

    /* ---------- WHILE ---------- */

    else if (node->tipo == NODE_WHILE)
    {
        const char *condType =
            checkExpr(node->dados.no_while.condicao, classeOrigem);

        if (strcmp(condType, "Bool") != 0)
        {

            printf("Erro semântico: "
                   "condição de WHILE deve ser Bool\n");
            sm_errors += 1;
        }

        checkExpr(node->dados.no_while.condicao, classeOrigem);

        return "Object";
    }

    /* ---------- BLOCO ---------- */

    else if (node->tipo == NODE_BLOCO)
    {
        const char *lastType = "Object";

        ASTNode *last = node->dados.bloco.lista_comandos;
        while (last->proximo != NULL)
        {
            last = last->proximo;
        }
        lastType =
            checkExpr(last, classeOrigem);

        return lastType;
    }

    /* ---------- SOMA, SUBTRAÇÃO, MULTIPLICAÇÃO E DIVISÃO ---------- */

    else if (node->tipo == NODE_ARITMETICO)
    {
        const char *left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        const char *right =
            checkExpr(node->dados.operacao_binaria.direito, classeOrigem);

        if (strcmp(left, "Int") != 0 ||
            strcmp(right, "Int") != 0)
        {

            printf("Erro semântico: "
                   "operação aritmetica requer Int\n");
            sm_errors += 1;
        }

        return "Int";
    }

    /* ---------- MENOR, MENOR OU IGUAL, OU IGUALDADE ---------- */

    else if (node->tipo == NODE_RELACIONAL)
    {
        const char *left =
            checkExpr(node->dados.operacao_binaria.esquerdo, classeOrigem);

        const char *right =
            checkExpr(node->dados.operacao_binaria.direito, classeOrigem);

        int leftInt = strcmp(left, "Int");
        int rightInt = strcmp(right, "Int");
        if (node->dados.operacao_binaria.operador == IGUAL)
        {
            int leftStr = strcmp(left, "String");
            int rightStr = strcmp(right, "String");
            int leftBool = strcmp(left, "Bool");
            int rightBool = strcmp(right, "Bool");

            if (leftInt != 0 || rightInt != 0 || leftStr != 0 || rightStr != 0 || leftBool != 0 || rightBool != 0)
            {
                if (strcmp(left, right) != 0)
                {
                    printf("Erro semântico: "
                           "tipos incompatíveis em igualdade\n");
                    sm_errors += 1;
                }
            }
        }
        else
        {
            if (leftInt != 0 || rightInt != 0)
            {
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
        const char *exprType = checkExpr(node->dados.dispatch.expressao_base, classeOrigem);

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            printf("Erro semântico: método '%s' não encontrado em '%s'.\n",
                   node->dados.dispatch.nome_metodo, exprType);
            sm_errors++;
            return "Object";
        }

        ASTNode *args = node->dados.dispatch.argumentos;
        Symbol *formais = metodo->info.smb_metodo.parametros; // <-- corrigido
        while (args != NULL && formais != NULL)
        {
            const char *argType = checkExpr(args, classeOrigem);
            if (isSubtype(argType, formais->info.smb_formal.tipo) == 0)
            {
                printf("Erro semântico: argumento incompatível no método '%s'.\n",
                       node->dados.dispatch.nome_metodo);
                sm_errors++;
            }
            args = args->proximo;
            formais = formais->next;
        }

        return metodo->info.smb_metodo.tipo_retorno;
    }

    /* ---------- DISPATCH EXPLÍCITO ---------- */
    else if (node->tipo == NODE_DISPATCH_EXPLICITO)
    {
        const char *exprType = checkExpr(node->dados.dispatch.expressao_base, classeOrigem);
        const char *targetType = node->dados.dispatch.tipo_estatico;

        if (isSubtype(exprType, targetType) == 0)
        {
            printf("Erro semântico: '%s' não é subtipo de '%s' em dispatch explícito.\n",
                   exprType, targetType);
            sm_errors++;
        }

        Symbol *metodo = buscar_simbolo(node->dados.dispatch.nome_metodo, tabela_metodos);
        if (metodo == NULL)
        {
            printf("Erro semântico: método '%s' não encontrado em '%s'.\n",
                   node->dados.dispatch.nome_metodo, targetType);
            sm_errors++;
            return "Object";
        }

        ASTNode *args = node->dados.dispatch.argumentos;
        Symbol *formais = metodo->info.smb_metodo.parametros; // <-- corrigido
        while (args != NULL && formais != NULL)
        {
            const char *argType = checkExpr(args, classeOrigem);
            if (isSubtype(argType, formais->info.smb_formal.tipo) == 0)
            {
                printf("Erro semântico: argumento incompatível no método '%s'.\n",
                       node->dados.dispatch.nome_metodo);
                sm_errors++;
            }
            args = args->proximo;
            formais = formais->next;
        }

        return metodo->info.smb_metodo.tipo_retorno;
    }
    /* ---------- NEW ---------- */

    else if (node->tipo == NODE_NEW)
    {
        if (node->dados.valor_lexico = "SELF_TYPE")
        {
            return classeOrigem;
        }
        else
        {
            return node->dados.valor_lexico;
        }
    }

    /* ---------- NOT ---------- */

    else if (node->tipo == NODE_NOT)
    {
        const char *type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        if (strcmp(type_expr, "Bool") < 0)
        {
            printf("Erro semântico: "
                   "operação NOT requer expressão booleana\n");
            sm_errors += 1;
        }

        return "Bool";
    }

    /* ---------- COMPLEMENTO ---------- */

    else if (node->tipo == NODE_COMPLEMENTO)
    {
        const char *type_expr = checkExpr(node->dados.operacao_unaria.expressao, classeOrigem);

        if (strcmp(type_expr, "Int") < 0)
        {
            printf("Erro semântico: "
                   "operação de complemento requer um Inteiro\n");
            sm_errors += 1;
        }

        return "Int";
    }

    /* ---------- ISVOID ---------- */

    else if (node->tipo == NODE_ISVOID)
    {
    }

    /* ---------- NÚMERO ---------- */

    else if (node->tipo == NODE_INTEIRO)
    {
        return "Int";
    }

    /* ---------- BOOLEANOS ---------- */

    else if (node->tipo == NODE_BOOLEANO)
    {
        return "Bool";
    }

    /* ---------- STRING ---------- */

    else if (node->tipo == NODE_STRING)
    {
        return "String";
    }

    /* ---------- FALLBACK ---------- */

    else
    {
        printf("Aviso: tipo desconhecido para %s\n",
               node->tipo);
        sm_errors += 1;
        return "Object";
    }
}