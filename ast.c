#include "ast.h"

// =========================================================================
// FUNÇÃO AJUDANTE (Uso interno da fábrica)
// =========================================================================
ASTNode* criar_no_base(NodeType tipo) {
    // 1. Pede espaço na memória RAM do tamanho exato da nossa Union
    ASTNode* novo_no = (ASTNode*)malloc(sizeof(ASTNode));
    
    // 2. Trava de segurança padrão do C
    if (novo_no == NULL) {
        printf("Erro fatal: Memória RAM insuficiente para a Árvore Sintática!\n");
        exit(1); 
    }
    
    // 3. Preenche as informações que todo nó tem em comum
    novo_no->tipo = tipo;
    novo_no->proximo = NULL; // O vagão começa desengatado por padrão
    
    return novo_no;
}

// =========================================================================
// ESTRUTURAS DE LET E CASE
// =========================================================================

ASTNode* criar_no_let(ASTNode* lista_variaveis, ASTNode* corpo) {
    ASTNode* no = criar_no_base(NODE_LET);
    no->dados.no_let.lista_variaveis = lista_variaveis;
    no->dados.no_let.corpo = corpo;
    return no;
}

ASTNode* criar_no_let_var(char* nome, char* tipo, ASTNode* init) {
    ASTNode* no = criar_no_base(NODE_LET_VAR);
    no->dados.let_var.nome_variavel = nome;
    no->dados.let_var.tipo_variavel = tipo;
    no->dados.let_var.inicializacao = init; // Pode ser NULL se não tiver "<-"
    return no;
}

ASTNode* criar_no_case(ASTNode* expressao_principal, ASTNode* lista_cases) {
    ASTNode* no = criar_no_base(NODE_CASE);
    no->dados.no_case.expressao_principal = expressao_principal;
    no->dados.no_case.lista_cases = lista_cases;
    return no;
}

ASTNode* criar_no_case_branch(char* nome, char* tipo, ASTNode* corpo) {
    ASTNode* no = criar_no_base(NODE_CASE_BRANCH);
    no->dados.case_branch.nome_variavel = nome;
    no->dados.case_branch.tipo_variavel = tipo;
    no->dados.case_branch.corpo = corpo;
    return no;
}

// =========================================================================
// ESTRUTURAS DE ORIENTAÇÃO A OBJETOS (Classes, Métodos, Atributos)
// =========================================================================

ASTNode* criar_no_classe(char* nome_classe, char* nome_pai, ASTNode* lista_features) {
    ASTNode* no = criar_no_base(NODE_CLASSE);
    no->dados.classe.nome_classe = nome_classe;
    no->dados.classe.nome_pai = nome_pai;
    no->dados.classe.lista_features = lista_features;
    return no;
}

ASTNode* criar_no_formal(char* nome_parametro, char* tipo_parametro) {
    ASTNode* no = criar_no_base(NODE_FORMAL);
    no->dados.formal.nome_parametro = nome_parametro;
    no->dados.formal.tipo_parametro = tipo_parametro;
    return no;
}

ASTNode* criar_no_metodo(char* nome_metodo, char* tipo_retorno, ASTNode* lista_formais, ASTNode* corpo) {
    ASTNode* no = criar_no_base(NODE_METODO);
    no->dados.metodo.nome_metodo = nome_metodo;
    no->dados.metodo.tipo_retorno = tipo_retorno;
    no->dados.metodo.lista_formais = lista_formais;
    no->dados.metodo.corpo = corpo;
    return no;
}

ASTNode* criar_no_atributo(char* nome_atributo, char* tipo_atributo, ASTNode* inicializacao) {
    ASTNode* no = criar_no_base(NODE_ATRIBUTO);
    no->dados.atributo.nome_atributo = nome_atributo;
    no->dados.atributo.tipo_atributo = tipo_atributo;
    no->dados.atributo.inicializacao = inicializacao;
    return no;
}


ASTNode* criar_no_inteiro(char* valor) {
    // 1. Chama o nosso "ajudante" para alocar a memória e definir o tipo
    ASTNode* no = criar_no_base(NODE_INTEIRO);
    
    // 2. Guarda o texto do número dentro do espaço correto da nossa union
    no->dados.valor_lexico = valor;
    
    // 3. Devolve a caixinha pronta!
    return no;
}

ASTNode* criar_no_string(char* valor) {
    // 1. Aloca a memória e define o tipo como STRING
    ASTNode* no = criar_no_base(NODE_STRING);
    
    // 2. Guarda o texto da string (que já vem com aspas do seu lexer)
    no->dados.valor_lexico = valor;
    
    // 3. Devolve a caixinha pronta
    return no;
}

ASTNode* criar_no_booleano(int valor) {
    // 1. Aloca a memória e define o tipo como BOOLEANO
    ASTNode* no = criar_no_base(NODE_BOOLEANO);
    
    // 2. Guarda o valor numérico (1 ou 0) na gaveta correta da union
    no->dados.valor_booleano = valor;
    
    // 3. Devolve a caixinha pronta
    return no;
}

ASTNode* criar_no_identificador(char* nome) {
    // 1. Aloca a memória e define o tipo como IDENTIFICADOR
    ASTNode* no = criar_no_base(NODE_IDENTIFICADOR);
    
    // 2. Guarda o nome da variável na mesma gaveta de texto da union
    no->dados.valor_lexico = nome;
    
    // 3. Devolve a caixinha pronta
    return no;
}


// =========================================================================
// MATEMÁTICA E LÓGICA (Operações)
// =========================================================================
ASTNode* criar_no_aritmetico(TokenTipo op, ASTNode* esq, ASTNode *dir){
    // 1. Aloca a memória e define o tipo como ARITMÉTICO
    ASTNode* no = criar_no_base(NODE_ARITMETICO);
    
    // 2. Preenche a gaveta "operacao_binaria" da nossa union
    no->dados.operacao_binaria.operador = op;   // Salva qual foi o sinal (+, -, *, /)
    no->dados.operacao_binaria.esquerdo = esq;  // Segura a caixinha da esquerda
    no->dados.operacao_binaria.direito = dir;   // Segura a caixinha da direita
    
    // 3. Devolve o pacote montado
    return no;
}

ASTNode* criar_no_relacional(TokenTipo op, ASTNode* esq, ASTNode* dir) {
    // 1. Aloca a memória e define o tipo como RELACIONAL
    ASTNode* no = criar_no_base(NODE_RELACIONAL);
    
    // 2. Preenche a mesma gaveta "operacao_binaria" usada na matemática
    no->dados.operacao_binaria.operador = op;   // Salva o sinal (<, <=, =, etc)
    no->dados.operacao_binaria.esquerdo = esq;  // Segura o que vem antes do sinal
    no->dados.operacao_binaria.direito = dir;   // Segura o que vem depois do sinal
    
    // 3. Devolve a balança montada
    return no;
}


ASTNode* criar_no_not(ASTNode* expr) {
    // 1. Aloca a memória e define o tipo como NOT
    ASTNode* no = criar_no_base(NODE_NOT);
    
    // 2. Preenche a gaveta "operacao_unaria" da nossa union
    no->dados.operacao_unaria.expressao = expr;  // Segura a caixinha que está sendo negada
    
    // 3. Devolve a estrutura montada
    return no;
}

ASTNode* criar_no_new(char* tipo_novo) {
    // 1. Aloca a memória e define o crachá como NEW
    ASTNode* no = criar_no_base(NODE_NEW);
    
    // 2. Guarda o nome da classe usando a nossa gaveta de texto padrão
    no->dados.valor_lexico = tipo_novo; 
    
    // 3. Devolve a estrutura pronta
    return no;
}

ASTNode* criar_no_isvoid(ASTNode* expr) {
    // 1. Aloca a memória e define o crachá como ISVOID
    ASTNode* no = criar_no_base(NODE_ISVOID);
    
    // 2. Preenche a mesma gaveta de operações de 1 lado só
    no->dados.operacao_unaria.expressao = expr;  // Segura a variável ou objeto que será testado
    
    // 3. Devolve a estrutura pronta
    return no;
}

ASTNode* criar_no_complemento(ASTNode* expr) {
    // 1. Aloca a memória e define o crachá como COMPLEMENTO
    ASTNode* no = criar_no_base(NODE_COMPLEMENTO);
    
    // 2. Guarda a expressão que sofrerá a inversão na gaveta unária
    no->dados.operacao_unaria.expressao = expr;
    
    // 3. Devolve a estrutura pronta
    return no;
}


// =========================================================================
// ESTRUTURAS DE COMANDO (Controle de Fluxo e Atribuições)
// =========================================================================

ASTNode* criar_no_atribuicao(char* nome, ASTNode* valor) {
    // 1. Aloca a memória e define o crachá como ATRIBUIÇÃO
    ASTNode* no = criar_no_base(NODE_ATRIBUICAO);
    
    // 2. Preenche a gaveta "atribuicao" da nossa union
    no->dados.atribuicao.nome_variavel = nome;  // Salva o texto com o nome da variável (ex: "hp")
    no->dados.atribuicao.valor = valor;         // Segura a árvore de matemática que será o novo valor
    
    // 3. Devolve a estrutura pronta
    return no;
}



ASTNode* criar_no_if(ASTNode* cond, ASTNode* bloco_then, ASTNode* bloco_else) {
    // 1. Aloca a memória e define o crachá como IF
    ASTNode* no = criar_no_base(NODE_IF);
    
    // 2. Preenche a gaveta "no_if" da nossa union
    no->dados.no_if.condicao = cond;        // Segura a árvore de teste (ex: hp < 0)
    no->dados.no_if.bloco_then = bloco_then;// Segura a árvore de comandos de sucesso
    no->dados.no_if.bloco_else = bloco_else;// Segura a árvore de comandos de falha (pode ser NULL se o COOL permitir IF sem ELSE)
    
    // 3. Devolve a estrutura robusta montada
    return no;
}

ASTNode* criar_no_while(ASTNode* cond, ASTNode* corpo) {
    // 1. Aloca a memória e define o crachá como WHILE
    ASTNode* no = criar_no_base(NODE_WHILE);
    
    // 2. Preenche a gaveta "no_while" da nossa union
    no->dados.no_while.condicao = cond;  // Segura a árvore do teste de repetição (ex: hp > 0)
    no->dados.no_while.corpo = corpo;    // Segura a árvore de comandos que vão rodar em loop
    
    // 3. Devolve a estrutura montada
    return no;
}

ASTNode* criar_no_bloco(ASTNode* lista_comandos) {
    // 1. Aloca a memória e define o crachá como BLOCO
    ASTNode* no = criar_no_base(NODE_BLOCO);
    
    // 2. Preenche a gaveta "bloco" da nossa union
    no->dados.bloco.lista_comandos = lista_comandos; // Segura a "locomotiva" do trem de comandos
    
    // 3. Devolve o caixote que guarda todas as instruções
    return no;
}

ASTNode* criar_no_dispatch_implicito(ASTNode* nome_metodo, ASTNode* argumentos) {
    // 1. Aloca a memória e define o crachá como DISPATCH IMPLÍCITO
    ASTNode* no = criar_no_base(NODE_DISPATCH_IMPLICITO);
    
    // 2. Preenche a gaveta "dispatch" da nossa union
    // Como é implícito, não tem um objeto explícito chamando, nem tipo estático (@).
    no->dados.dispatch.expressao_base = NULL; 
    no->dados.dispatch.tipo_estatico = NULL;  
    
    // 3. O Pulo do Gato: Extraímos o texto puro de dentro do nó Identificador!
    no->dados.dispatch.nome_metodo = nome_metodo->dados.valor_lexico;
    
    // 4. Segura o "trem" com a lista de argumentos (ex: 10, "fogo", true)
    no->dados.dispatch.argumentos = argumentos;
    
    // 5. Como já tiramos a palavra de dentro da caixinha do identificador,
    // podemos liberar essa caixinha antiga para não desperdiçar memória RAM!
    free(nome_metodo);
    
    // 6. Devolve a estrutura de chamada montada
    return no;
}

ASTNode* criar_no_dispatch_explicito(ASTNode* base, char* tipo_estatico, char* nome_metodo, ASTNode* args) {
    // 1. Aloca a memória e define o crachá como DISPATCH EXPLÍCITO
    ASTNode* no = criar_no_base(NODE_DISPATCH_EXPLICITO);
    
    // 2. Preenche a gaveta "dispatch" da nossa union
    no->dados.dispatch.expressao_base = base;   // A árvore de quem está chamando o método (ex: "jogador")
    no->dados.dispatch.tipo_estatico = tipo_estatico; // O texto do @Classe (ou NULL, se for um '.' normal)
    no->dados.dispatch.nome_metodo = nome_metodo;     // O nome da ação (ex: "atacar")
    no->dados.dispatch.argumentos = args;             // A "locomotiva" com a lista de argumentos
    
    // 3. Devolve a estrutura de chamada completa
    return no;
}


void imprimir_arvore(ASTNode* raiz, int nivel) {
    if (raiz == NULL) return;

    for (int i = 0; i < nivel; i++) {
        printf("  "); // 2 espaços por nível de profundidade
    }

    switch(raiz->tipo) {
        // --- FOLHAS ---
        case NODE_INTEIRO:
            printf("[INTEIRO]: %s\n", raiz->dados.valor_lexico);
            break;
        case NODE_STRING:
            printf("[STRING]: %s\n", raiz->dados.valor_lexico);
            break;
        case NODE_BOOLEANO:
            printf("[BOOLEANO]: %s\n", raiz->dados.valor_booleano ? "true" : "false");
            break;
        case NODE_IDENTIFICADOR:
            printf("[ID]: %s\n", raiz->dados.valor_lexico);
            break;

        // --- MATEMÁTICA E LÓGICA ---
        case NODE_ARITMETICO:
        case NODE_RELACIONAL:
            printf("[%s] Sinal (Token): %d\n", 
                   raiz->tipo == NODE_ARITMETICO ? "ARITMETICO" : "RELACIONAL", 
                   raiz->dados.operacao_binaria.operador);
            imprimir_arvore(raiz->dados.operacao_binaria.esquerdo, nivel + 1);
            imprimir_arvore(raiz->dados.operacao_binaria.direito, nivel + 1);
            break;
        
        case NODE_NOT:
        case NODE_ISVOID:
        case NODE_COMPLEMENTO:
            printf("[%s]\n", raiz->tipo == NODE_NOT ? "NOT" : 
                             raiz->tipo == NODE_ISVOID ? "ISVOID" : "COMPLEMENTO (~)");
            imprimir_arvore(raiz->dados.operacao_unaria.expressao, nivel + 1);
            break;

        // --- ESTRUTURAS DE COMANDO ---
        case NODE_ATRIBUICAO:
            printf("[ATRIBUICAO] Variavel: %s\n", raiz->dados.atribuicao.nome_variavel);
            imprimir_arvore(raiz->dados.atribuicao.valor, nivel + 1);
            break;

        case NODE_IF:
            printf("[IF] Condicao:\n");
            imprimir_arvore(raiz->dados.no_if.condicao, nivel + 1);
            for (int i = 0; i < nivel; i++) printf("  "); printf("[THEN]:\n");
            imprimir_arvore(raiz->dados.no_if.bloco_then, nivel + 1);
            if(raiz->dados.no_if.bloco_else != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("[ELSE]:\n");
                imprimir_arvore(raiz->dados.no_if.bloco_else, nivel + 1);
            }
            break;

        case NODE_WHILE:
            printf("[WHILE] Condicao:\n");
            imprimir_arvore(raiz->dados.no_while.condicao, nivel + 1);
            for (int i = 0; i < nivel; i++) printf("  "); printf("[CORPO]:\n");
            imprimir_arvore(raiz->dados.no_while.corpo, nivel + 1);
            break;

        case NODE_BLOCO:
            printf("[BLOCO DE COMANDOS] {\n");
            imprimir_arvore(raiz->dados.bloco.lista_comandos, nivel + 1);
            for (int i = 0; i < nivel; i++) printf("  "); printf("}\n");
            break;

        // --- DISPATCH (CHAMADAS) ---
        case NODE_DISPATCH_IMPLICITO:
            printf("[CHAMADA IMPLICITA] Metodo: %s\n", raiz->dados.dispatch.nome_metodo);
            if (raiz->dados.dispatch.argumentos != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("Argumentos:\n");
                imprimir_arvore(raiz->dados.dispatch.argumentos, nivel + 1);
            }
            break;

        case NODE_DISPATCH_EXPLICITO:
            printf("[CHAMADA EXPLICITA] Metodo: %s\n", raiz->dados.dispatch.nome_metodo);
            if (raiz->dados.dispatch.tipo_estatico != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("Tipo Estatico (@): %s\n", raiz->dados.dispatch.tipo_estatico);
            }
            for (int i = 0; i < nivel; i++) printf("  "); printf("Objeto Base:\n");
            imprimir_arvore(raiz->dados.dispatch.expressao_base, nivel + 1);
            
            if (raiz->dados.dispatch.argumentos != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("Argumentos:\n");
                imprimir_arvore(raiz->dados.dispatch.argumentos, nivel + 1);
            }
            break;
        
            // --- ORIENTAÇÃO A OBJETOS ---
        case NODE_CLASSE:
            printf("[CLASSE]: %s (Herda de: %s)\n", raiz->dados.classe.nome_classe, raiz->dados.classe.nome_pai);
            imprimir_arvore(raiz->dados.classe.lista_features, nivel + 1);
            break;
            
        case NODE_METODO:
            printf("[METODO]: %s (Retorno: %s)\n", raiz->dados.metodo.nome_metodo, raiz->dados.metodo.tipo_retorno);
            if (raiz->dados.metodo.lista_formais != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("Parametros:\n");
                imprimir_arvore(raiz->dados.metodo.lista_formais, nivel + 1);
            }
            for (int i = 0; i < nivel; i++) printf("  "); printf("Corpo:\n");
            imprimir_arvore(raiz->dados.metodo.corpo, nivel + 1);
            break;

        case NODE_ATRIBUTO:
            printf("[ATRIBUTO]: %s (Tipo: %s)\n", raiz->dados.atributo.nome_atributo, raiz->dados.atributo.tipo_atributo);
            if (raiz->dados.atributo.inicializacao != NULL) {
                for (int i = 0; i < nivel; i++) printf("  "); printf("Inicializacao:\n");
                imprimir_arvore(raiz->dados.atributo.inicializacao, nivel + 1);
            }
            break;

        case NODE_FORMAL:
            printf("[PARAMETRO]: %s (Tipo: %s)\n", raiz->dados.formal.nome_parametro, raiz->dados.formal.tipo_parametro);
            break;

        // --- ESTRUTURAS AVANÇADAS ---
        case NODE_LET:
            printf("[LET]\n");
            for (int i = 0; i < nivel; i++) printf("  "); printf("Variaveis:\n");
            imprimir_arvore(raiz->dados.no_let.lista_variaveis, nivel + 1);
            for (int i = 0; i < nivel; i++) printf("  "); printf("Corpo:\n");
            imprimir_arvore(raiz->dados.no_let.corpo, nivel + 1);
            break;

        case NODE_LET_VAR:
            printf("[VARIAVEL LET]: %s (Tipo: %s)\n", raiz->dados.let_var.nome_variavel, raiz->dados.let_var.tipo_variavel);
            if (raiz->dados.let_var.inicializacao != NULL) {
                imprimir_arvore(raiz->dados.let_var.inicializacao, nivel + 1);
            }
            break;

        case NODE_CASE:
            printf("[CASE] Avaliando:\n");
            imprimir_arvore(raiz->dados.no_case.expressao_principal, nivel + 1);
            for (int i = 0; i < nivel; i++) printf("  "); printf("Opcoes:\n");
            imprimir_arvore(raiz->dados.no_case.lista_cases, nivel + 1);
            break;

        case NODE_CASE_BRANCH:
            printf("[OPCAO CASE]: %s (Tipo: %s) =>\n", raiz->dados.case_branch.nome_variavel, raiz->dados.case_branch.tipo_variavel);
            imprimir_arvore(raiz->dados.case_branch.corpo, nivel + 1);
            break;
            
        case NODE_NEW:
            printf("[NEW]: %s\n", raiz->dados.valor_lexico);
            break;

        default:
            printf("[NÓ DESCONHECIDO OU NÃO MAPEADO: %d]\n", raiz->tipo);
            break;
    }

    // A mágica da lista (o trem)! O próximo comando fica na mesma indentação.
    imprimir_arvore(raiz->proximo, nivel);
}


void liberar_arvore(ASTNode* raiz) {
    if (raiz == NULL) return;

    // 1. A Mágica do Trem: Avança pela lista encadeada e manda o próximo vagão se auto-destruir
    liberar_arvore(raiz->proximo);

    // 2. Destrói o conteúdo interno de acordo com a gaveta que usamos
    switch(raiz->tipo) {
        
        // --- FOLHAS ---
        case NODE_INTEIRO:
        case NODE_STRING:
        case NODE_IDENTIFICADOR:
            // Como usamos strdup() no parser, o texto ocupa memória separada e precisa de free!
            if (raiz->dados.valor_lexico != NULL) {
                free(raiz->dados.valor_lexico);
            }
            break;
            
        case NODE_BOOLEANO:
            // Booleanos são apenas int normais, não precisam de free. A caixa morre sozinha.
            break;

        // --- MATEMÁTICA E LÓGICA ---
        case NODE_ARITMETICO:
        case NODE_RELACIONAL:
            liberar_arvore(raiz->dados.operacao_binaria.esquerdo);
            liberar_arvore(raiz->dados.operacao_binaria.direito);
            break;

        case NODE_NOT:
        case NODE_ISVOID:
        case NODE_COMPLEMENTO:
            liberar_arvore(raiz->dados.operacao_unaria.expressao);
            break;

        // --- ESTRUTURAS DE COMANDO ---
        case NODE_ATRIBUICAO:
            if (raiz->dados.atribuicao.nome_variavel != NULL) {
                free(raiz->dados.atribuicao.nome_variavel); // O nome da variável foi criado com strdup()
            }
            liberar_arvore(raiz->dados.atribuicao.valor);
            break;

        case NODE_IF:
            liberar_arvore(raiz->dados.no_if.condicao);
            liberar_arvore(raiz->dados.no_if.bloco_then);
            liberar_arvore(raiz->dados.no_if.bloco_else);
            break;

        case NODE_WHILE:
            liberar_arvore(raiz->dados.no_while.condicao);
            liberar_arvore(raiz->dados.no_while.corpo);
            break;

        case NODE_BLOCO:
            liberar_arvore(raiz->dados.bloco.lista_comandos);
            break;

        // --- DISPATCH (CHAMADAS) ---
        case NODE_DISPATCH_IMPLICITO:
            if (raiz->dados.dispatch.nome_metodo != NULL) {
                free(raiz->dados.dispatch.nome_metodo);
            }
            liberar_arvore(raiz->dados.dispatch.argumentos);
            break;

        case NODE_DISPATCH_EXPLICITO:
            liberar_arvore(raiz->dados.dispatch.expressao_base);
            
            // Textos criados com strdup precisam ser destruídos manualmente
            if (raiz->dados.dispatch.tipo_estatico != NULL) {
                free(raiz->dados.dispatch.tipo_estatico);
            }
            if (raiz->dados.dispatch.nome_metodo != NULL) {
                free(raiz->dados.dispatch.nome_metodo);
            }
            
            liberar_arvore(raiz->dados.dispatch.argumentos);
            break;

        default:
            // Caso caia um nó não mapeado, ele passará reto e sofrerá o free() no passo 3.
            break;
    }

    // 3. O Tiro de Misericórdia: Depois que os filhos e os textos foram varridos da memória,
    // destruímos a própria caixa base que segurava tudo.
    free(raiz);
}

// =========================================================================
// FERRAMENTAS DE LISTA
// =========================================================================

ASTNode* adicionar_comando(ASTNode* lista, ASTNode* novo_comando) {
    // 1. Se a lista estiver vazia, a nova peça se torna o primeiro "vagão" (a locomotiva)
    if (lista == NULL) {
        return novo_comando;
    }
    
    // 2. Se já tem gente na lista, precisamos andar até o último vagão
    ASTNode* atual = lista;
    while (atual->proximo != NULL) {
        atual = atual->proximo;
    }
    
    // 3. Chegou no final! Engata o novo comando
    atual->proximo = novo_comando;
    
    // 4. Devolve a locomotiva (o início da lista nunca muda)
    return lista;
}

// A lógica dos argumentos é rigorosamente a mesma!
ASTNode* adicionar_argumento(ASTNode* lista, ASTNode* novo_argumento) {
    if (lista == NULL) return novo_argumento;
    
    ASTNode* atual = lista;
    while (atual->proximo != NULL) {
        atual = atual->proximo;
    }
    
    atual->proximo = novo_argumento;
    return lista;
}