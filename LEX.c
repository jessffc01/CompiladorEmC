#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

typedef enum {
    NUMERO,
    IDENTIFICADOR,
    MAIS,
    MENOS,
    DIVISAO,
    MULTIPLICACAO,
    MENOROUIGUAL,
    MAIOROUIGUAL,
    MAIOR,
    MENOR,
    IGUAL,
    ATRIBUI,
    CLASS,
    INHERITS,
    IF,
    THEN,
    ELSE,
    FI,
    WHILE,
    LOOP,
    POOL,
    LET,
    IN,
    CASE,
    OF,
    ESAC,
    NEW,
    ISVOID,
    NOT,
    TRUE,
    FALSE,
    LBRACE,
    RBRACE,
    LPAREN, 
    RPAREN, 
    SEMICOLON,
    COLON, 
    DOT, 
    COMMA, 
    ARROBA,
    STRING,
    EOF_TOKEN,
    COMENTARIO
} TokenTipo;

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

typedef struct {
    char *palavra;
    TokenTipo tipo;
} Keyword;

typedef struct {
    TokenTipo tipo;
    char lexema[100];
} Token;

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
    
    while (isspace(c)) {
        c = fgetc(arquivo);
    }

    if (c == EOF) {
        t.tipo = EOF_TOKEN;
        strcpy(t.lexema, "EOF");
        return t;
    }

    if (isdigit(c)) {
        while (isdigit(c)) {
            t.lexema[i++] = c;
            c = fgetc(arquivo);
        }
        t.lexema[i] = '\0';

        ungetc(c, arquivo); // devolve o último char

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

        t.tipo = verificaPalavraReservada(t.lexema);
        return t;
    }

    switch (c){
        case '"':
            c = fgetc(arquivo);

            while (c != '"' && c != EOF) {
                t.lexema[i++] = c;
                c = fgetc(arquivo);
            }

            t.lexema[i] = '\0';
            t.tipo = STRING;
            return t;
            break;
        
        case '+':
            t.tipo = MAIS;
            strcpy(t.lexema, "+");
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
                return t;    
            }
            else{
                ungetc(next, arquivo);
                t.tipo = MENOS;
                strcpy(t.lexema, "-");
                return t;
            }
            break;
        
        case '*':
            t.tipo = MULTIPLICACAO;
            strcpy(t.lexema, "*");
            return t;
            break;
        
        case '/':
            t.tipo = DIVISAO;
            strcpy(t.lexema, "/");
            return t;
            break;    
        
        case '<':    
            next = fgetc(arquivo);

            if (next == '=') {
                t.tipo = MENOROUIGUAL;
                strcpy(t.lexema, "<=");
            } else if (next == '-') {
                t.tipo = ATRIBUI;
                strcpy(t.lexema, "<-");
            } else {
                t.tipo = MENOR;
                strcpy(t.lexema, "<");
                ungetc(next, arquivo);
            }
            return t;
            break;

        case '>':
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
            break;

        case '=':
            t.tipo = IGUAL;
            strcpy(t.lexema, "=");
            return t;
            break;
        
        case '{': 
            t.tipo = LBRACE;
            strcpy(t.lexema, "{");
            return t;
            break; 

        case '}': 
            t.tipo = RBRACE; 
            strcpy(t.lexema, "}"); 
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
                    }
                    else{
                        ungetc(next, arquivo);
                        t.lexema[i] = '\0';
                        t.tipo = COMENTARIO;
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
            return t;
            break; 
        
        case ')':
            t.tipo = RPAREN; 
            strcpy(t.lexema, ")"); 
            return t; 
            break;

        case ';': 
            t.tipo = SEMICOLON; 
            strcpy(t.lexema, ";"); 
            return t;
            break;

        case ':':
            t.tipo = COLON;
            strcpy(t.lexema, ":"); 
            return t; 
            break;

        case '.':
            t.tipo = DOT; strcpy(t.lexema, "."); 
            return t; 
            break;

        case ',':
            t.tipo = COMMA;
            strcpy(t.lexema, ","); 
            return t;
            break;

        case '@':
            t.tipo = ARROBA; 
            strcpy(t.lexema, "@"); 
            return t;
            break;
            
        default:
            t.tipo = EOF_TOKEN;
            strcpy(t.lexema, "UNKNOWN");
            return t;
    }
}

int main() {
    FILE *cool;

    cool = fopen("arquivo.cl", "r");

    if (cool == NULL) {
        printf("Erro ao abrir o arquivo\n");
        return 1;
    }

    printf("Arquivo pronto para leitura\n");

    Token t;

    do {
        t = getNextToken(cool);
        printf("Token: %s | Lexema: %s\n", strTipos[t.tipo], t.lexema);
    } while (t.tipo != EOF_TOKEN);

    fclose(cool);

    return 0;
}