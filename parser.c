#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include <math.h>
#include "ast.h"

#define MAX_ERRORS 100

FILE *arquivo;
Token currentToken;
int modo_panico = 0;

Token nextToken;
int looked = 0;

char* errors[MAX_ERRORS];
int errorCount = 0;

// ================= ERRO =================
void reportError(const char* msg, int linha, int coluna) {
    if(modo_panico) return;
    modo_panico = 1;
    
    if (errorCount < MAX_ERRORS) {
        char buffer[200];
        snprintf(buffer, sizeof(buffer),
                 "Erro na linha %d, coluna %d: %s",
                 linha, coluna, msg);
        errors[errorCount++] = strdup(buffer);
    }
}

// ================= CONTROLE =================
Token look() {
    if (!looked) {
        nextToken = getNextToken(arquivo);
        looked = 1;
        while (currentToken.tipo == COMENTARIO) {
            currentToken = getNextToken(arquivo);
        }
    }
    return nextToken;
}

void advance() {
    if (looked) {
        currentToken = nextToken;
        looked = 0;
    }
    else{
        currentToken = getNextToken(arquivo);
    }

    while (currentToken.tipo == COMENTARIO) {
        currentToken = getNextToken(arquivo);
    }
}

// ================= RECUPERAÇÃO INTELIGENTE =================
void synchronize() {
    int profundidade_chaves = 0;

    // 1. Ponto de parada seguro imediato (Se o erro já aconteceu em cima de um delimitador)
    if (currentToken.tipo == SEMICOLON || currentToken.tipo == RBRACE || currentToken.tipo == EOF_TOKEN) {
        return;
    }

    // 2. O Submarino: Navegando pelas profundezas dos blocos
    while (currentToken.tipo != EOF_TOKEN) {
        
        if (currentToken.tipo == LBRACE) {
            profundidade_chaves++;
        } else if (currentToken.tipo == RBRACE) {
            profundidade_chaves--;
        }

        // ==========================================
        // A CORREÇÃO DE OURO (MENOR QUE ZERO)
        // Se a profundidade ficar negativa, significa que a chave '}' na esteira
        // pertence ao fechamento do método verdadeiro. É hora de parar e deixá-la aí!
        if (profundidade_chaves < 0) {
            return; 
        }
        // ==========================================

        // Se estamos no nível zero perfeito e achamos um ponto e vírgula, recomeça
        if (profundidade_chaves == 0 && currentToken.tipo == SEMICOLON) {
            return; 
        }

        // Se achamos uma palavra-chave segura no nível zero, recomeça
        if (profundidade_chaves == 0) {
            TokenTipo t = currentToken.tipo;
            if (t == CLASS || t == IF || t == WHILE || t == LET || t == CASE) {
                return; 
            }
        }

        advance(); // Engole o lixo silenciosamente
    }
}

void match(TokenTipo esperado) {
    if (currentToken.tipo == esperado) {
        modo_panico = 0;
        advance();
    } else {
        reportError("Token inesperado",
                    currentToken.location.linha,
                    currentToken.location.coluna);
        synchronize();
    }
}

// ================= PROTÓTIPOS =================
// ================= ESTRUTURAS PRINCIPAIS =================
ASTNode* program();
ASTNode* class();    // Lê uma classe (ou lista de classes)
ASTNode* feature();  // Lê métodos ou atributos
ASTNode* formal();   // Lê os parâmetros de um método
ASTNode* expr();     // O nosso grande trator principal
ASTNode* args();     // Lê as listas separadas por vírgula

// ================= EXPRESSÕES (A Escada Matemática) =================
ASTNode* expr_atrib();
ASTNode* expr_not();
ASTNode* expr_rel();
ASTNode* expr_arit();
ASTNode* term();
ASTNode* factor();
ASTNode* dispatch();
ASTNode* primary();

// ================= PROGRAMA =================
ASTNode* program() {
    ASTNode* lista_classes = NULL;
    while (currentToken.tipo == CLASS){
        //2 - chama a função class() e guarda o nó que vai construir
        ASTNode* nova_classe = class(); 
        //3 - engata a nova classe no fim usando ast.c
        lista_classes = adicionar_comando(lista_classes, nova_classe);
    } 
    return lista_classes;
}

// ================= CLASS =================
ASTNode* class() {
    advance();
    
    //clona o nome da classe antes de avançar
    char* nome_classe = strdup(currentToken.lexema);
    match(IDENTIFICADOR);

    //preparando o pai padrão ("Object")
    char* nome_pai = strdup("object");

    if(currentToken.tipo == INHERITS){
        advance();

        //se herdar de alguém, descartamos "Object"
        free(nome_pai);
        nome_pai = strdup(currentToken.lexema);
        match(IDENTIFICADOR);
    }
    match(LBRACE);

    //caso tenha mais de um atributos ou métodos
    ASTNode* lista_features = NULL;

    while(currentToken.tipo == IDENTIFICADOR){
        //lemos a feature e a colocamos no final
        ASTNode * nova_feature = feature();
        lista_features = adicionar_comando(lista_features,nova_feature);
    }

    match(RBRACE);
    match(SEMICOLON);

    return criar_no_classe(nome_classe, nome_pai, lista_features);
}

// ================= FORMAL =================
ASTNode* formal() {
    //salva o nome da variável antes de avançar
    char* nome_parametro = strdup(currentToken.lexema);
    match(IDENTIFICADOR);
    match(COLON);
    //salva o tipo
    char* tipo_parametro = strdup(currentToken.lexema);
    match(IDENTIFICADOR);
    return criar_no_formal(nome_parametro, tipo_parametro);
}

// ================= FEATURE =================

ASTNode* feature(){
    char* nome_feature = strdup(currentToken.lexema);
    match(IDENTIFICADOR);

    if(currentToken.tipo == LPAREN){
        //metodo
        advance(); 
        //preparação para lista de parâmetros;
        ASTNode* lista_formals = NULL;

        if(currentToken.tipo == IDENTIFICADOR){
            //guarda o primeiro parâmetro;
            lista_formals = adicionar_comando(lista_formals,formal());

            while(currentToken.tipo == COMMA){
                match(COMMA);
                //liga os próximos parâmetros
                lista_formals = adicionar_comando(lista_formals, formal());
            }
        }

        match(RPAREN);
        match(COLON);

        // Salvo o tipo de retorno do método int, string;
        char* tipo_retorno = strdup(currentToken.lexema);
        match(IDENTIFICADOR);

        // ==========================================
        // A REGRA DA PORTA FECHADA
        // ==========================================
        ASTNode* corpo_metodo = NULL;

        if (currentToken.tipo == LBRACE) {
            match(LBRACE);         // Entra no método
            corpo_metodo = expr(); // Lê os móveis (comandos)
            match(RBRACE);         // Sai do método
        } 
        else {
            // Dá o erro na letra 'a' e o synchronize pula tudo até parar no '}'
            match(LBRACE); 
            
            // ESSA É A LINHA MÁGICA: Consome o '}' para a esteira andar pro ';' !
            match(RBRACE); 
        }
        // ==========================================

        match(SEMICOLON); // Agora ele vai achar o ';' certinho e ser feliz!

        return criar_no_metodo(nome_feature, tipo_retorno, lista_formals, corpo_metodo);
    }else{
        //atributo;

        match(COLON);
        char* tipo_atributo = strdup(currentToken.lexema);
        match(IDENTIFICADOR);

        ASTNode* inicializacao = NULL;

        if(currentToken.tipo == ATRIBUI){
            match(ATRIBUI);

            inicializacao = expr();
        }
        
        match(SEMICOLON);
        

        return criar_no_atributo(nome_feature, tipo_atributo, inicializacao);
    }
}

// ================= EXPRESSÕES =================
ASTNode* expr(){
    //IF
    if(currentToken.tipo == IF){
        advance();
        ASTNode* condicao = expr();//pega a conta do if ex: a>b
        match(THEN);
        ASTNode* bloco_then = expr();//se for vdd
        match(ELSE);
        ASTNode* bloco_else = expr();
        match(FI);

        return criar_no_if(condicao, bloco_then, bloco_else);
    }
    //WHILE
    else if(currentToken.tipo == WHILE){
        match(WHILE);
        ASTNode* condicao = expr();//regra do laço
        match(LOOP);
        ASTNode* corpo = expr(); //pega a repetição;
        match(POOL);

        return criar_no_while(condicao, corpo);
    }

    else if(currentToken.tipo == LET){
        match(LET);

        //lista para todas as variaveis criadas no let;
        ASTNode* lista_variaveis = NULL;
        
        //primeira variável
        char* nome = strdup(currentToken.lexema);
        match(IDENTIFICADOR);
        match(COLON);
        char* tipo = strdup(currentToken.lexema);
        match(IDENTIFICADOR);

        ASTNode* init = NULL;
        if(currentToken.tipo == ATRIBUI){
            match(ATRIBUI);
            init = expr();
        }

        //primeira variável na lista;
        lista_variaveis = adicionar_comando(lista_variaveis, criar_no_let_var(nome, tipo, init));

        //proximas variáveis;
        while(currentToken.tipo == COMMA){
            match(COMMA);   
            char* prox_nome = strdup(currentToken.lexema);
            match(IDENTIFICADOR);
            match(COLON);
            char* prox_tipo = strdup(currentToken.lexema);
            match(IDENTIFICADOR);

            ASTNode* prox_init = NULL;
            if(currentToken.tipo == ATRIBUI){
                match(ATRIBUI);
                prox_init = expr();
            }
            lista_variaveis = adicionar_comando(lista_variaveis, criar_no_let_var(prox_nome, prox_tipo, prox_init));
        }
        match(IN);
        ASTNode* corpo_let = expr();//codigo que vai usar as variaveis;
        return criar_no_let(lista_variaveis, corpo_let);
    }
    //CASE
    else if(currentToken.tipo == CASE){
        match(CASE);
        ASTNode* expressao_principal = expr();
        match(OF);

        ASTNode* lista_cases = NULL;

        do{
            char* nome_case = strdup(currentToken.lexema);
            match(IDENTIFICADOR);
            match(COLON);
            char* tipo_case = strdup(currentToken.lexema);
            match(IDENTIFICADOR);
            match(AVALIA);// setinha =>
            ASTNode* corpo_case = expr();
            match(SEMICOLON);

            lista_cases = adicionar_comando(lista_cases, criar_no_case_branch(nome_case,tipo_case, corpo_case));
        }while(currentToken.tipo == IDENTIFICADOR);

        match(ESAC);

        return criar_no_case(expressao_principal, lista_cases);
    }
    //BLOCO
    else if(currentToken.tipo == LBRACE){
        match(LBRACE);
        ASTNode* lista_bloco = NULL;
        do{
            lista_bloco = adicionar_comando(lista_bloco, expr());
            match(SEMICOLON);
        }while(currentToken.tipo != RBRACE && currentToken.tipo != EOF_TOKEN);
        match(RBRACE);
        return criar_no_bloco(lista_bloco);
    }
    //new
    else if(currentToken.tipo == NEW){
        match(NEW);
        char* tipo_novo = strdup(currentToken.lexema);
        match(IDENTIFICADOR);
        return criar_no_new(tipo_novo);
    }

    // ================= DEFAULT =================
    else {
        // Se não for nenhuma palavra reservada, só pode ser uma conta ou chamada de função!
        return expr_atrib(); 
    }
}
//================= ATRIBUIÇÃO =================
ASTNode* expr_atrib(){
    if (currentToken.tipo == IDENTIFICADOR && look().tipo == ATRIBUI) {
        char* nome_var = strdup(currentToken.lexema);
        advance();
        advance();
        ASTNode* valor_direito = expr_atrib();
        return criar_no_atribuicao(nome_var, valor_direito);
    }
    return expr_not();
}

//================= NOT =================
ASTNode* expr_not(){
    
    int quantidade_not=0;
    
    while(currentToken.tipo == NOT){
        match(NOT);
        quantidade_not++;
    }
    ASTNode* expressao = expr_rel();

    for(int i=0; i<quantidade_not; i++){
        expressao = criar_no_not(expressao);
    }

    return expressao;
}

// ================= RELACIONAL =================
ASTNode* expr_rel() {
    //lê o lado esquerdo, ou a conta inteira;
    ASTNode* no_esquerdo = expr_arit();


    while (currentToken.tipo == MENOR ||
        currentToken.tipo == MENOROUIGUAL ||
        currentToken.tipo == MAIOR ||
        currentToken.tipo == MAIOROUIGUAL ||
        currentToken.tipo == IGUAL) {

        TokenTipo operador = currentToken.tipo;
        advance();

        ASTNode* no_direito = expr_arit();
        no_esquerdo = criar_no_relacional(operador,no_esquerdo, no_direito);
    }

    return no_esquerdo;
}

// ================= ARITMÉTICA =================
ASTNode* expr_arit() {
    ASTNode* no_esquerdo = term();

    while (currentToken.tipo == MAIS ||
        currentToken.tipo == MENOS) {

        TokenTipo operador = currentToken.tipo;
        advance();

        ASTNode* no_direito = term();
        no_esquerdo = criar_no_aritmetico(operador, no_esquerdo, no_direito);
    }

    return no_esquerdo;
}

ASTNode* term() {
    ASTNode* no_esquerdo = factor();

    while (currentToken.tipo == MULTIPLICACAO ||
        currentToken.tipo == DIVISAO) {

        TokenTipo operador = currentToken.tipo;
        advance();

        ASTNode* no_direito = factor();
        no_esquerdo = criar_no_aritmetico(operador, no_esquerdo, no_direito);
    }
    return no_esquerdo;
}

//FATOR
ASTNode* factor() {
    if(currentToken.tipo == ISVOID) {
        match(ISVOID);
        ASTNode* expressao = factor();
        return criar_no_isvoid(expressao);
    }
    else if(currentToken.tipo == COMPLEMENTO){
        match(COMPLEMENTO);
        ASTNode* expressao = factor();

        return criar_no_complemento(expressao);
    }
    else{
        return dispatch();
    }
}


ASTNode* dispatch(){
    ASTNode* expressao_base = primary();

    if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        ASTNode* argumentos = args();
        match(RPAREN);
        expressao_base = criar_no_dispatch_implicito(expressao_base, argumentos);
    }

    while(currentToken.tipo == DOT || currentToken.tipo == ARROBA) {
        
        char* tipo_estatico = NULL; // se houver, irá guardar
        if (currentToken.tipo == ARROBA) {
            match(ARROBA);
            tipo_estatico = strdup(currentToken.lexema);
            match(IDENTIFICADOR);
        }
        
        match(DOT);// lê o ponto
        char* nome_metodo = strdup(currentToken.lexema);
        match(IDENTIFICADOR);
        match(LPAREN);
        ASTNode* argumentos = args();
        match(RPAREN);

        expressao_base = criar_no_dispatch_explicito(expressao_base, tipo_estatico,nome_metodo, argumentos);
    }

    return expressao_base;
}

// ================= PRIMÁRIO (Os Átomos da Linguagem) =================
ASTNode* primary() {
    
  
    if (currentToken.tipo == NUMERO) {
        char* valor = strdup(currentToken.lexema);
        match(NUMERO);
        return criar_no_inteiro(valor);
    }
    
    
    else if (currentToken.tipo == IDENTIFICADOR) {
        char* nome = strdup(currentToken.lexema);
        match(IDENTIFICADOR);
        return criar_no_identificador(nome);
    }
    
    
    else if (currentToken.tipo == LPAREN) {
        match(LPAREN);
        // Pega o elevador de volta para o topo e lê toda a matemática de dentro!
        ASTNode* expressao_interna = expr();
        match(RPAREN);
        
        // Retorna a árvore gigante que estava dentro dos parênteses como se fosse um bloco só
        return expressao_interna;
    }
    
    
    else if (currentToken.tipo == STRING) {
        char* texto = strdup(currentToken.lexema);
        match(STRING);
        return criar_no_string(texto);
    }
    
  
    else if (currentToken.tipo == TRUE) {
        match(TRUE);
        return criar_no_booleano(1); // 1 representa true em C
    }
    else if (currentToken.tipo == FALSE) {
        match(FALSE);
        return criar_no_booleano(0); // 0 representa false em C
    }
    
    
    else {
        reportError("Fator inválido",
                    currentToken.location.linha,
                    currentToken.location.coluna);
        
        // O synchronize() tenta pular os tokens bugados até achar um ponto e vírgula para não travar o compilador inteiro!
        synchronize(); 
        
        // Retorna NULL porque o usuário digitou lixo onde deveria ter um número ou variável
        return NULL; 
    }
}
// ================= ARGUMENTOS =================
ASTNode* args() {
    
    // 1. Se não tem argumentos (já fechou o parêntese), não devolve árvore nenhuma
    if (currentToken.tipo == RPAREN) {
        return NULL;
    }

    // 2. Lê o primeiro argumento e cria a "locomotiva" da nossa lista
    ASTNode* lista_args = expr();

    // 3. Enquanto tiver VÍRGULA (,) na esteira...
    while (currentToken.tipo == COMMA) {
        match(COMMA);
        
        // Lê o próximo argumento e engata atrás do anterior
        lista_args = adicionar_argumento(lista_args, expr());
    }

    // 4. Devolve a lista inteira de argumentos amarradinha
    return lista_args;
}
// ================= MAIN =================
int main() {
    arquivo = fopen("arquivo.cl", "r");

    if (!arquivo) {
        printf("Erro ao abrir arquivo\n");
        return 1;
    }

    advance();
    
    // 1. A MUDANÇA PRINCIPAL: Capturamos a raiz da árvore inteira!
    ASTNode* arvore_sintatica = program();
    
    if (errorCount > 0 || currentToken.tipo != EOF_TOKEN) {
        for (int i = 0; i < errorCount; i++) {
            printf("%s\n", errors[i]);
            free(errors[i]);
        }
        if(currentToken.tipo != EOF_TOKEN){
            printf("Erro: tokens restantes\n");
        }
        
        printf("\n--- ÁRVORE SINTÁTICA COM ERRO DETECTADO---\n");
        imprimir_arvore(arvore_sintatica, 0); 
    }
    else {
        printf("Parsing realizado com sucesso!\n");
        
       // 2. O GRANDE TESTE: Imprimir a árvore no terminal para ver se funcionou!
        printf("\n--- ÁRVORE SINTÁTICA ---\n");
        imprimir_arvore(arvore_sintatica, 0); 
        
        // 3. LIMPEZA: Como programamos em C, temos que liberar a memória no final!
        liberar_arvore(arvore_sintatica);
    }

    fclose(arquivo);
    return 0;
}