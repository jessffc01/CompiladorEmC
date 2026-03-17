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
    EOF_TOKEN
} TokenTipo;
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

    return -1; // não é keyword
}

Token getNextToken(FILE *arquivo) {
    Token t;
    char c;
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
        int i = 0;

        while (isdigit(c)) {
            t.lexema[i++] = c;
            c = fgetc(arquivo);
        }
        t.lexema[i] = '\0';

        ungetc(c, arquivo); // devolve o último char

        t.tipo = NUMERO;
        return t;
    }
    if (c == '"') {
    int i = 0;
    c = fgetc(arquivo);

    while (c != '"' && c != EOF) {
        t.lexema[i++] = c;
        c = fgetc(arquivo);
    }
    t.lexema[i] = '\0';
    t.tipo = STRING;
    return t;
    }
    if (isalpha(c)) {
        int i = 0;

        while (isalnum(c)) {
            t.lexema[i++] = c;
            c = fgetc(arquivo);
        }
        t.lexema[i] = '\0';

        ungetc(c, arquivo);

        t.tipo = IDENTIFICADOR;
        return t;
    }
    if (c == '+') {
        t.tipo = MAIS;
        strcpy(t.lexema, "+");
        return t;
    }
    if (c == '-') {
        t.tipo = MENOS;
        strcpy(t.lexema, "-");
        return t;
    }
    if (c == '*') {
        t.tipo = MULTIPLICACAO;
        strcpy(t.lexema, "*");
        return t;
    }
    if (c == '/') {
        t.tipo = DIVISAO;
        strcpy(t.lexema, "/");
        return t;
    }
    if (c == '<') {
        char next = fgetc(arquivo);

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
    }
    if (c == '>') {
        char next = fgetc(arquivo);

        if (next == '=') {
            t.tipo = MAIOROUIGUAL;
            strcpy(t.lexema, ">=");
        } else {
            t.tipo = MAIOR;
            strcpy(t.lexema, ">");
            ungetc(next, arquivo);
        }
        return t;
    }
    if (c == '=') {
        t.tipo = IGUAL;
        strcpy(t.lexema, "=");
        return t;
    }
    if (c == '{') { 
        t.tipo = LBRACE;
         strcpy(t.lexema, "{");
          return t; 
        }
    if (c == '}') { 
        t.tipo = RBRACE; 
        strcpy(t.lexema, "}"); 
        return t;
     }
    if (c == '(') { 
        t.tipo = LPAREN; 
        strcpy(t.lexema, "("); 
        return t; 
    }
    if (c == ')') { 
        t.tipo = RPAREN; 
        strcpy(t.lexema, ")"); 
        return t; 
    }
    if (c == ';') { 
        t.tipo = SEMICOLON; 
        strcpy(t.lexema, ";"); 
        return t;
     }
    if (c == ':') { 
        t.tipo = COLON;
         strcpy(t.lexema, ":"); 
        return t; 
    }
    if (c == '.') { 
        t.tipo = DOT; strcpy(t.lexema, "."); 
        return t; 
    }
    if (c == ',') { 
        t.tipo = COMMA;
         strcpy(t.lexema, ","); 
        return t;
     }
    if (c == '@') { 
        t.tipo = ARROBA; 
        strcpy(t.lexema, "@"); 
        return t;
     }
 
    t.tipo = EOF_TOKEN;
    strcpy(t.lexema, "UNKNOWN");
    return t;
}

int main() {
    FILE *cool;

    cool = fopen("arquivo.cool", "r");

    if (cool == NULL) {
        printf("Erro ao abrir o arquivo\n");
        return 1;
    }

    printf("Arquivo pronto para leitura\n");

    Token t;

    do {
        t = getNextToken(cool);
        printf("Token: %d | Lexema: %s\n", t.tipo, t.lexema);
    } while (t.tipo != EOF_TOKEN);

    fclose(cool);

    return 0;
}