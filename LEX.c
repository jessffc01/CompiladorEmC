#include "LEX.h"

int linha = 1;
int coluna = 1;

const char* strTipos[] = {
    "NUMERO",
    "IDENTIFICADOR",
    "MAIS",
    "MENOS",
    "DIVISAO",
    "MULTIPLICACAO",
    "MENOROUIGUAL",
    "MAIOROUIGUAL",
    "MAIOR",
    "MENOR",
    "IGUAL",
    "ATRIBUI",
    "CLASS",
    "INHERITS",
    "IF",
    "THEN",
    "ELSE",
    "FI",
    "WHILE",
    "LOOP",
    "POOL",
    "LET",
    "IN",
    "CASE",
    "OF",
    "ESAC",
    "NEW",
    "ISVOID",
    "NOT",
    "TRUE",
    "FALSE",
    "LBRACE",
    "RBRACE",
    "LPAREN", 
    "RPAREN", 
    "SEMICOLON",
    "COLON", 
    "DOT", 
    "COMMA", 
    "ARROBA",
    "STRING",
    "EOF_TOKEN",
    "COMENTARIO"
};

Keyword keywords[] = {
    {"class", CLASS},
    {"inherits", INHERITS},
    {"if", IF},
    {"then", THEN},
    {"else", ELSE},
    {"fi", FI},
    {"while", WHILE},
    {"loop", LOOP},
    {"pool", POOL},
    {"let", LET},
    {"in", IN},
    {"case", CASE},
    {"of", OF},
    {"esac", ESAC},
    {"new", NEW},
    {"isvoid", ISVOID},
    {"not", NOT},
    {"true", TRUE},
    {"false", FALSE}
};

TokenTipo verificaPalavraReservada(char *lexema) {
    int tamanho = sizeof(keywords) / sizeof(Keyword);

    for (int i = 0; i < tamanho; i++) {
        if (strcmp(lexema, keywords[i].palavra) == 0) {
            return keywords[i].tipo;
        }
    }

    return IDENTIFICADOR; // não é keyword
}

Token getNextToken(FILE *arquivo) {
    Token t;
    char c, next;
    int i = 0;
    c = fgetc(arquivo);

    while (isspace(c)){
        if(c == '\n'){
            linha += 1;
            coluna = 1;
        }
        else{
            coluna += 1;
        }
        c = fgetc(arquivo);
    }

    if (c == EOF) {
        t.tipo = EOF_TOKEN;
        strcpy(t.lexema, "EOF");
        t.location.linha = linha;
        t.location.coluna = coluna;
        return t;
    }

    if (isdigit(c)) {
        while (isdigit(c)) {
            t.lexema[i++] = c;
            c = fgetc(arquivo);
        }
        t.lexema[i] = '\0';

        ungetc(c, arquivo); // devolve o último char
        t.location.linha = linha;
        t.location.coluna = coluna;
        coluna += i;
        t.tipo = NUMERO;
        return t;
    }

    if (isalpha(c)) {
        while (isalnum(c) || (c == '_')) {
            t.lexema[i++] = c;
            c = fgetc(arquivo);
        }
        t.lexema[i] = '\0';

        ungetc(c, arquivo);
        t.location.linha = linha;
        t.location.coluna = coluna;
        coluna += i;
        t.tipo = verificaPalavraReservada(t.lexema);
        return t;
    }

    int crossLines = 0;
    int colCount;
    int linhaCount = 0;
    switch (c){
        case '"':
            c = fgetc(arquivo);
            while (c != '"' && c != EOF) {
                t.lexema[i++] = c;
                c = fgetc(arquivo);
                if(c == '\n'){
                    crossLines = 1;
                    linhaCount += 1;
                    colCount = 0;
                }
                if(crossLines == 1){
                    colCount += 1;
                }
            }

            t.lexema[i] = '\0';
            t.location.linha = linha;
            t.location.coluna = coluna;
            linha += linhaCount;
            if(crossLines == 0){
                coluna += i;
            }
            else{
                coluna = colCount;
            }
            t.tipo = STRING;
            return t;
            break;
        
        case '+':
            t.tipo = MAIS;
            strcpy(t.lexema, "+");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break;
        
        case '-':
            next = fgetc(arquivo); 

            if (next == '-') { //Dois traços indicam um comentário
                t.lexema[i++] = c;
                while (next != '\n' && next != EOF) {
                    t.lexema[i++] = next;
                    next = fgetc(arquivo);
                }
                ungetc(next, arquivo);
                t.lexema[i] = '\0';
                t.tipo = COMENTARIO;
                t.location.linha = linha;
                t.location.coluna = coluna;
                coluna += i;
                return t;    
            }
            else{
                ungetc(next, arquivo);
                t.tipo = MENOS;
                strcpy(t.lexema, "-");
                t.location.linha = linha;
                t.location.coluna = coluna;
                coluna += 1;
                return t;
            }
            break;
        
        case '*':
            t.tipo = MULTIPLICACAO;
            strcpy(t.lexema, "*");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break;
        
        case '/':
            t.tipo = DIVISAO;
            strcpy(t.lexema, "/");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break;    
        
        case '<':    
            next = fgetc(arquivo);
            t.location.linha = linha;
            t.location.coluna = coluna;
            if (next == '=') {
                t.tipo = MENOROUIGUAL;
                strcpy(t.lexema, "<=");
                coluna += 2;
            } else if (next == '-') {
                t.tipo = ATRIBUI;
                strcpy(t.lexema, "<-");
                coluna += 2;
            } else {
                t.tipo = MENOR;
                strcpy(t.lexema, "<");
                ungetc(next, arquivo);
                coluna += 1;
            }
            return t;
            break;

        /*case '>':
            next = fgetc(arquivo);

            if (next == '=') {
                t.tipo = MAIOROUIGUAL;
                strcpy(t.lexema, ">=");
            } else {
                t.tipo = MAIOR;
                strcpy(t.lexema, ">");
                ungetc(next, arquivo);
            }
            return t;
            break;*/

        case '=':
            t.tipo = IGUAL;
            strcpy(t.lexema, "=");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break;
        
        case '{': 
            t.tipo = LBRACE;
            strcpy(t.lexema, "{");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break; 

        case '}': 
            t.tipo = RBRACE; 
            strcpy(t.lexema, "}");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1; 
            return t;
            break;
        
        case '(':
            int comms = 0;
            next = fgetc(arquivo); 
            int abrir = 1;
            int fechar = 0;
            int init = 1;
            do{    //Loop para caso se trate de um ou mais comentários feitos com (* ... *), que podem ser de várias linhas e aninhados
                if (next == '*') {
                    if(abrir == 1){
                        comms += 1;
                    }
                    else{
                        fechar = 1;

                    }     
                }

                abrir = 0;

                if(comms>0){

                    if (init == 1){
                        t.lexema[i++] = c;
                        init = 0;
                    }
                    
                    if (next != EOF) {
                        t.lexema[i++] = next;
                        next = fgetc(arquivo);
                        if(next == '\n'){
                            crossLines = 1;
                            linhaCount += 1;
                            colCount = 0;
                        }
                        if(crossLines == 1){
                            colCount += 1;
                        }
                    }
                    else{
                        ungetc(next, arquivo);
                        t.lexema[i] = '\0';
                        t.tipo = COMENTARIO;
                        t.location.linha = linha;
                        t.location.coluna = coluna;
                        linha += linhaCount;
                        if(crossLines == 0){
                            coluna += i;
                        }
                        else{
                            coluna = colCount;
                        }
                        return t;
                    }

                    if (next == '(') {
                        int abrir = 1;
                    }

                    if (fechar == 1) {
                        if(next == ')'){
                            comms -= 1;
                            fechar = 0;
                            if(comms == 0){
                                t.lexema[i++] = next;
                                t.lexema[i] = '\0';
                                t.tipo = COMENTARIO;
                                t.location.linha = linha;
                                t.location.coluna = coluna;
                                linha += linhaCount;
                                if(crossLines == 0){
                                    coluna += i;
                                }
                                else{
                                    coluna = colCount;
                                }
                                return t;
                            }
                        }
                        else{
                            fechar = 0;
                        }
                    }
                }
            } while(comms>0);

            //Caso se trate apenas de um parêntese comum
            ungetc(next, arquivo); 
            t.tipo = LPAREN; 
            strcpy(t.lexema, "("); 
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
            break; 
        
        case ')':
            t.tipo = RPAREN; 
            strcpy(t.lexema, ")"); 
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t; 
            break;

        case ';': 
            t.tipo = SEMICOLON; 
            strcpy(t.lexema, ";");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1; 
            return t;
            break;

        case ':':
            t.tipo = COLON;
            strcpy(t.lexema, ":"); 
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t; 
            break;

        case '.':
            t.tipo = DOT; 
            strcpy(t.lexema, "."); 
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t; 
            break;

        case ',':
            t.tipo = COMMA;
            strcpy(t.lexema, ",");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1; 
            return t;
            break;

        case '@':
            t.tipo = ARROBA; 
            strcpy(t.lexema, "@");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1; 
            return t;
            break;
            
        default:
            t.tipo = EOF_TOKEN;
            strcpy(t.lexema, "UNKNOWN");
            t.location.linha = linha;
            t.location.coluna = coluna;
            coluna += 1;
            return t;
    }
}

TokenList* new_item(Token t){
    TokenList* node = (TokenList*)malloc(sizeof(TokenList));
    node->token = t;
    node->next = NULL;
    return node;
}
TokenList* push_back(TokenList* head, Token t){
    TokenList* node = new_item(t);
    if(head == NULL){
        return node;
    }

    TokenList* back = head;
    while(back->next!=NULL){
        back = back->next;
    }

    back->next = node;
    return head;
}

void free_list(TokenList* head){
    if(head!=NULL){
        free_list(head->next);
        free(head);
    }
}