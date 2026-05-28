#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#ifndef SYMTAB_H
#define SYMTAB_H

typedef enum {
    SYM_CLASSE,
    SYM_ATRIBUTO,
    SYM_METODO,
    SYM_FORMAL,
    SYM_VAR
} SymbolType;

// A estrutura de uma "linha" no nosso caderno de variáveis
typedef struct Symbol {
    char* nome;             // Nome da variável (ex: "x", "contador")
    SymbolType tipo_simbolo;  
    //int linha_declaracao;   // Em qual linha do código ela nasceu?
        
    union {
        struct {
            char* tipo;
            char* classOrigem;
        } smb_var;

        struct {
            char* parentName;
            int confirmado;
        } smb_classe;
       
        struct {
            char* tipo;
            char* classOrigem;
            int confirmado;
        } smb_atributo;

        struct {
            char* tipo_retorno;
            char* classOrigem;
            struct Symbol *parametros;
            int confirmado;
        } smb_metodo;

        struct {
            char* tipo;
        } smb_formal;

    } info; // Nome da nossa union

    struct Symbol* next;    // O gancho para a próxima variável da lista
} Symbol;

// Ferramentas para manipular o caderno
void init_symbols();
Symbol *inserir_simbolo(char* nome, SymbolType tipo, /*int linha,*/ Symbol *tabela_simbolos, char* tipoParam);

void adicionar_symbol_classe(char* nome, char* pai, int confirmado);
void adicionar_symbol_metodo(char* nome, char* retorno, char* classOrigem, int confirmado, ASTNode* lista_parametros);
void adicionar_symbol_atributo(char* nome, char* tipo, char* classOrigem, int confirmado);
void adicionar_symbol_var(char* nome, char* tipo, char* classOrigem);

Symbol *buscar_simbolo(char* nome, Symbol *tabela_simbolos);
void imprimir_tabelas();
void imprimir_tabela(Symbol* tabela_simbolos);

#endif