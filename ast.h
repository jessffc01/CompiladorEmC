#ifndef AST_H
#define AST_H

#include "lex.h" // Precisamos importar para usar o TokenType (os sinais de +, -, etc)

// 1. O CATÁLOGO DE PEÇAS (Enum)
// Uma lista de tudo o que o nosso compilador sabe construir
typedef enum {
    NODE_PROGRAMA,
    NODE_CLASSE,
    NODE_METODO,
    NODE_ATRIBUTO,
    NODE_FORMAL,    
    
    // Expressões
    NODE_IF,
    NODE_WHILE,
    NODE_LET,
    NODE_CASE,
    NODE_BLOCO,
    NODE_ATRIBUICAO,
    
    // Dispatch (Chamadas)
    NODE_DISPATCH_IMPLICITO,
    NODE_DISPATCH_EXPLICITO,

    NODE_LET_VAR,      
    NODE_CASE_BRANCH,
    
    // Matemática e Lógica
    NODE_ARITMETICO,
    NODE_RELACIONAL,
    NODE_NOT,
    NODE_NEW,
    NODE_ISVOID,
    NODE_COMPLEMENTO,
    
    
    // Valores (Folhas)
    NODE_INTEIRO,
    NODE_STRING,
    NODE_BOOLEANO,
    NODE_IDENTIFICADOR
} NodeType;

// 2. A ESTRUTURA PRINCIPAL (ASTNode)
typedef struct ASTNode {
    NodeType tipo;           // Diz o que essa caixa é (ex: NODE_IF)
    struct ASTNode* proximo; // Ponteiro mágico para listas (ex: o próximo argumento, a próxima linha do bloco)
    
    // A UNION: A caixinha se molda dependendo do tipo!
    // Se for um IF, ela usa a struct "no_if". Se for uma SOMA, usa a struct "operacao_binaria".
    union {
        // Para números, textos e variáveis
        char* valor_lexico;
        int valor_booleano;

        // Para matemática (+, -, *, /) e comparações (<, <=, =)
        struct {
            TokenTipo operador; // Guarda se foi MAIS, MENOS, MENOR, etc.
            struct ASTNode* esquerdo;
            struct ASTNode* direito;
        } operacao_binaria;

        // Para not, isvoid e ~
        struct {
            struct ASTNode* expressao;
        } operacao_unaria;

        // Para Atribuição (ex: hp <- 100)
        struct {
            char* nome_variavel;
            struct ASTNode* valor;
        } atribuicao;

        // Para o IF
        struct {
            struct ASTNode* condicao;
            struct ASTNode* bloco_then;
            struct ASTNode* bloco_else;
        } no_if;
        
        // Para o WHILE
        struct {
            struct ASTNode* condicao;
            struct ASTNode* corpo;
        } no_while;

        // Para blocos { }
        struct {
            struct ASTNode* lista_comandos;
        } bloco;

        // Para chamadas de método (Dispatch)
        struct {
            struct ASTNode* expressao_base; // Quem está chamando
            char* tipo_estatico;            // O @Classe (Pode ser NULL)
            char* nome_metodo;              // Qual método está chamando
            struct ASTNode* argumentos;     // Lista de argumentos passados
        } dispatch;

        // Para a Classe
        struct {
            char* nome_classe;
            char* nome_pai;
            struct ASTNode* lista_features;
        } classe;

        // Para Parâmetros (Formais)
        struct {
            char* nome_parametro;
            char* tipo_parametro;
        } formal;

        // Para Métodos
        struct {
            char* nome_metodo;
            char* tipo_retorno;
            struct ASTNode* lista_formais;
            struct ASTNode* corpo;
        } metodo;

        // Para Atributos
        struct {
            char* nome_atributo;
            char* tipo_atributo;
            struct ASTNode* inicializacao;
        } atributo;
        // Para o bloco LET
        struct {
            struct ASTNode* lista_variaveis;
            struct ASTNode* corpo;
        } no_let;

        // Para as Variáveis do LET (ex: x: Int <- 5)
        struct {
            char* nome_variavel;
            char* tipo_variavel;
            struct ASTNode* inicializacao;
        } let_var;

        // Para o bloco CASE
        struct {
            struct ASTNode* expressao_principal;
            struct ASTNode* lista_cases;
        } no_case;

        // Para os ramos do CASE (ex: x: Int => corpo)
        struct {
            char* nome_variavel;
            char* tipo_variavel;
            struct ASTNode* corpo;
        } case_branch;

    } dados; // Nome da nossa union

} ASTNode;

// Estruturas de LET e CASE
ASTNode* criar_no_let(ASTNode* lista_variaveis, ASTNode* corpo);
ASTNode* criar_no_let_var(char* nome, char* tipo, ASTNode* init);
ASTNode* criar_no_case(ASTNode* expressao_principal, ASTNode* lista_cases);
ASTNode* criar_no_case_branch(char* nome, char* tipo, ASTNode* corpo);

// Estruturas de Orientação a Objetos
ASTNode* criar_no_classe(char* nome_classe, char* nome_pai, ASTNode* lista_features);
ASTNode* criar_no_formal(char* nome_parametro, char* tipo_parametro);
ASTNode* criar_no_metodo(char* nome_metodo, char* tipo_retorno, ASTNode* lista_formais, ASTNode* corpo);
ASTNode* criar_no_atributo(char* nome_atributo, char* tipo_atributo, ASTNode* inicializacao);

// =========================================================================
// 3. AS FÁBRICAS DE NÓS (Protótipos das funções que usaremos no ast.c)
// =========================================================================

// Folhas
ASTNode* criar_no_inteiro(char* valor);
ASTNode* criar_no_string(char* valor);
ASTNode* criar_no_booleano(int valor);
ASTNode* criar_no_identificador(char* nome);

// Matemática e Lógica
ASTNode* criar_no_aritmetico(TokenTipo op, ASTNode* esq, ASTNode* dir);
ASTNode* criar_no_relacional(TokenTipo op, ASTNode* esq, ASTNode* dir);
ASTNode* criar_no_not(ASTNode* expr);
ASTNode* criar_no_new(char* tipo_novo);
ASTNode* criar_no_isvoid(ASTNode* expr);
ASTNode* criar_no_complemento(ASTNode* expr);

// Estruturas
ASTNode* criar_no_atribuicao(char* nome, ASTNode* valor);
ASTNode* criar_no_if(ASTNode* cond, ASTNode* bloco_then, ASTNode* bloco_else);
ASTNode* criar_no_while(ASTNode* cond, ASTNode* corpo);
ASTNode* criar_no_bloco(ASTNode* lista_comandos);

// Dispatch
ASTNode* criar_no_dispatch_implicito(ASTNode* nome_metodo, ASTNode* argumentos);
ASTNode* criar_no_dispatch_explicito(ASTNode* base, char* tipo_estatico, char* nome_metodo, ASTNode* args);

// Ferramentas de Lista
ASTNode* adicionar_comando(ASTNode* lista, ASTNode* novo_comando);
ASTNode* adicionar_argumento(ASTNode* lista, ASTNode* novo_argumento);

// Ferramentas Gerais
void imprimir_arvore(ASTNode* raiz, int nivel);
void liberar_arvore(ASTNode* raiz);





#endif // AST_H