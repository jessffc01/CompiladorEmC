#include "parser.h"
#include "semantic.h"

extern Token currentToken;

FILE *arquivo;

int main() {
    arquivo = fopen("arquivo.cl", "r");
    if (!arquivo) {
        printf("Erro ao abrir arquivo\n");
        return 1;
    }

    advance();
    ASTNode *root = program();

    if (errorCount > 0) {
        for (int i = 0; i < errorCount; i++) {
            printf("%s\n", errors[i]);
            free(errors[i]);
        }
        if(currentToken.tipo != EOF_TOKEN){
            printf("Erro: tokens restantes\n");
        }    
    }
    else {
        printf("Parsing realizado com sucesso!\n");
        
        printf("\n--- ÁRVORE SINTÁTICA ---\n");
        imprimir_arvore(root, 0); 
        
        int sm_errors = checkProgram(root); // chamada da análise semântica
        printf("----------");
        imprimir_tabelas();
        if(sm_errors == 0){
            printf("Analise semantica realizada com sucesso!\n");
        }
        else{
            printf("%d Erros encontrados na analise semantica.\n", sm_errors);
        }

        liberar_arvore(root);
    }

    fclose(arquivo);
    return 0;
}
