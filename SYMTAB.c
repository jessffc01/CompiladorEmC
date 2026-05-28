#include "symtab.h"

// O início do nosso caderno (A cabeça da lista encadeada)
// Começa vazio (NULL)
Symbol* tabela_classes;
Symbol* tabela_atributos;
Symbol* tabela_metodos;
Symbol* tabela_variaveis;

/* =========================================================================
   Init: Inicia as tabelas para as classes básicas.
   ========================================================================= */

void init_symbols(){
    tabela_classes = NULL;
    tabela_atributos = NULL;
    tabela_metodos = NULL;
    tabela_variaveis = NULL;
    adicionar_symbol_classe("Object", "", 1);
    adicionar_symbol_classe("Int", "Object", 1);
    adicionar_symbol_classe("Bool", "Object", 1);
    adicionar_symbol_classe("String", "Object", 1);
    adicionar_symbol_classe("IO", "Object", 1);

    adicionar_symbol_metodo("abort", "Object", "Object", 1, NULL);
    adicionar_symbol_metodo("type_name", "String", "Object", 1, NULL);
    adicionar_symbol_metodo("copy", "SELF_TYPE", "Object", 1, NULL);
    adicionar_symbol_metodo("out_string", "SELF_TYPE", "IO", 1, criar_no_formal("x", "String"));
    adicionar_symbol_metodo("out_int", "SELF_TYPE", "IO", 1, criar_no_formal("x", "Int"));
    adicionar_symbol_metodo("in_string", "String", "IO", 1, NULL);
    adicionar_symbol_metodo("in_int", "Int", "IO", 1, NULL);
    adicionar_symbol_metodo("length", "Int", "String", 1, NULL);
    adicionar_symbol_metodo("concat", "String", "String", 1, criar_no_formal("s", "String"));

    ASTNode* substr_param = criar_no_formal("i", "Int");
    substr_param->proximo = criar_no_formal("l", "Int");
    adicionar_symbol_metodo("substr", "String", "String", 1, substr_param);
}

/* =========================================================================
   BUSCAR: Procura uma variável pelo nome. 
   Retorna o ponteiro para ela se achar, ou NULL se ela não existir.
   ========================================================================= */
Symbol* buscar_simbolo(char* nome, Symbol* tabela_simbolos) {
    Symbol* atual = tabela_simbolos;
    
    // Percorre a lista vagão por vagão
    while (atual != NULL) {
        if (strcmp(atual->nome, nome) == 0) {
            return atual; // Achou!
        }
        atual = atual->next;
    }
    
    return NULL; // Chegou no fim da lista e não achou nada
}

/* =========================================================================
   INSERIR: Adiciona uma nova variável no caderno
   ========================================================================= */
Symbol* inserir_simbolo(char* nome, SymbolType tipo, /*int linha,*/ Symbol* tabela_simbolos, char* tipoParam) {
    // 1. Aloca espaço na memória para a nova variável
    Symbol* novo_simbolo = (Symbol*) malloc(sizeof(Symbol));
    if (!novo_simbolo) {
        printf("Erro fatal: Sem memoria para a Tabela de Simbolos!\n");
        exit(1);
    }

    // 2. Preenche a ficha da variável
    novo_simbolo->nome = strdup(nome);
    novo_simbolo->tipo_simbolo = tipo;
    //novo_simbolo->linha_declaracao = linha;

    if(tipo != SYM_FORMAL){
    // 3. Insere no INÍCIO da lista (é mais rápido e fácil em C)
        novo_simbolo->next = tabela_simbolos;
        tabela_simbolos = novo_simbolo;
    }
    else{
    // Insere no final da lista para manter a ordem dos parâmetros
        novo_simbolo->info.smb_formal.tipo = tipoParam;
        Symbol* back = tabela_simbolos;
        while(back->next != NULL){
            back = back->next;
        }
        back->next = novo_simbolo;
    }

     return tabela_simbolos;
}

void adicionar_symbol_classe(char* nome, char* pai, int confirmado){
    if(pai == NULL && strcmp(nome, "Object") < 0){
        pai = "Object";
    }
    tabela_classes = inserir_simbolo(nome, SYM_CLASSE, tabela_classes, NULL);
    tabela_classes->info.smb_classe.parentName = strdup(pai);
    tabela_classes->info.smb_classe.confirmado = confirmado;
}

void adicionar_symbol_metodo(char* nome, char* retorno, char* classOrigem, int confirmado, ASTNode* lista_parametros){
    tabela_metodos = inserir_simbolo(nome, SYM_METODO, tabela_metodos, NULL);
    tabela_metodos->info.smb_metodo.tipo_retorno = strdup(retorno);
    tabela_metodos->info.smb_metodo.classOrigem = strdup(classOrigem);
    tabela_metodos->info.smb_metodo.confirmado = confirmado;

    Symbol* parametros = NULL;
    
    ASTNode* parametro_it = lista_parametros;
    while(parametro_it != NULL){
        parametros = inserir_simbolo(parametro_it->dados.formal.nome_parametro, SYM_FORMAL, parametros, parametro_it->dados.formal.tipo_parametro);
        parametro_it = parametro_it->proximo;
    }

    tabela_metodos->info.smb_metodo.parametros = parametros;
}

void adicionar_symbol_atributo(char* nome, char* tipo, char* classOrigem, int confirmado){
    tabela_atributos = inserir_simbolo(nome, SYM_ATRIBUTO, tabela_atributos, NULL);
    tabela_atributos->info.smb_atributo.tipo = strdup(tipo);
    tabela_atributos->info.smb_atributo.classOrigem = strdup(classOrigem);
    tabela_atributos->info.smb_atributo.confirmado = confirmado;
}

void adicionar_symbol_var(char* nome, char* tipo, char* classOrigem){
    tabela_variaveis = inserir_simbolo(nome, SYM_VAR, tabela_variaveis, NULL);
    tabela_variaveis->info.smb_var.tipo = strdup(tipo);
    tabela_variaveis->info.smb_var.classOrigem = strdup(classOrigem);
}

/* =========================================================================
   IMPRIMIR: Mostra todas as variáveis salvas (Para debug visual)
   ========================================================================= */
void imprimir_tabela(Symbol* tabela_simbolos) {
    Symbol* atual = tabela_simbolos;
    
    if (atual == NULL) {
        printf("Tabela vazia.\n");
    }
    
    while (atual != NULL) {
        printf("Variavel: %s | Tipo: %s", atual->nome, atual->tipo_simbolo);
        atual = atual->next;
    }
    printf("--------------------------\n\n");
}

void imprimir_tabelas(){
    printf("\n--- TABELA DE CLASSES ---\n");
    imprimir_tabela(tabela_classes);
    printf("\n--- TABELA DE ATRIBUTOS ---\n");
    imprimir_tabela(tabela_atributos);
    printf("\n--- TABELA DE METODOS ---\n");    
    imprimir_tabela(tabela_metodos);
    printf("\n--- TABELA DE VARIAVEIS ---\n");
    imprimir_tabela(tabela_variaveis);
}
