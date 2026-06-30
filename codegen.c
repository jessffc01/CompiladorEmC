#include "codegen.h"
#include "SYMTAB.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Codegen adaptado para a tabela semantica com buscar_metodo/buscar_atributo. */

/* ========================================================================== */
/*                    REPRESENTACAO INTERNA DO CODEGEN                        */
/* ========================================================================== */

/*
 * Um valor produzido pela traducao de uma expressao.
 *
 * Exemplo para a expressao COOL "2 + 3":
 *   nome       = "v3"
 *   tipo_bril  = "int"
 *   tipo_cool  = "Int"
 */
typedef struct CGValue {
    char *nome;
    const char *tipo_bril;
    const char *tipo_cool;
} CGValue;

/* Ambiente lexico do gerador: formals, lets e self. */
typedef struct CGVar {
    char *nome_cool;
    char *nome_bril;
    const char *tipo_bril;
    const char *tipo_cool;
    int em_memoria;          /* 0 = valor direto; 1 = endereco de um slot */
    struct CGVar *proximo;
} CGVar;

typedef struct CGScope {
    CGVar *variaveis;
    struct CGScope *anterior;
} CGScope;

/* Campo de um objeto dentro do heap. O offset 0 fica reservado para a tag. */
typedef struct CampoLayout {
    char *nome;
    const char *tipo_cool;
    int offset;
    ASTNode *declaracao;
    const char *classe_declaradora;
    struct CampoLayout *proximo;
} CampoLayout;

/* Layout de uma classe COOL que sera materializado no heap do Bril. */
typedef struct LayoutClasse {
    char *nome;
    char *pai;
    int tag;
    int tamanho;              /* quantidade de celulas int; inclui a tag */
    int pronto;
    int em_construcao;
    int embutida;
    ASTNode *no_classe;       /* NULL para Object, IO, Int, Bool e String */
    CampoLayout *campos;
    struct LayoutClasse *proximo;
} LayoutClasse;

/* Informacao temporaria sobre um metodo encontrado na hierarquia. */
typedef struct MetodoEncontrado {
    ASTNode *declaracao;
    LayoutClasse *classe_implementacao;
} MetodoEncontrado;

/* ========================================================================== */
/*                              ESTADO GLOBAL                                 */
/* ========================================================================== */

#define OFFSET_TAG       0
#define OFFSET_PAYLOAD   1
#define TAG_VOID        -1

static unsigned long profundidade_new = 0;
static unsigned long contador_temporarios = 0;
static unsigned long contador_labels = 0;
static int erros_codegen = 0;

static LayoutClasse *layouts_classes = NULL;
static CGScope *escopo_codegen = NULL;

/* Classe cujo metodo/initializer esta sendo emitido no momento. */
static const char *classe_atual = NULL;

/* ========================================================================== */
/*                              PROTOTIPOS                                     */
/* ========================================================================== */

static void erro_codegen(ASTNode *no, const char *mensagem);
static char *cg_strdup(const char *texto);
static char *cg_formatar(const char *formato, const char *a, const char *b);

static CGValue *valor_criar(const char *nome, const char *tipo_bril,
                            const char *tipo_cool);
static void valor_liberar(CGValue *valor);

static void escopo_push(void);
static void escopo_pop(void);
static void escopo_definir_valor(const char *nome_cool, const CGValue *valor);
static void escopo_definir_local(const char *nome_cool, const CGValue *valor_inicial);
static CGVar *escopo_buscar_entrada(const char *nome_cool);
static CGValue *escopo_ler(const char *nome_cool);

static void limpar_layouts(void);
static LayoutClasse *adicionar_layout(const char *nome, const char *pai,
                                      int tag, ASTNode *no_classe, int embutida);
static LayoutClasse *buscar_layout(const char *nome);
static const char *normalizar_pai(const char *pai);
static void adicionar_campo(LayoutClasse *layout, const char *nome,
                            const char *tipo_cool, int offset,
                            ASTNode *declaracao,
                            const char *classe_declaradora);
static void copiar_campos(LayoutClasse *destino, const CampoLayout *origem);
static void montar_layout(LayoutClasse *layout);
static void preparar_layouts(ASTNode *raiz);
static CampoLayout *buscar_campo(LayoutClasse *layout, const char *nome);

static const char *resolver_self_type(const char *tipo_cool);
static const char *tipo_bril_do_cool(const char *tipo_cool);
static char *tipo_ponteiro_para(const char *tipo_bril);
static const char *inferir_tipo_cool(ASTNode *no);

static CGValue *emitir_int_bruto(int valor);
static CGValue *emitir_bool_bruto(int valor);
static CGValue *emitir_void(const char *tipo_estatico, ASTNode *contexto);
static CGValue *emitir_valor_padrao(const char *tipo_cool, ASTNode *contexto);
static char *emitir_ptradd(const CGValue *objeto, int offset);
static void emitir_store_campo(const CGValue *objeto, int offset,
                               const CGValue *valor, ASTNode *contexto);
static CGValue *boxear_int(const CGValue *valor, ASTNode *contexto);
static CGValue *boxear_escalar(const char *tipo_cool, const CGValue *payload, ASTNode *contexto);
static CGValue *boxear_bool(const CGValue *valor, ASTNode *contexto);
static CGValue *desboxear_payload(const CGValue *objeto, const char *tipo_bril, ASTNode *contexto);
static CGValue *desboxear_int(const CGValue *objeto, ASTNode *contexto);
static CGValue *desboxear_bool(const CGValue *objeto, ASTNode *contexto);
static CGValue *emitir_load_campo(const CGValue *objeto, CampoLayout *campo,
                                  ASTNode *contexto);

static CGValue *gerar_expressao(ASTNode *no);
static CGValue *gerar_inteiro(ASTNode *no);
static CGValue *gerar_booleano(ASTNode *no);
static CGValue *gerar_string(ASTNode *no);
static CGValue *gerar_string_vazia(ASTNode *contexto);
static CGValue *gerar_identificador(ASTNode *no);
static CGValue *gerar_aritmetico(ASTNode *no);
static CGValue *gerar_relacional(ASTNode *no);
static CGValue *gerar_unario(ASTNode *no);
static CGValue *gerar_atribuicao_expr(ASTNode *no);
static CGValue *gerar_bloco_expr(ASTNode *no);
static CGValue *gerar_if_expr(ASTNode *no);
static CGValue *gerar_while_expr(ASTNode *no);
static CGValue *gerar_let_expr(ASTNode *no);
static CGValue *gerar_new_expr(ASTNode *no);
static CGValue *gerar_objeto_novo(const char *nome_classe, ASTNode *contexto);
static CGValue *gerar_dispatch_expr(ASTNode *no);

static const char *opcode_aritmetico(TokenTipo operador);
static const char *opcode_relacional(TokenTipo operador);

static ASTNode *buscar_metodo_declarado(LayoutClasse *classe,
                                        const char *nome_metodo,
                                        LayoutClasse **classe_implementacao);
static const char *tipo_retorno_efetivo(const char *tipo_declarado,
                                        const CGValue *receptor);
static char *montar_nome_funcao(const char *classe, const char *metodo);

static void gerar_lista(ASTNode *primeiro);
static void gerar_classe(ASTNode *no);
static void gerar_metodo(ASTNode *no);
static void gerar_atributo(ASTNode *no);
static void gerar_formal(ASTNode *no);
static void gerar_expressao_como_comando(ASTNode *no);
static void emitir_funcao_main(void);
static Symbol *consultar_metodo_semantico(const char *nome_metodo,
                                          const char *classe_receptor);
static const char *tipo_retorno_simbolo(const Symbol *metodo,
                                        const CGValue *receptor);
static void emitir_runtime_basico(void);

static int layout_eh_subtipo(LayoutClasse *filha,
                             const char *nome_ancestral);

static int profundidade_layout(LayoutClasse *layout);

static const char *lca_tipos_cool(const char *tipo_a,
                                  const char *tipo_b);

static void ordenar_branches_por_especificidade(ASTNode **branches,
                                                int quantidade);

static const char *inferir_tipo_case(ASTNode *no);

static CGValue *gerar_case_expr(ASTNode *no);

static char *montar_nome_dispatch(const char *classe,
                                  const char *metodo);

static int contar_formais_metodo(ASTNode *metodo);

static const char *buscar_implementacao_dinamica(
    LayoutClasse *classe,
    const char *nome_metodo
);

static void emitir_dispatcher_dinamico(
    LayoutClasse *classe_base,
    const char *nome_metodo,
    int quantidade_argumentos
);

static void emitir_dispatchers_dinamicos(ASTNode *raiz);

static void emitir_verificar_receptor_nao_void(
    const CGValue *receptor,
    ASTNode *contexto
);

static CGValue *emitir_carregar_tag(const CGValue *objeto,
                                    ASTNode *contexto);

static CGValue *gerar_new_self_type_expr(ASTNode *contexto);

static CGValue *emitir_string_literal_runtime(const char *texto);
static void emitir_runtime_copy_n(void);
static void emitir_runtime_object_copy(void);
static void emitir_runtime_object_type_name(void);
static void emitir_runtime_string_concat(void);
static void emitir_runtime_string_substr(void);
static void emitir_runtime_string_equal(void);

static char *montar_nome_construtor(const char *classe);

static int classe_usa_construtor(const LayoutClasse *layout);

static void emitir_construtor_classe(LayoutClasse *layout);

static void emitir_construtores(void);

/* ========================================================================== */
/*                            UTILITARIOS BASICOS                             */
/* ========================================================================== */

static void erro_codegen(ASTNode *no, const char *mensagem) {
    erros_codegen++;

    if (no != NULL) {
        fprintf(stderr, "Codegen (linha %d, coluna %d): %s\n",
                no->linha, no->coluna, mensagem);
    } else {
        fprintf(stderr, "Codegen: %s\n", mensagem);
    }
}

static char *cg_strdup(const char *texto) {
    size_t tamanho;
    char *copia;

    if (texto == NULL) {
        texto = "";
    }

    tamanho = strlen(texto) + 1;
    copia = malloc(tamanho);

    if (copia == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    memcpy(copia, texto, tamanho);
    return copia;
}

static char *cg_formatar(const char *formato, const char *a, const char *b) {
    int tamanho;
    char *texto;

    tamanho = snprintf(NULL, 0, formato, a, b);
    if (tamanho < 0) {
        fprintf(stderr, "Erro fatal ao formatar texto.\n");
        exit(EXIT_FAILURE);
    }

    texto = malloc((size_t)tamanho + 1);
    if (texto == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(texto, (size_t)tamanho + 1, formato, a, b);
    return texto;
}

/* Gera v1, v2, v3... */
char *gerar_temporario(void) {
    int tamanho;
    char *nome;

    contador_temporarios++;
    tamanho = snprintf(NULL, 0, "v%lu", contador_temporarios);

    nome = malloc((size_t)tamanho + 1);
    if (nome == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente para temporario.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(nome, (size_t)tamanho + 1, "v%lu", contador_temporarios);
    return nome;
}

/* Gera .if_true_1, .if_false_2, .while_loop_3... */
char *gerar_label(const char *prefixo) {
    int tamanho;
    char *label;

    contador_labels++;
    tamanho = snprintf(NULL, 0, ".%s_%lu", prefixo, contador_labels);

    label = malloc((size_t)tamanho + 1);
    if (label == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente para label.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(label, (size_t)tamanho + 1, ".%s_%lu", prefixo, contador_labels);
    return label;
}

/* ========================================================================== */
/*                                VALORES                                     */
/* ========================================================================== */

static CGValue *valor_criar(const char *nome, const char *tipo_bril,
                            const char *tipo_cool) {
    CGValue *valor = malloc(sizeof(CGValue));

    if (valor == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    valor->nome = cg_strdup(nome);
    valor->tipo_bril = tipo_bril;
    valor->tipo_cool = tipo_cool;
    return valor;
}

static void valor_liberar(CGValue *valor) {
    if (valor != NULL) {
        free(valor->nome);
        free(valor);
    }
}

/* ========================================================================== */
/*                       AMBIENTE LEXICO DO CODEGEN                           */
/* ========================================================================== */

static void escopo_push(void) {
    CGScope *novo = malloc(sizeof(CGScope));

    if (novo == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    novo->variaveis = NULL;
    novo->anterior = escopo_codegen;
    escopo_codegen = novo;
}

static void escopo_pop(void) {
    CGScope *atual;
    CGVar *var;

    if (escopo_codegen == NULL) {
        return;
    }

    atual = escopo_codegen;
    escopo_codegen = atual->anterior;

    var = atual->variaveis;
    while (var != NULL) {
        CGVar *proximo = var->proximo;
        free(var->nome_cool);
        free(var->nome_bril);
        free(var);
        var = proximo;
    }

    free(atual);
}

static void escopo_inserir(const char *nome_cool, const char *nome_bril,
                           const char *tipo_bril, const char *tipo_cool,
                           int em_memoria) {
    CGVar *nova;

    if (escopo_codegen == NULL) {
        escopo_push();
    }

    nova = malloc(sizeof(CGVar));
    if (nova == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    nova->nome_cool = cg_strdup(nome_cool);
    nova->nome_bril = cg_strdup(nome_bril);
    nova->tipo_bril = tipo_bril;
    nova->tipo_cool = tipo_cool;
    nova->em_memoria = em_memoria;
    nova->proximo = escopo_codegen->variaveis;
    escopo_codegen->variaveis = nova;
}

/* self e valores auxiliares de inferencia sao mantidos diretamente. */
static void escopo_definir_valor(const char *nome_cool, const CGValue *valor) {
    escopo_inserir(nome_cool, valor->nome, valor->tipo_bril,
                   valor->tipo_cool, 0);
}

/*
 * Formais e variaveis let recebem um slot no heap. Assim, uma atribuicao
 * gera store e uma leitura gera load; isso mantem os valores corretos em
 * if/while sem precisar de phi manual para cada variavel local.
 */
static void escopo_definir_local(const char *nome_cool,
                                 const CGValue *valor_inicial) {
    CGValue *um;
    char *tipo_slot;
    char *slot;

    if (valor_inicial == NULL) {
        return;
    }

    um = emitir_int_bruto(1);
    tipo_slot = tipo_ponteiro_para(valor_inicial->tipo_bril);
    slot = gerar_temporario();

    printf("  %s: %s = alloc %s;\n", slot, tipo_slot, um->nome);
    printf("  store %s %s;\n", slot, valor_inicial->nome);

    escopo_inserir(nome_cool, slot, valor_inicial->tipo_bril,
                   valor_inicial->tipo_cool, 1);

    valor_liberar(um);
    free(tipo_slot);
    free(slot);
}

static CGVar *escopo_buscar_entrada(const char *nome_cool) {
    CGScope *escopo = escopo_codegen;

    while (escopo != NULL) {
        CGVar *var = escopo->variaveis;

        while (var != NULL) {
            if (strcmp(var->nome_cool, nome_cool) == 0) {
                return var;
            }
            var = var->proximo;
        }

        escopo = escopo->anterior;
    }

    return NULL;
}

static CGValue *escopo_ler(const char *nome_cool) {
    CGVar *entrada = escopo_buscar_entrada(nome_cool);

    if (entrada == NULL) {
        return NULL;
    }

    if (!entrada->em_memoria) {
        return valor_criar(entrada->nome_bril, entrada->tipo_bril,
                           entrada->tipo_cool);
    }

    {
        char *destino = gerar_temporario();
        CGValue *resultado;

        printf("  %s: %s = load %s;\n", destino, entrada->tipo_bril,
               entrada->nome_bril);
        resultado = valor_criar(destino, entrada->tipo_bril,
                                entrada->tipo_cool);
        free(destino);
        return resultado;
    }
}

/* ========================================================================== */
/*                             LAYOUT DE CLASSES                              */
/* ========================================================================== */

static void limpar_campos(CampoLayout *campo) {
    while (campo != NULL) {
        CampoLayout *proximo = campo->proximo;
        free(campo->nome);
        free(campo);
        campo = proximo;
    }
}

static void limpar_layouts(void) {
    LayoutClasse *layout = layouts_classes;

    while (layout != NULL) {
        LayoutClasse *proximo = layout->proximo;
        free(layout->nome);
        free(layout->pai);
        limpar_campos(layout->campos);
        free(layout);
        layout = proximo;
    }

    layouts_classes = NULL;
}

static LayoutClasse *adicionar_layout(const char *nome, const char *pai,
                                      int tag, ASTNode *no_classe, int embutida) {
    LayoutClasse *novo = calloc(1, sizeof(LayoutClasse));

    if (novo == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    novo->nome = cg_strdup(nome);
    novo->pai = cg_strdup(pai == NULL ? "" : pai);
    novo->tag = tag;
    novo->tamanho = 1; /* offset 0 = tag da classe */
    novo->no_classe = no_classe;
    novo->embutida = embutida;

    novo->proximo = layouts_classes;
    layouts_classes = novo;
    return novo;
}

static LayoutClasse *buscar_layout(const char *nome) {
    LayoutClasse *atual;

    if (nome == NULL) {
        return NULL;
    }

    if (strcmp(nome, "object") == 0) {
        nome = "Object";
    }

    atual = layouts_classes;
    while (atual != NULL) {
        if (strcmp(atual->nome, nome) == 0) {
            return atual;
        }
        atual = atual->proximo;
    }

    return NULL;
}

static const char *normalizar_pai(const char *pai) {
    if (pai == NULL || pai[0] == '\0' || strcmp(pai, "object") == 0) {
        return "Object";
    }

    return pai;
}

static void adicionar_campo(LayoutClasse *layout, const char *nome,
                            const char *tipo_cool, int offset,
                            ASTNode *declaracao,
                            const char *classe_declaradora) {
    CampoLayout *novo = calloc(1, sizeof(CampoLayout));
    CampoLayout *ultimo;

    if (novo == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    novo->nome = cg_strdup(nome);
    novo->tipo_cool = tipo_cool;
    novo->offset = offset;
    novo->declaracao = declaracao;
    novo->classe_declaradora = classe_declaradora;

    if (layout->campos == NULL) {
        layout->campos = novo;
        return;
    }

    ultimo = layout->campos;
    while (ultimo->proximo != NULL) {
        ultimo = ultimo->proximo;
    }
    ultimo->proximo = novo;
}

static void copiar_campos(LayoutClasse *destino, const CampoLayout *origem) {
    while (origem != NULL) {
        adicionar_campo(destino, origem->nome, origem->tipo_cool,
                        origem->offset, origem->declaracao,
                        origem->classe_declaradora);
        origem = origem->proximo;
    }
}

static void montar_layout(LayoutClasse *layout) {
    LayoutClasse *pai;
    ASTNode *feature;

    if (layout == NULL || layout->pronto) {
        return;
    }

    if (layout->em_construcao) {
        fprintf(stderr, "Codegen: ciclo de heranca envolvendo '%s'.\n", layout->nome);
        exit(EXIT_FAILURE);
    }

    layout->em_construcao = 1;
    layout->tamanho = 1;

    pai = buscar_layout(normalizar_pai(layout->pai));
    if (pai != NULL && pai != layout) {
        montar_layout(pai);
        copiar_campos(layout, pai->campos);
        layout->tamanho = pai->tamanho;
    }

    if (layout->no_classe != NULL) {
        feature = layout->no_classe->dados.classe.lista_features;

        while (feature != NULL) {
            if (feature->tipo == NODE_ATRIBUTO) {
                adicionar_campo(layout,
                                feature->dados.atributo.nome_atributo,
                                feature->dados.atributo.tipo_atributo,
                                layout->tamanho,
                                feature,
                                layout->nome);
                layout->tamanho++;
            }

            feature = feature->proximo;
        }
    }

    layout->em_construcao = 0;
    layout->pronto = 1;
}

static void preparar_layouts(ASTNode *raiz) {
    ASTNode *classe = raiz;
    LayoutClasse *layout;
    int proxima_tag = 5;

    limpar_layouts();

    /* Tags reservadas para classes basicas. */
    adicionar_layout("Object", "", 0, NULL, 1);
    adicionar_layout("Int", "Object", 1, NULL, 1);
    adicionar_layout("Bool", "Object", 2, NULL, 1);
    adicionar_layout("String", "Object", 3, NULL, 1);
    adicionar_layout("IO", "Object", 4, NULL, 1);

    while (classe != NULL) {
        if (classe->tipo == NODE_CLASSE) {
            if (buscar_layout(classe->dados.classe.nome_classe) != NULL) {
                erro_codegen(classe, "classe duplicada no layout de geracao.");
            } else {
                adicionar_layout(classe->dados.classe.nome_classe,
                                normalizar_pai(classe->dados.classe.nome_pai),
                                proxima_tag++, classe, 0);
            }
        }
        classe = classe->proximo;
    }

    layout = layouts_classes;
    while (layout != NULL) {
        montar_layout(layout);
        layout = layout->proximo;
    }
}

static CampoLayout *buscar_campo(LayoutClasse *layout, const char *nome) {
    CampoLayout *campo;

    if (layout == NULL) {
        return NULL;
    }

    campo = layout->campos;
    while (campo != NULL) {
        if (strcmp(campo->nome, nome) == 0) {
            return campo;
        }
        campo = campo->proximo;
    }

    return NULL;
}

static int layout_eh_subtipo(LayoutClasse *filha,
                             const char *nome_ancestral) {
    LayoutClasse *atual = filha;
    const char *ancestral = resolver_self_type(nome_ancestral);

    while (atual != NULL) {
        if (strcmp(atual->nome, ancestral) == 0) {
            return 1;
        }

        if (strcmp(atual->nome, "Object") == 0) {
            break;
        }

        atual = buscar_layout(normalizar_pai(atual->pai));
    }

    return 0;
}

static int profundidade_layout(LayoutClasse *layout) {
    int profundidade = 0;

    while (layout != NULL && strcmp(layout->nome, "Object") != 0) {
        profundidade++;
        layout = buscar_layout(normalizar_pai(layout->pai));
    }

    return profundidade;
}

static const char *lca_tipos_cool(const char *tipo_a,
                                  const char *tipo_b) {
    LayoutClasse *candidato;
    LayoutClasse *outro;

    tipo_a = resolver_self_type(tipo_a);
    tipo_b = resolver_self_type(tipo_b);

    candidato = buscar_layout(tipo_a);
    outro = buscar_layout(tipo_b);

    if (candidato == NULL || outro == NULL) {
        return "Object";
    }

    while (candidato != NULL) {
        if (layout_eh_subtipo(outro, candidato->nome)) {
            return candidato->nome;
        }

        if (strcmp(candidato->nome, "Object") == 0) {
            break;
        }

        candidato = buscar_layout(normalizar_pai(candidato->pai));
    }

    return "Object";
}

static void ordenar_branches_por_especificidade(ASTNode **branches,
                                                int quantidade) {
    int i;
    int j;

    for (i = 0; i < quantidade - 1; i++) {
        for (j = 0; j < quantidade - 1 - i; j++) {
            LayoutClasse *a = buscar_layout(
                resolver_self_type(
                    branches[j]->dados.case_branch.tipo_variavel
                )
            );

            LayoutClasse *b = buscar_layout(
                resolver_self_type(
                    branches[j + 1]->dados.case_branch.tipo_variavel
                )
            );

            if (profundidade_layout(a) < profundidade_layout(b)) {
                ASTNode *temp = branches[j];
                branches[j] = branches[j + 1];
                branches[j + 1] = temp;
            }
        }
    }
}

static const char *inferir_tipo_case(ASTNode *no) {
    ASTNode *branch = no->dados.no_case.lista_cases;
    const char *resultado = NULL;

    while (branch != NULL) {
        const char *tipo_branch;
        const char *tipo_corpo;
        CGValue *variavel_falsa;

        tipo_branch = resolver_self_type(
            branch->dados.case_branch.tipo_variavel
        );

        variavel_falsa = valor_criar(
            "_case_infer",
            tipo_bril_do_cool(tipo_branch),
            tipo_branch
        );

        escopo_push();
        escopo_definir_valor(
            branch->dados.case_branch.nome_variavel,
            variavel_falsa
        );

        tipo_corpo = inferir_tipo_cool(
            branch->dados.case_branch.corpo
        );

        escopo_pop();
        valor_liberar(variavel_falsa);

        if (resultado == NULL) {
            resultado = tipo_corpo;
        } else {
            resultado = lca_tipos_cool(resultado, tipo_corpo);
        }

        branch = branch->proximo;
    }

    return resultado == NULL ? "Object" : resultado;
}

/* ========================================================================== */
/*                               TIPOS                                         */
/* ========================================================================== */

static const char *resolver_self_type(const char *tipo_cool) {
    if (tipo_cool != NULL && strcmp(tipo_cool, "SELF_TYPE") == 0) {
        return classe_atual == NULL ? "Object" : classe_atual;
    }

    return tipo_cool == NULL ? "Object" : tipo_cool;
}

/* Bril possui int e bool; classes, Object e String sao ponteiros. */
static const char *tipo_bril_do_cool(const char *tipo_cool) {
    (void)tipo_cool;
    return "ptr<any>";
}

static char *tipo_ponteiro_para(const char *tipo_bril) {
    int tamanho = snprintf(NULL, 0, "ptr<%s>", tipo_bril);
    char *resultado = malloc((size_t)tamanho + 1);

    if (resultado == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(resultado, (size_t)tamanho + 1, "ptr<%s>", tipo_bril);
    return resultado;
}

/*
 * Inferencia leve usada apenas para escolher o tipo do slot de resultado do IF.
 * A analise semantica continua sendo a autoridade para validar o programa.
 */
static const char *inferir_tipo_cool(ASTNode *no) {
    ASTNode *ultimo;

    if (no == NULL) {
        return "Object";
    }

    switch (no->tipo) {
        case NODE_INTEIRO:
            return "Int";
        case NODE_BOOLEANO:
            return "Bool";
        case NODE_STRING:
            return "String";
        case NODE_NEW:
            return resolver_self_type(no->dados.valor_lexico);
        case NODE_ARITMETICO:
        case NODE_COMPLEMENTO:
            return "Int";
        case NODE_RELACIONAL:
        case NODE_NOT:
        case NODE_ISVOID:
            return "Bool";
        case NODE_ATRIBUICAO:
            return inferir_tipo_cool(no->dados.atribuicao.valor);
        case NODE_IDENTIFICADOR: {
            CGVar *local = escopo_buscar_entrada(no->dados.valor_lexico);
            LayoutClasse *layout;
            CampoLayout *campo;

            if (local != NULL) {
                return local->tipo_cool;
            }

            layout = buscar_layout(classe_atual);
            campo = buscar_campo(layout, no->dados.valor_lexico);
            return campo == NULL ? "Object" : resolver_self_type(campo->tipo_cool);
        }
        case NODE_BLOCO:
            ultimo = no->dados.bloco.lista_comandos;
            if (ultimo == NULL) {
                return "Object";
            }
            while (ultimo->proximo != NULL) {
                ultimo = ultimo->proximo;
            }
            return inferir_tipo_cool(ultimo);
        case NODE_IF: {
            const char *tipo_then = inferir_tipo_cool(no->dados.no_if.bloco_then);
            const char *tipo_else = inferir_tipo_cool(no->dados.no_if.bloco_else);
            return lca_tipos_cool(tipo_then, tipo_else);
        }
        case NODE_WHILE:
            return "Object";
        case NODE_LET: {
            ASTNode *var;
            const char *resultado;

            escopo_push();
            var = no->dados.no_let.lista_variaveis;
            while (var != NULL) {
                CGValue *falso = valor_criar("_infer", 
                                              tipo_bril_do_cool(var->dados.let_var.tipo_variavel),
                                              resolver_self_type(var->dados.let_var.tipo_variavel));
                escopo_definir_valor(var->dados.let_var.nome_variavel, falso);
                valor_liberar(falso);
                var = var->proximo;
            }
            resultado = inferir_tipo_cool(no->dados.no_let.corpo);
            escopo_pop();
            return resultado;
        }
        case NODE_DISPATCH_IMPLICITO:
        case NODE_DISPATCH_EXPLICITO: {
            const char *tipo_receptor;
            LayoutClasse *layout;
            LayoutClasse *classe_impl = NULL;
            ASTNode *metodo;
            Symbol *metodo_semantico;

            if (no->tipo == NODE_DISPATCH_IMPLICITO) {
                tipo_receptor = classe_atual == NULL ? "Object" : classe_atual;
            } else if (no->dados.dispatch.tipo_estatico != NULL) {
                tipo_receptor = no->dados.dispatch.tipo_estatico;
            } else {
                tipo_receptor = inferir_tipo_cool(no->dados.dispatch.expressao_base);
            }

            if (strcmp(tipo_receptor, "SELF_TYPE") == 0) {
                tipo_receptor = classe_atual == NULL ? "Object" : classe_atual;
            }

            layout = buscar_layout(tipo_receptor);
            metodo = buscar_metodo_declarado(layout, no->dados.dispatch.nome_metodo,
                                             &classe_impl);

            if (metodo != NULL) {
                if (strcmp(metodo->dados.metodo.tipo_retorno, "SELF_TYPE") == 0) {
                    return tipo_receptor;
                }
                return resolver_self_type(metodo->dados.metodo.tipo_retorno);
            }

            /*
             * A manutenção do semântico passou a registrar os métodos
             * herdados e os métodos básicos em tabela_metodos. A AST não
             * contém nós para Object/IO/String, então esta consulta é o
             * fallback necessário para inferir, por exemplo, out_int e
             * out_string.
             */
            metodo_semantico = consultar_metodo_semantico(
                no->dados.dispatch.nome_metodo, tipo_receptor
            );

            if (metodo_semantico != NULL) {
                if (strcmp(metodo_semantico->info.smb_metodo.tipo_retorno,
                           "SELF_TYPE") == 0) {
                    return tipo_receptor;
                }
                return resolver_self_type(
                    metodo_semantico->info.smb_metodo.tipo_retorno
                );
            }

            return "Object";
        }
        case NODE_CASE:
            return inferir_tipo_case(no);
        default:
            return "Object";
    }
}

/* ========================================================================== */
/*                          EMISSAO DE INSTRUCOES                              */
/* ========================================================================== */

static CGValue *emitir_int_bruto(int valor) {
    char *nome = gerar_temporario();
    CGValue *resultado;

    printf("  %s: int = const %d;\n", nome, valor);
    resultado = valor_criar(nome, "int", "Int");
    free(nome);
    return resultado;
}

static CGValue *emitir_bool_bruto(int valor) {
    char *nome = gerar_temporario();
    CGValue *resultado;

    printf("  %s: bool = const %s;\n", nome, valor ? "true" : "false");
    resultado = valor_criar(nome, "bool", "Bool");
    free(nome);
    return resultado;
}

static CGValue *emitir_void(const char *tipo_estatico,
                            ASTNode *contexto) {
    CGValue *tamanho;
    CGValue *tag;
    CGValue *objeto;
    char *nome_objeto;

    tamanho = emitir_int_bruto(1);

    nome_objeto = gerar_temporario();
    printf("  %s: ptr<any> = alloc %s;\n",
           nome_objeto, tamanho->nome);

    objeto = valor_criar(nome_objeto,
                         "ptr<any>",
                         resolver_self_type(tipo_estatico));

    free(nome_objeto);
    valor_liberar(tamanho);

    tag = emitir_int_bruto(TAG_VOID);
    emitir_store_campo(objeto, OFFSET_TAG, tag, contexto);
    valor_liberar(tag);

    return objeto;
}

static CGValue *emitir_valor_padrao(const char *tipo_cool, ASTNode *contexto) {
    CGValue *bruto;
    CGValue *resultado;

    tipo_cool = resolver_self_type(tipo_cool);

    if (strcmp(tipo_cool, "Int") == 0) {
        bruto = emitir_int_bruto(0);
        resultado = boxear_int(bruto, contexto);
        valor_liberar(bruto);
        return resultado;
    }

    if (strcmp(tipo_cool, "Bool") == 0) {
        bruto = emitir_bool_bruto(0);
        resultado = boxear_bool(bruto, contexto);
        valor_liberar(bruto);
        return resultado;
    }

    if (strcmp(tipo_cool, "String") == 0) {
        return gerar_string_vazia(contexto);
    }

    return emitir_void(tipo_cool, contexto);
}

static char *emitir_ptradd(const CGValue *objeto, int offset) {
    CGValue *offset_bril;
    char *endereco;

    offset_bril = emitir_int_bruto(offset);
    endereco = gerar_temporario();

    printf("  %s: ptr<any> = ptradd %s %s;\n",
           endereco, objeto->nome, offset_bril->nome);

    valor_liberar(offset_bril);
    return endereco;
}

static CGValue *emitir_carregar_tag(const CGValue *objeto,
                                    ASTNode *contexto) {
    char *endereco;
    char *nome;
    CGValue *resultado;

    if (objeto == NULL ||
        strcmp(objeto->tipo_bril, "ptr<any>") != 0) {
        erro_codegen(contexto, "objeto invalido para leitura de tag.");
        return NULL;
    }

    endereco = emitir_ptradd(objeto, OFFSET_TAG);
    nome = gerar_temporario();

    printf("  %s: int = load %s;\n", nome, endereco);

    resultado = valor_criar(nome, "int", "<raw-tag>");

    free(endereco);
    free(nome);

    return resultado;
}

static void emitir_store_campo(const CGValue *objeto, int offset,
                               const CGValue *valor, ASTNode *contexto) {
    char *endereco;

    if (objeto == NULL || valor == NULL) {
        erro_codegen(contexto, "tentativa de store com valor inexistente.");
        return;
    }

    if (strcmp(objeto->tipo_bril, "ptr<any>") != 0) {
        erro_codegen(contexto,
                     "objeto nao possui representacao ptr<any>.");
        return;
    }

    endereco = emitir_ptradd(objeto, offset);
    printf("  store %s %s;\n", endereco, valor->nome);
    free(endereco);
}

static void emitir_dispatcher_dinamico(
    LayoutClasse *classe_base,
    const char *nome_metodo,
    int quantidade_argumentos
) {
    LayoutClasse *classe_dinamica;
    CGValue *offset_tag;
    char *nome_dispatch;
    char *endereco_tag;
    char *tag_dinamica;
    char *label_falha;
    int i;

    if (classe_base == NULL || nome_metodo == NULL) {
        return;
    }

    nome_dispatch = montar_nome_dispatch(
        classe_base->nome,
        nome_metodo
    );

    printf("%s(self: ptr<any>", nome_dispatch);

    for (i = 0; i < quantidade_argumentos; i++) {
        printf(", arg%d: ptr<any>", i);
    }

    printf("): ptr<any> {\n");

    offset_tag = emitir_int_bruto(OFFSET_TAG);

    endereco_tag = gerar_temporario();
    tag_dinamica = gerar_temporario();

    printf("  %s: ptr<any> = ptradd self %s;\n",
           endereco_tag,
           offset_tag->nome);

    printf("  %s: int = load %s;\n",
           tag_dinamica,
           endereco_tag);

    valor_liberar(offset_tag);
    free(endereco_tag);

    classe_dinamica = layouts_classes;

    while (classe_dinamica != NULL) {
        if (layout_eh_subtipo(classe_dinamica, classe_base->nome)) {
            const char *classe_impl;
            CGValue *tag_esperada;
            char *nome_comparacao;
            char *label_match;
            char *label_proximo;
            char *nome_funcao;
            char *nome_resultado;

            classe_impl = buscar_implementacao_dinamica(
                classe_dinamica,
                nome_metodo
            );

            if (classe_impl == NULL) {
                classe_dinamica = classe_dinamica->proximo;
                continue;
            }

            tag_esperada = emitir_int_bruto(classe_dinamica->tag);

            nome_comparacao = gerar_temporario();
            label_match = gerar_label("dispatch_match");
            label_proximo = gerar_label("dispatch_next");

            printf("  %s: bool = eq %s %s;\n",
                   nome_comparacao,
                   tag_dinamica,
                   tag_esperada->nome);

            printf("  br %s %s %s;\n\n",
                   nome_comparacao,
                   label_match,
                   label_proximo);

            printf("%s:\n", label_match);

            nome_funcao = montar_nome_funcao(
                classe_impl,
                nome_metodo
            );

            nome_resultado = gerar_temporario();

            printf("  %s: ptr<any> = call %s self",
                   nome_resultado,
                   nome_funcao);

            for (i = 0; i < quantidade_argumentos; i++) {
                printf(" arg%d", i);
            }

            printf(";\n");
            printf("  ret %s;\n\n", nome_resultado);

            printf("%s:\n", label_proximo);

            valor_liberar(tag_esperada);

            free(nome_comparacao);
            free(label_match);
            free(label_proximo);
            free(nome_funcao);
            free(nome_resultado);
        }

        classe_dinamica = classe_dinamica->proximo;
    }

    label_falha = gerar_label("dispatch_fail");

    printf("  jmp %s;\n\n", label_falha);

    printf("%s:\n", label_falha);
    {
        char *nome_abort = gerar_temporario();

        printf("  %s: ptr<any> = call @Object_abort self;\n",
               nome_abort);

        printf("  ret %s;\n", nome_abort);

        free(nome_abort);
    }

    printf("}\n\n");

    free(nome_dispatch);
    free(tag_dinamica);
    free(label_falha);
}

static void emitir_verificar_receptor_nao_void(
    const CGValue *receptor,
    ASTNode *contexto
) {
    char *endereco_tag;
    char *tag;
    char *eh_void;
    char *label_abort;
    char *label_ok;
    char *resultado_abort;
    CGValue *tag_void;

    if (receptor == NULL ||
        strcmp(receptor->tipo_bril, "ptr<any>") != 0) {
        erro_codegen(contexto, "receptor de dispatch invalido.");
        return;
    }

    endereco_tag = emitir_ptradd(receptor, OFFSET_TAG);
    tag = gerar_temporario();
    eh_void = gerar_temporario();
    label_abort = gerar_label("dispatch_void");
    label_ok = gerar_label("dispatch_ok");
    resultado_abort = gerar_temporario();
    tag_void = emitir_int_bruto(TAG_VOID);

    printf("  %s: int = load %s;\n", tag, endereco_tag);
    printf("  %s: bool = eq %s %s;\n",
           eh_void, tag, tag_void->nome);

    printf("  br %s %s %s;\n\n",
           eh_void, label_abort, label_ok);

    printf("%s:\n", label_abort);
    printf("  %s: ptr<any> = call @Object_abort %s;\n",
           resultado_abort, receptor->nome);
    printf("  jmp %s;\n\n", label_ok);

    printf("%s:\n", label_ok);

    valor_liberar(tag_void);
    free(endereco_tag);
    free(tag);
    free(eh_void);
    free(label_abort);
    free(label_ok);
    free(resultado_abort);
}

static void emitir_dispatchers_dinamicos(ASTNode *raiz) {
    ASTNode *classe;

    /* Métodos básicos do runtime. */
    emitir_dispatcher_dinamico(buscar_layout("Object"), "abort", 0);
    emitir_dispatcher_dinamico(buscar_layout("Object"), "type_name", 0);
    emitir_dispatcher_dinamico(buscar_layout("Object"), "copy", 0);

    emitir_dispatcher_dinamico(buscar_layout("IO"), "out_string", 1);
    emitir_dispatcher_dinamico(buscar_layout("IO"), "out_int", 1);
    emitir_dispatcher_dinamico(buscar_layout("IO"), "in_string", 0);
    emitir_dispatcher_dinamico(buscar_layout("IO"), "in_int", 0);

    emitir_dispatcher_dinamico(buscar_layout("String"), "length", 0);
    emitir_dispatcher_dinamico(buscar_layout("String"), "concat", 1);
    emitir_dispatcher_dinamico(buscar_layout("String"), "substr", 2);

    classe = raiz;

    while (classe != NULL) {
        if (classe->tipo == NODE_CLASSE) {
            LayoutClasse *layout = buscar_layout(
                classe->dados.classe.nome_classe
            );

            ASTNode *feature =
                classe->dados.classe.lista_features;

            while (feature != NULL) {
                if (feature->tipo == NODE_METODO) {
                    emitir_dispatcher_dinamico(
                        layout,
                        feature->dados.metodo.nome_metodo,
                        contar_formais_metodo(feature)
                    );
                }

                feature = feature->proximo;
            }
        }

        classe = classe->proximo;
    }
}

static CGValue *boxear_escalar(const char *tipo_cool,
                                const CGValue *payload,
                                ASTNode *contexto) {
    LayoutClasse *layout = buscar_layout(tipo_cool);
    CGValue *tamanho;
    CGValue *tag;
    CGValue *objeto;
    char *nome_objeto;

    if (layout == NULL || payload == NULL) {
        erro_codegen(contexto, "falha ao boxear valor escalar.");
        return NULL;
    }

    tamanho = emitir_int_bruto(2);

    nome_objeto = gerar_temporario();
    printf("  %s: ptr<any> = alloc %s;\n",
           nome_objeto, tamanho->nome);

    objeto = valor_criar(nome_objeto, "ptr<any>", tipo_cool);

    free(nome_objeto);
    valor_liberar(tamanho);

    tag = emitir_int_bruto(layout->tag);

    emitir_store_campo(objeto, OFFSET_TAG, tag, contexto);
    emitir_store_campo(objeto, OFFSET_PAYLOAD, payload, contexto);

    valor_liberar(tag);

    return objeto;
}

static CGValue *boxear_int(const CGValue *valor, ASTNode *contexto) {
    if (valor == NULL || strcmp(valor->tipo_bril, "int") != 0) {
        erro_codegen(contexto, "boxear_int recebeu valor nao inteiro.");
        return NULL;
    }

    return boxear_escalar("Int", valor, contexto);
}

static CGValue *boxear_bool(const CGValue *valor, ASTNode *contexto) {
    if (valor == NULL || strcmp(valor->tipo_bril, "bool") != 0) {
        erro_codegen(contexto, "boxear_bool recebeu valor nao booleano.");
        return NULL;
    }

    return boxear_escalar("Bool", valor, contexto);
}

static CGValue *desboxear_payload(const CGValue *objeto,
                                  const char *tipo_bril,
                                  ASTNode *contexto) {
    char *endereco;
    char *nome;
    CGValue *resultado;

    if (objeto == NULL ||
        strcmp(objeto->tipo_bril, "ptr<any>") != 0) {
        erro_codegen(contexto, "tentativa de desempacotar valor nao objeto.");
        return NULL;
    }

    endereco = emitir_ptradd(objeto, OFFSET_PAYLOAD);
    nome = gerar_temporario();

    printf("  %s: %s = load %s;\n", nome, tipo_bril, endereco);

    resultado = valor_criar(nome, tipo_bril, "<raw>");

    free(endereco);
    free(nome);

    return resultado;
}

static CGValue *desboxear_int(const CGValue *objeto,
                              ASTNode *contexto) {
    return desboxear_payload(objeto, "int", contexto);
}

static CGValue *desboxear_bool(const CGValue *objeto,
                               ASTNode *contexto) {
    return desboxear_payload(objeto, "bool", contexto);
}

static CGValue *emitir_load_campo(const CGValue *objeto, CampoLayout *campo,
                                  ASTNode *contexto) {
    char *endereco;
    char *destino;
    const char *tipo_bril;
    CGValue *resultado;

    if (objeto == NULL || campo == NULL) {
        erro_codegen(contexto, "campo inexistente.");
        return NULL;
    }

    endereco = emitir_ptradd(objeto, campo->offset);
    destino = gerar_temporario();
    tipo_bril = tipo_bril_do_cool(campo->tipo_cool);

    printf("  %s: %s = load %s;\n", destino, tipo_bril, endereco);

    resultado = valor_criar(destino, tipo_bril, resolver_self_type(campo->tipo_cool));
    free(endereco);
    free(destino);
    return resultado;
}

/* ========================================================================== */
/*                       EXPRESSOES: VISITOR QUE RETORNA VALOR                */
/* ========================================================================== */

static CGValue *gerar_expressao(ASTNode *no) {
    if (no == NULL) {
        return NULL;
    }

    switch (no->tipo) {
        case NODE_INTEIRO:
            return gerar_inteiro(no);

        case NODE_BOOLEANO:
            return gerar_booleano(no);

        case NODE_STRING:
            return gerar_string(no);

        case NODE_IDENTIFICADOR:
            return gerar_identificador(no);

        case NODE_ARITMETICO:
            return gerar_aritmetico(no);

        case NODE_RELACIONAL:
            return gerar_relacional(no);

        case NODE_NOT:
        case NODE_COMPLEMENTO:
        case NODE_ISVOID:
            return gerar_unario(no);

        case NODE_ATRIBUICAO:
            return gerar_atribuicao_expr(no);

        case NODE_BLOCO:
            return gerar_bloco_expr(no);

        case NODE_IF:
            return gerar_if_expr(no);

        case NODE_WHILE:
            return gerar_while_expr(no);

        case NODE_LET:
            return gerar_let_expr(no);

        case NODE_NEW:
            return gerar_new_expr(no);

        case NODE_DISPATCH_IMPLICITO:
        case NODE_DISPATCH_EXPLICITO:
            return gerar_dispatch_expr(no);

        case NODE_CASE:
            return gerar_case_expr(no);

        case NODE_CASE_BRANCH:
            erro_codegen(no,"NODE_CASE_BRANCH deve ser tratado dentro de NODE_CASE.");
            return NULL;

        default:
            erro_codegen(no, "tipo de expressao nao suportado.");
            return NULL;
    }
}

static CGValue *gerar_inteiro(ASTNode *no) {
    CGValue *bruto;
    CGValue *resultado;
    long numero = strtol(no->dados.valor_lexico, NULL, 10);

    bruto = emitir_int_bruto((int)numero);
    resultado = boxear_int(bruto, no);

    valor_liberar(bruto);
    return resultado;
}

static CGValue *gerar_booleano(ASTNode *no) {
    CGValue *bruto;
    CGValue *resultado;

    bruto = emitir_bool_bruto(no->dados.valor_booleano);
    resultado = boxear_bool(bruto, no);

    valor_liberar(bruto);
    return resultado;
}

/*
 * String simplificada no heap:
 *   offset 0 -> tag String
 *   offset 1 -> tamanho
 *   offset 2.. -> codigo ASCII de cada caractere
 *
 * Isso permite passar "Racao" como ptr<any> para um metodo do usuario.
 * Metodos completos de String (length, concat, substr) ainda dependem do runtime.
 */

static size_t tamanho_string_decodificada(const char *texto) {
    size_t i = 0;
    size_t tamanho = 0;

    while (texto[i] != '\0') {
        if (texto[i] == '\\' && texto[i + 1] != '\0') {
            i += 2;
        } else {
            i++;
        }

        tamanho++;
    }

    return tamanho;
}

static unsigned char decodificar_caractere_string(const char *texto,
                                                   size_t *indice) {
    unsigned char caractere;

    if (texto[*indice] == '\\' && texto[*indice + 1] != '\0') {
        (*indice)++;

        switch (texto[*indice]) {
            case 'n':  caractere = '\n'; break;
            case 't':  caractere = '\t'; break;
            case 'r':  caractere = '\r'; break;
            case '\\': caractere = '\\'; break;
            case '"':  caractere = '"';  break;
            default:
                /* Mantém o caractere após a barra em escapes desconhecidos. */
                caractere = (unsigned char)texto[*indice];
                break;
        }
    } else {
        caractere = (unsigned char)texto[*indice];
    }

    return caractere;
}

static CGValue *gerar_string(ASTNode *no) {
    const char *texto = no->dados.valor_lexico;
    LayoutClasse *layout_string = buscar_layout("String");
    size_t tamanho_texto = tamanho_string_decodificada(texto);
    CGValue *tamanho;
    CGValue *objeto;
    CGValue *tag;
    size_t i;
    int offset = 2;

    if (layout_string == NULL) {
        erro_codegen(no, "layout de String nao encontrado.");
        return NULL;
    }

    /* tag + comprimento + caracteres reais */
    tamanho = emitir_int_bruto((int)tamanho_texto + 2);

    {
        char *nome_objeto = gerar_temporario();

        printf("  %s: ptr<any> = alloc %s;\n", nome_objeto, tamanho->nome);

        objeto = valor_criar(nome_objeto, "ptr<any>", "String");
        free(nome_objeto);
    }

    tag = emitir_int_bruto(layout_string->tag);
    emitir_store_campo(objeto, 0, tag, no);
    valor_liberar(tag);

    {
        CGValue *comprimento = emitir_int_bruto((int)tamanho_texto);

        emitir_store_campo(objeto, 1, comprimento, no);
        valor_liberar(comprimento);
    }

    for (i = 0; texto[i] != '\0'; i++) {
        unsigned char codigo;
        CGValue *caractere;

        codigo = decodificar_caractere_string(texto, &i);

        caractere = emitir_int_bruto((int)codigo);
        emitir_store_campo(objeto, offset, caractere, no);

        valor_liberar(caractere);
        offset++;
    }

    valor_liberar(tamanho);
    return objeto;
}

static CGValue *gerar_string_vazia(ASTNode *contexto) {
    LayoutClasse *layout_string = buscar_layout("String");
    CGValue *tamanho;
    CGValue *objeto;
    CGValue *tag;
    CGValue *comprimento;
    char *nome_objeto;

    if (layout_string == NULL) {
        erro_codegen(contexto, "layout de String nao encontrado.");
        return NULL;
    }

    /*
     * String vazia:
     * [0] tag String
     * [1] comprimento 0
     */
    tamanho = emitir_int_bruto(2);

    nome_objeto = gerar_temporario();
    printf("  %s: ptr<any> = alloc %s;\n",
           nome_objeto, tamanho->nome);

    objeto = valor_criar(nome_objeto, "ptr<any>", "String");

    free(nome_objeto);
    valor_liberar(tamanho);

    tag = emitir_int_bruto(layout_string->tag);
    emitir_store_campo(objeto, 0, tag, contexto);
    valor_liberar(tag);

    comprimento = emitir_int_bruto(0);
    emitir_store_campo(objeto, 1, comprimento, contexto);
    valor_liberar(comprimento);

    return objeto;
}

static CGValue *emitir_string_literal_runtime(const char *texto) {
    LayoutClasse *layout_string = buscar_layout("String");
    CGValue *tamanho;
    CGValue *objeto;
    CGValue *tag;
    CGValue *comprimento;
    char *nome_objeto;
    size_t i;
    size_t tamanho_texto;

    if (layout_string == NULL) {
        erro_codegen(NULL, "layout String nao encontrado no runtime.");
        return NULL;
    }

    tamanho_texto = strlen(texto);

    tamanho = emitir_int_bruto((int)tamanho_texto + 2);

    nome_objeto = gerar_temporario();
    printf("  %s: ptr<any> = alloc %s;\n",
           nome_objeto,
           tamanho->nome);

    objeto = valor_criar(nome_objeto, "ptr<any>", "String");

    free(nome_objeto);
    valor_liberar(tamanho);

    tag = emitir_int_bruto(layout_string->tag);
    emitir_store_campo(objeto, 0, tag, NULL);
    valor_liberar(tag);

    comprimento = emitir_int_bruto((int)tamanho_texto);
    emitir_store_campo(objeto, 1, comprimento, NULL);
    valor_liberar(comprimento);

    for (i = 0; i < tamanho_texto; i++) {
        CGValue *caractere =
            emitir_int_bruto((unsigned char)texto[i]);

        emitir_store_campo(objeto, (int)i + 2, caractere, NULL);
        valor_liberar(caractere);
    }

    return objeto;
}

static void emitir_runtime_object_type_name(void) {
    LayoutClasse *layout;
    char *offset_tag;
    char *endereco_tag;
    char *tag_dinamica;

    printf("@Object_type_name(self: ptr<any>): ptr<any> {\n");

    offset_tag = gerar_temporario();
    endereco_tag = gerar_temporario();
    tag_dinamica = gerar_temporario();

    printf("  %s: int = const 0;\n", offset_tag);
    printf("  %s: ptr<any> = ptradd self %s;\n",
           endereco_tag,
           offset_tag);
    printf("  %s: int = load %s;\n",
           tag_dinamica,
           endereco_tag);

    layout = layouts_classes;

    while (layout != NULL) {
        CGValue *tag_esperada;
        CGValue *nome_classe;
        char *comparacao;
        char *label_match;
        char *label_next;

        tag_esperada = emitir_int_bruto(layout->tag);
        comparacao = gerar_temporario();
        label_match = gerar_label("typename_match");
        label_next = gerar_label("typename_next");

        printf("  %s: bool = eq %s %s;\n",
               comparacao,
               tag_dinamica,
               tag_esperada->nome);

        printf("  br %s %s %s;\n\n",
               comparacao,
               label_match,
               label_next);

        printf("%s:\n", label_match);

        nome_classe = emitir_string_literal_runtime(layout->nome);

        if (nome_classe != NULL) {
            printf("  ret %s;\n\n", nome_classe->nome);
            valor_liberar(nome_classe);
        } else {
            printf("  typename_abort: ptr<any> = call @Object_abort self;\n");
            printf("  ret typename_abort;\n\n");
        }

        printf("%s:\n", label_next);

        valor_liberar(tag_esperada);

        free(comparacao);
        free(label_match);
        free(label_next);

        layout = layout->proximo;
    }

    printf("  typename_abort_final: ptr<any> = call @Object_abort self;\n");
    printf("  ret typename_abort_final;\n");
    printf("}\n\n");

    free(offset_tag);
    free(endereco_tag);
    free(tag_dinamica);
}

static void emitir_runtime_string_concat(void) {
    LayoutClasse *layout_string = buscar_layout("String");
    int tag_string = layout_string == NULL ? 3 : layout_string->tag;

    printf("@String_concat(self: ptr<any>, s: ptr<any>): ptr<any> {\n");

    printf("  sc_zero: int = const 0;\n");
    printf("  sc_one: int = const 1;\n");
    printf("  sc_two: int = const 2;\n");

    printf("  sc_self_len_ptr: ptr<any> = ptradd self sc_one;\n");
    printf("  sc_self_len: int = load sc_self_len_ptr;\n");

    printf("  sc_s_len_ptr: ptr<any> = ptradd s sc_one;\n");
    printf("  sc_s_len: int = load sc_s_len_ptr;\n");

    printf("  sc_total_len: int = add sc_self_len sc_s_len;\n");
    printf("  sc_alloc_size: int = add sc_total_len sc_two;\n");

    printf("  sc_out: ptr<any> = alloc sc_alloc_size;\n");

    printf("  sc_tag: int = const %d;\n", tag_string);

    printf("  sc_out_tag_ptr: ptr<any> = ptradd sc_out sc_zero;\n");
    printf("  store sc_out_tag_ptr sc_tag;\n");

    printf("  sc_out_len_ptr: ptr<any> = ptradd sc_out sc_one;\n");
    printf("  store sc_out_len_ptr sc_total_len;\n");

    /*
     * Copia self.
     */
    printf("  sc_left_slot: ptr<int> = alloc sc_one;\n");
    printf("  store sc_left_slot sc_zero;\n\n");

    printf(".sc_left_loop:\n");
    printf("  sc_left_i: int = load sc_left_slot;\n");
    printf("  sc_left_more: bool = lt sc_left_i sc_self_len;\n");
    printf("  br sc_left_more .sc_left_body .sc_left_end;\n\n");

    printf(".sc_left_body:\n");
    printf("  sc_left_offset: int = add sc_left_i sc_two;\n");
    printf("  sc_left_src: ptr<any> = ptradd self sc_left_offset;\n");
    printf("  sc_left_char: int = load sc_left_src;\n");
    printf("  sc_left_dst: ptr<any> = ptradd sc_out sc_left_offset;\n");
    printf("  store sc_left_dst sc_left_char;\n");
    printf("  sc_left_next: int = add sc_left_i sc_one;\n");
    printf("  store sc_left_slot sc_left_next;\n");
    printf("  jmp .sc_left_loop;\n\n");

    printf(".sc_left_end:\n");
    printf("  free sc_left_slot;\n\n");

    /*
     * Copia s após self.
     */
    printf("  sc_right_slot: ptr<int> = alloc sc_one;\n");
    printf("  store sc_right_slot sc_zero;\n\n");

    printf(".sc_right_loop:\n");
    printf("  sc_right_i: int = load sc_right_slot;\n");
    printf("  sc_right_more: bool = lt sc_right_i sc_s_len;\n");
    printf("  br sc_right_more .sc_right_body .sc_right_end;\n\n");

    printf(".sc_right_body:\n");
    printf("  sc_right_src_offset: int = add sc_right_i sc_two;\n");
    printf("  sc_right_src: ptr<any> = ptradd s sc_right_src_offset;\n");
    printf("  sc_right_char: int = load sc_right_src;\n");

    printf("  sc_right_base: int = add sc_self_len sc_two;\n");
    printf("  sc_right_dst_offset: int = add sc_right_i sc_right_base;\n");
    printf("  sc_right_dst: ptr<any> = ptradd sc_out sc_right_dst_offset;\n");
    printf("  store sc_right_dst sc_right_char;\n");

    printf("  sc_right_next: int = add sc_right_i sc_one;\n");
    printf("  store sc_right_slot sc_right_next;\n");
    printf("  jmp .sc_right_loop;\n\n");

    printf(".sc_right_end:\n");
    printf("  free sc_right_slot;\n");
    printf("  ret sc_out;\n");
    printf("}\n\n");
}

static void emitir_runtime_string_substr(void) {
    LayoutClasse *layout_string = buscar_layout("String");
    int tag_string = layout_string == NULL ? 3 : layout_string->tag;

    printf("@String_substr(self: ptr<any>, i: ptr<any>, l: ptr<any>): ptr<any> {\n");

    printf("  ss_zero: int = const 0;\n");
    printf("  ss_one: int = const 1;\n");
    printf("  ss_two: int = const 2;\n");

    /*
     * Desboxea i.
     */
    printf("  ss_i_ptr: ptr<any> = ptradd i ss_one;\n");
    printf("  ss_i: int = load ss_i_ptr;\n");

    /*
     * Desboxea l.
     */
    printf("  ss_l_ptr: ptr<any> = ptradd l ss_one;\n");
    printf("  ss_l: int = load ss_l_ptr;\n");

    /*
     * Tamanho de self.
     */
    printf("  ss_len_ptr: ptr<any> = ptradd self ss_one;\n");
    printf("  ss_len: int = load ss_len_ptr;\n");

    /*
     * Verifica i >= 0.
     */
    printf("  ss_i_negativo: bool = lt ss_i ss_zero;\n");
    printf("  br ss_i_negativo .ss_abort .ss_check_l;\n\n");

    printf(".ss_check_l:\n");
    printf("  ss_l_negativo: bool = lt ss_l ss_zero;\n");
    printf("  br ss_l_negativo .ss_abort .ss_check_limite;\n\n");

    printf(".ss_check_limite:\n");
    printf("  ss_fim: int = add ss_i ss_l;\n");
    printf("  ss_fora: bool = gt ss_fim ss_len;\n");
    printf("  br ss_fora .ss_abort .ss_ok;\n\n");

    printf(".ss_ok:\n");
    printf("  ss_alloc_size: int = add ss_l ss_two;\n");
    printf("  ss_out: ptr<any> = alloc ss_alloc_size;\n");

    printf("  ss_tag: int = const %d;\n", tag_string);

    printf("  ss_out_tag: ptr<any> = ptradd ss_out ss_zero;\n");
    printf("  store ss_out_tag ss_tag;\n");

    printf("  ss_out_len: ptr<any> = ptradd ss_out ss_one;\n");
    printf("  store ss_out_len ss_l;\n");

    printf("  ss_slot: ptr<int> = alloc ss_one;\n");
    printf("  store ss_slot ss_zero;\n\n");

    printf(".ss_loop:\n");
    printf("  ss_k: int = load ss_slot;\n");
    printf("  ss_more: bool = lt ss_k ss_l;\n");
    printf("  br ss_more .ss_body .ss_end;\n\n");

    printf(".ss_body:\n");
    printf("  ss_src_base: int = add ss_i ss_two;\n");
    printf("  ss_src_offset: int = add ss_src_base ss_k;\n");
    printf("  ss_src: ptr<any> = ptradd self ss_src_offset;\n");
    printf("  ss_char: int = load ss_src;\n");

    printf("  ss_dst_offset: int = add ss_k ss_two;\n");
    printf("  ss_dst: ptr<any> = ptradd ss_out ss_dst_offset;\n");
    printf("  store ss_dst ss_char;\n");

    printf("  ss_next: int = add ss_k ss_one;\n");
    printf("  store ss_slot ss_next;\n");
    printf("  jmp .ss_loop;\n\n");

    printf(".ss_end:\n");
    printf("  free ss_slot;\n");
    printf("  ret ss_out;\n\n");

    printf(".ss_abort:\n");
    printf("  ss_abort_val: ptr<any> = call @Object_abort self;\n");
    printf("  ret ss_abort_val;\n");

    printf("}\n\n");
}

static void emitir_runtime_string_equal(void) {
    printf("@Runtime_string_equal(a: ptr<any>, b: ptr<any>): bool {\n");

    printf("  rse_zero: int = const 0;\n");
    printf("  rse_one: int = const 1;\n");
    printf("  rse_two: int = const 2;\n");
    printf("  rse_void_tag: int = const %d;\n", TAG_VOID);

    /*
     * Slot do índice.
     */
    printf("  rse_slot: ptr<int> = alloc rse_one;\n");
    printf("  store rse_slot rse_zero;\n");

    /*
     * Tags.
     */
    printf("  rse_a_tag_ptr: ptr<any> = ptradd a rse_zero;\n");
    printf("  rse_a_tag: int = load rse_a_tag_ptr;\n");

    printf("  rse_b_tag_ptr: ptr<any> = ptradd b rse_zero;\n");
    printf("  rse_b_tag: int = load rse_b_tag_ptr;\n");

    /*
     * void = void -> true
     * void = String -> false
     */
    printf("  rse_a_void: bool = eq rse_a_tag rse_void_tag;\n");
    printf("  br rse_a_void .rse_a_is_void .rse_a_not_void;\n\n");

    printf(".rse_a_is_void:\n");
    printf("  rse_b_void: bool = eq rse_b_tag rse_void_tag;\n");
    printf("  free rse_slot;\n");
    printf("  ret rse_b_void;\n\n");

    printf(".rse_a_not_void:\n");
    printf("  rse_b_void2: bool = eq rse_b_tag rse_void_tag;\n");
    printf("  br rse_b_void2 .rse_false .rse_compare_length;\n\n");

    printf(".rse_compare_length:\n");
    printf("  rse_a_len_ptr: ptr<any> = ptradd a rse_one;\n");
    printf("  rse_a_len: int = load rse_a_len_ptr;\n");

    printf("  rse_b_len_ptr: ptr<any> = ptradd b rse_one;\n");
    printf("  rse_b_len: int = load rse_b_len_ptr;\n");

    printf("  rse_same_len: bool = eq rse_a_len rse_b_len;\n");
    printf("  br rse_same_len .rse_loop .rse_false;\n\n");

    printf(".rse_loop:\n");
    printf("  rse_i: int = load rse_slot;\n");
    printf("  rse_more: bool = lt rse_i rse_a_len;\n");
    printf("  br rse_more .rse_body .rse_true;\n\n");

    printf(".rse_body:\n");
    printf("  rse_offset: int = add rse_i rse_two;\n");

    printf("  rse_a_ptr: ptr<any> = ptradd a rse_offset;\n");
    printf("  rse_a_char: int = load rse_a_ptr;\n");

    printf("  rse_b_ptr: ptr<any> = ptradd b rse_offset;\n");
    printf("  rse_b_char: int = load rse_b_ptr;\n");

    printf("  rse_same_char: bool = eq rse_a_char rse_b_char;\n");
    printf("  br rse_same_char .rse_next .rse_false;\n\n");

    printf(".rse_next:\n");
    printf("  rse_next_i: int = add rse_i rse_one;\n");
    printf("  store rse_slot rse_next_i;\n");
    printf("  jmp .rse_loop;\n\n");

    printf(".rse_false:\n");
    printf("  free rse_slot;\n");
    printf("  rse_false_value: bool = const false;\n");
    printf("  ret rse_false_value;\n\n");

    printf(".rse_true:\n");
    printf("  free rse_slot;\n");
    printf("  rse_true_value: bool = const true;\n");
    printf("  ret rse_true_value;\n");

    printf("}\n\n");
}

static CGValue *gerar_identificador(ASTNode *no) {
    const char *nome = no->dados.valor_lexico;
    CGValue *local;
    LayoutClasse *layout;
    CampoLayout *campo;
    CGValue *self;

    local = escopo_ler(nome);
    if (local != NULL) {
        return local;
    }

    layout = buscar_layout(classe_atual);
    campo = buscar_campo(layout, nome);
    self = escopo_ler("self");

    if (campo != NULL && self != NULL) {
        return emitir_load_campo(self, campo, no);
    }

    erro_codegen(no, "identificador nao encontrado no ambiente do gerador.");
    return NULL;
}

static const char *opcode_aritmetico(TokenTipo operador) {
    switch (operador) {
        case MAIS:          return "add";
        case MENOS:         return "sub";
        case MULTIPLICACAO: return "mul";
        case DIVISAO:       return "div";
        default:            return NULL;
    }
}

static CGValue *gerar_aritmetico(ASTNode *no) {
    CGValue *esquerdo;
    CGValue *direito;
    const char *opcode;
    char *resultado_nome;
    CGValue *resultado;

    esquerdo = gerar_expressao(no->dados.operacao_binaria.esquerdo);
    direito = gerar_expressao(no->dados.operacao_binaria.direito);
    opcode = opcode_aritmetico(no->dados.operacao_binaria.operador);

    CGValue *esquerdo_raw = desboxear_int(esquerdo, no);
    CGValue *direito_raw = desboxear_int(direito, no);

    if (esquerdo_raw == NULL || direito_raw == NULL || opcode == NULL) {
        erro_codegen(no, "operacao aritmetica invalida.");
        valor_liberar(esquerdo_raw);
        valor_liberar(direito_raw);
        return NULL;
    }

    if (strcmp(esquerdo_raw->tipo_bril, "int") != 0 ||
        strcmp(direito_raw->tipo_bril, "int") != 0) {
        erro_codegen(no, "operacao aritmetica exige dois inteiros.");
        valor_liberar(esquerdo_raw);
        valor_liberar(direito_raw);
        return NULL;
    }

    resultado_nome = gerar_temporario();

    /* Exato Bril: v3: int = add v1 v2; */
    printf("  %s: int = %s %s %s;\n",
           resultado_nome, opcode, esquerdo_raw->nome, direito_raw->nome);

    CGValue *raw = valor_criar(resultado_nome, "int", "<raw-int>");
    resultado = boxear_int(raw, no);

    valor_liberar(raw);
    free(resultado_nome);
    valor_liberar(esquerdo_raw);
    valor_liberar(direito_raw);
    valor_liberar(esquerdo);
    valor_liberar(direito);
    return resultado;
}

static const char *opcode_relacional(TokenTipo operador) {
    switch (operador) {
        case MENOR:         return "lt";
        case MENOROUIGUAL:  return "le";
        case MAIOR:         return "gt";
        case MAIOROUIGUAL:  return "ge";
        case IGUAL:         return "eq";
        default:            return NULL;
    }
}

static CGValue *gerar_relacional(ASTNode *no) {
    CGValue *esquerdo;
    CGValue *direito;
    const char *opcode;
    char *resultado_nome;
    CGValue *resultado;

    esquerdo = gerar_expressao(no->dados.operacao_binaria.esquerdo);
    direito = gerar_expressao(no->dados.operacao_binaria.direito);
    opcode = opcode_relacional(no->dados.operacao_binaria.operador);

    const char *tipo_esq =
    inferir_tipo_cool(no->dados.operacao_binaria.esquerdo);

    CGValue *esquerdo_raw;
    CGValue *direito_raw;

    int raws_sao_aliases = 0;

    if (no->dados.operacao_binaria.operador != IGUAL) {
        esquerdo_raw = desboxear_int(esquerdo, no);
        direito_raw = desboxear_int(direito, no);
    } else if (strcmp(tipo_esq, "Int") == 0) {
        esquerdo_raw = desboxear_int(esquerdo, no);
        direito_raw = desboxear_int(direito, no);
    } else if (strcmp(tipo_esq, "Bool") == 0) {
        esquerdo_raw = desboxear_bool(esquerdo, no);
        direito_raw = desboxear_bool(direito, no);
    }else if (strcmp(tipo_esq, "String") == 0) {
        char *nome_raw = gerar_temporario();
        CGValue *raw;
        CGValue *resultado_string;

        printf("  %s: bool = call @Runtime_string_equal %s %s;\n",
            nome_raw,
            esquerdo->nome,
            direito->nome);

        raw = valor_criar(nome_raw, "bool", "<raw-bool>");
        resultado_string = boxear_bool(raw, no);

        valor_liberar(raw);
        free(nome_raw);

        valor_liberar(esquerdo);
        valor_liberar(direito);

        return resultado_string;
    } else {
        /*
        * Objectos normais: comparar ponteiros diretamente.
        * String ainda exigirá comparação de conteúdo depois.
        */
        esquerdo_raw = esquerdo;
        direito_raw = direito;
        raws_sao_aliases = 1;
    }

    if (esquerdo_raw == NULL || direito_raw == NULL || opcode == NULL) {
        erro_codegen(no, "operacao relacional invalida.");
        valor_liberar(esquerdo_raw);
        valor_liberar(direito_raw);
        return NULL;
    }

    if (strcmp(esquerdo_raw->tipo_bril, direito_raw->tipo_bril) != 0) {
        erro_codegen(no, "comparacao entre valores com tipos Bril diferentes.");
        valor_liberar(esquerdo_raw);
        valor_liberar(direito_raw);
        return NULL;
    }

    resultado_nome = gerar_temporario();

    /* Exato Bril: v3: bool = lt v1 v2; */
    printf("  %s: bool = %s %s %s;\n",
           resultado_nome, opcode, esquerdo_raw->nome, direito_raw->nome);

    CGValue *raw = valor_criar(resultado_nome, "bool", "<raw-bool>");
    resultado = boxear_bool(raw, no);

    valor_liberar(raw);
    free(resultado_nome);
    if (!raws_sao_aliases) {
        valor_liberar(esquerdo_raw);
        valor_liberar(direito_raw);
    }

    valor_liberar(esquerdo);
    valor_liberar(direito);
    return resultado;
}

static CGValue *gerar_unario(ASTNode *no) {
    CGValue *operando = gerar_expressao(no->dados.operacao_unaria.expressao);
    char *nome;
    CGValue *resultado;

    if (operando == NULL) {
        return NULL;
    }

    CGValue *operando_raw;

    if (no->tipo == NODE_NOT) {
        operando_raw = desboxear_bool(operando, no);

        if (operando_raw == NULL) {
            valor_liberar(operando);
            return NULL;
        }

        if (strcmp(operando_raw->tipo_bril, "bool") != 0) {
            erro_codegen(no, "not exige um Bool.");
            valor_liberar(operando_raw);
            return NULL;
        }

        nome = gerar_temporario();
        printf("  %s: bool = not %s;\n", nome, operando_raw->nome);
        CGValue *raw = valor_criar(nome, "bool", "<raw-bool>");
        resultado = boxear_bool(raw, no);

        valor_liberar(raw);
        free(nome);
        valor_liberar(operando_raw);
        valor_liberar(operando);
        return resultado;
    }

    if (no->tipo == NODE_COMPLEMENTO) {
        CGValue *zero;
        operando_raw = desboxear_int(operando, no);

        if (operando_raw == NULL) {
            valor_liberar(operando);
            return NULL;
        }

        if (strcmp(operando_raw->tipo_bril, "int") != 0) {
            erro_codegen(no, "~ exige um Int.");
            valor_liberar(operando_raw);
            return NULL;
        }

        /* ~x em COOL e -x. Usamos sub para nao depender de uma opcode extra. */
        zero = emitir_int_bruto(0);
        nome = gerar_temporario();
        printf("  %s: int = sub %s %s;\n", nome, zero->nome, operando_raw->nome);
        CGValue *raw = valor_criar(nome, "int", "<raw-int>");
        resultado = boxear_int(raw, no);

        valor_liberar(raw);
        free(nome);
        valor_liberar(zero);
        valor_liberar(operando);
        valor_liberar(operando_raw);
        return resultado;
    }

    if (no->tipo == NODE_ISVOID) {
        char *endereco = emitir_ptradd(operando, OFFSET_TAG);
        char *nome_tag = gerar_temporario();
        char *nome_resultado = gerar_temporario();

        CGValue *tag_void = emitir_int_bruto(TAG_VOID);
        CGValue *raw;
        CGValue *resultado_void;

        printf("  %s: int = load %s;\n", nome_tag, endereco);
        printf("  %s: bool = eq %s %s;\n",
            nome_resultado, nome_tag, tag_void->nome);

        raw = valor_criar(nome_resultado, "bool", "<raw-bool>");
        resultado_void = boxear_bool(raw, no);

        valor_liberar(raw);
        valor_liberar(tag_void);
        valor_liberar(operando);

        free(endereco);
        free(nome_tag);
        free(nome_resultado);

        return resultado_void;
    }

    erro_codegen(no, "operador unario desconhecido.");
    valor_liberar(operando);
    return NULL;
}

static CGValue *gerar_atribuicao_expr(ASTNode *no) {
    const char *nome = no->dados.atribuicao.nome_variavel;
    CGValue *direito = gerar_expressao(no->dados.atribuicao.valor);
    CGVar *local;
    LayoutClasse *layout;
    CampoLayout *campo;
    CGValue *self;

    if (direito == NULL) {
        return NULL;
    }

    local = escopo_buscar_entrada(nome);
    if (local != NULL) {
        if (!local->em_memoria) {
            erro_codegen(no, "nao e permitido atribuir para self.");
            valor_liberar(direito);
            return NULL;
        }

        if (strcmp(local->tipo_bril, direito->tipo_bril) != 0) {
            erro_codegen(no, "atribuicao local com tipo Bril incompativel.");
            valor_liberar(direito);
            return NULL;
        }

        /* Exato Bril: store slot_da_variavel valor_direito; */
        printf("  store %s %s;\n", local->nome_bril, direito->nome);
        return direito; /* Em COOL, x <- e devolve o valor de e. */
    }

    layout = buscar_layout(classe_atual);
    campo = buscar_campo(layout, nome);
    self = escopo_ler("self");

    if (campo != NULL && self != NULL) {
        emitir_store_campo(self, campo->offset, direito, no);
        return direito; /* Em COOL, x <- e devolve o valor de e. */
    }

    erro_codegen(no, "atribuicao para identificador inexistente.");
    valor_liberar(direito);
    return NULL;
}

static CGValue *gerar_bloco_expr(ASTNode *no) {
    ASTNode *comando = no->dados.bloco.lista_comandos;
    CGValue *ultimo = NULL;

    while (comando != NULL) {
        CGValue *atual = gerar_expressao(comando);

        valor_liberar(ultimo);
        ultimo = atual;
        comando = comando->proximo;
    }

    if (ultimo == NULL) {
        erro_codegen(no, "bloco vazio nao possui valor COOL valido.");
    }

    return ultimo;
}

/*
 * IF como expressao:
 *
 *   slot: ptr<int> = alloc um;
 *   br cond .if_true_1 .if_false_2;
 * .if_true_1:
 *   store slot valor_then;
 *   jmp .if_end_3;
 * .if_false_2:
 *   store slot valor_else;
 *   jmp .if_end_3;
 * .if_end_3:
 *   resultado: int = load slot;
 *   free slot;
 *
 * O slot evita redefinir a mesma variavel Bril em dois blocos de controle.
 */
static CGValue *gerar_if_expr(ASTNode *no) {
    const char *tipo_resultado_cool;
    const char *tipo_resultado_bril;
    CGValue *condicao;
    CGValue *valor_then;
    CGValue *valor_else;
    CGValue *um;
    char *tipo_slot;
    char *slot;
    char *label_then;
    char *label_else;
    char *label_fim;
    char *resultado_nome;
    CGValue *resultado;

    /* O tipo do slot deve ser conhecido antes do br. */
    tipo_resultado_cool = lca_tipos_cool(
        inferir_tipo_cool(no->dados.no_if.bloco_then),
        inferir_tipo_cool(no->dados.no_if.bloco_else)
    );

    tipo_resultado_bril = tipo_bril_do_cool(tipo_resultado_cool);

    condicao = gerar_expressao(no->dados.no_if.condicao);
    CGValue *condicao_raw = desboxear_bool(condicao, no);

    if (condicao == NULL || condicao_raw == NULL) {
        valor_liberar(condicao);
        valor_liberar(condicao_raw);
        return NULL;
    }

    um = emitir_int_bruto(1);
    tipo_slot = tipo_ponteiro_para(tipo_resultado_bril);
    slot = gerar_temporario();
    printf("  %s: %s = alloc %s;\n", slot, tipo_slot, um->nome);

    label_then = gerar_label("if_true");
    label_else = gerar_label("if_false");
    label_fim = gerar_label("if_end");

    printf("  br %s %s %s;\n\n", condicao_raw->nome, label_then, label_else);

    printf("%s:\n", label_then);
    valor_then = gerar_expressao(no->dados.no_if.bloco_then);
    if (valor_then == NULL) {
        valor_liberar(condicao);
        valor_liberar(um);
        free(tipo_slot); free(slot); free(label_then); free(label_else); free(label_fim);
        return NULL;
    }
    if (strcmp(valor_then->tipo_bril, tipo_resultado_bril) != 0) {
        erro_codegen(no, "ramo then gerou tipo diferente do tipo esperado do if.");
    }
    printf("  store %s %s;\n", slot, valor_then->nome);
    printf("  jmp %s;\n\n", label_fim);

    printf("%s:\n", label_else);
    valor_else = gerar_expressao(no->dados.no_if.bloco_else);
    if (valor_else == NULL) {
        valor_liberar(condicao);
        valor_liberar(valor_then);
        valor_liberar(um);
        free(tipo_slot); free(slot); free(label_then); free(label_else); free(label_fim);
        return NULL;
    }
    if (strcmp(valor_else->tipo_bril, tipo_resultado_bril) != 0) {
        erro_codegen(no, "ramo else gerou tipo diferente do tipo esperado do if.");
    }
    printf("  store %s %s;\n", slot, valor_else->nome);
    printf("  jmp %s;\n\n", label_fim);

    printf("%s:\n", label_fim);
    resultado_nome = gerar_temporario();
    printf("  %s: %s = load %s;\n", resultado_nome, tipo_resultado_bril, slot);
    printf("  free %s;\n", slot);

    resultado = valor_criar(resultado_nome, tipo_resultado_bril, tipo_resultado_cool);

    valor_liberar(condicao);
    valor_liberar(condicao_raw);
    valor_liberar(valor_then);
    valor_liberar(valor_else);
    valor_liberar(um);
    free(tipo_slot);
    free(slot);
    free(label_then);
    free(label_else);
    free(label_fim);
    free(resultado_nome);

    return resultado;
}
/* WHILE gera saltos; COOL atribui Object ao while, que nao possui valor em Bril. */
static CGValue *gerar_while_expr(ASTNode *no) {
    char *label_inicio = gerar_label("while_loop");
    char *label_corpo = gerar_label("while_body");
    char *label_fim = gerar_label("while_end");
    CGValue *condicao;
    CGValue *corpo;

    printf("%s:\n", label_inicio);

    condicao = gerar_expressao(no->dados.no_while.condicao);
    
    CGValue *condicao_raw = desboxear_bool(condicao, no);

    if (condicao == NULL || condicao_raw == NULL) {
        valor_liberar(condicao);
        valor_liberar(condicao_raw);
        return NULL;
    }

    printf("  br %s %s %s;\n\n", condicao_raw->nome, label_corpo, label_fim);

    printf("%s:\n", label_corpo);
    corpo = gerar_expressao(no->dados.no_while.corpo);
    valor_liberar(corpo);
    printf("  jmp %s;\n\n", label_inicio);

    printf("%s:\n", label_fim);

    valor_liberar(condicao_raw);
    valor_liberar(condicao);
    free(label_inicio);
    free(label_corpo);
    free(label_fim);

    /*
     * Enquanto for usado como comando dentro de um bloco, NULL e suficiente:
     * o proximo comando (ou o ultimo comando valido) fornece o valor do bloco.
     * Se ele for usado onde um valor e obrigatorio, o no pai sinalizara erro.
     */
    return emitir_void("Object", no);
}

static CGValue *gerar_let_expr(ASTNode *no) {
    ASTNode *variavel;
    CGValue *resultado;

    escopo_push();

    variavel = no->dados.no_let.lista_variaveis;
    while (variavel != NULL) {
        CGValue *inicializacao;
        const char *tipo_cool;

        if (variavel->tipo != NODE_LET_VAR) {
            erro_codegen(variavel, "lista de let contem um no invalido.");
            escopo_pop();
            return NULL;
        }

        tipo_cool = resolver_self_type(variavel->dados.let_var.tipo_variavel);

        if (variavel->dados.let_var.inicializacao != NULL) {
            inicializacao = gerar_expressao(variavel->dados.let_var.inicializacao);
        } else {
            inicializacao = emitir_valor_padrao(tipo_cool, variavel);
        }

        if (inicializacao == NULL) {
            escopo_pop();
            return NULL;
        }

        /* O slot local preserva o valor entre ramos e repeticoes. */
        inicializacao->tipo_cool = tipo_cool;
        inicializacao->tipo_bril = tipo_bril_do_cool(tipo_cool);
        escopo_definir_local(variavel->dados.let_var.nome_variavel, inicializacao);

        valor_liberar(inicializacao);
        variavel = variavel->proximo;
    }

    resultado = gerar_expressao(no->dados.no_let.corpo);
    /* O nome Bril do resultado continua valido apos deixar o escopo. */
    escopo_pop();
    return resultado;
}

static CGValue *gerar_new_self_type_expr(ASTNode *contexto) {
    LayoutClasse *layout;
    LayoutClasse **classes;
    char **labels;
    CGValue *self;
    CGValue *tag_dinamica;
    CGValue *um;
    CGValue *resultado;

    char *tipo_slot;
    char *slot;
    char *label_falha;
    char *label_fim;
    char *nome_resultado;

    int quantidade = 0;
    int i = 0;

    if (classe_atual == NULL) {
        erro_codegen(contexto, "new SELF_TYPE sem classe atual.");
        return NULL;
    }

    self = escopo_ler("self");

    if (self == NULL) {
        erro_codegen(contexto, "new SELF_TYPE sem self.");
        return NULL;
    }

    /*
     * Considera a classe atual e todas as suas subclasses.
     * Exemplo:
     * A <- B <- C
     * Em um metodo de A, SELF_TYPE pode ser A, B ou C.
     */
    layout = layouts_classes;

    while (layout != NULL) {
        if (!layout->embutida &&
            layout_eh_subtipo(layout, classe_atual)) {
            quantidade++;
        }

        layout = layout->proximo;
    }

    if (quantidade == 0) {
        erro_codegen(contexto,
                     "nenhuma classe encontrada para new SELF_TYPE.");
        valor_liberar(self);
        return NULL;
    }

    classes = calloc((size_t)quantidade, sizeof(LayoutClasse *));
    labels = calloc((size_t)quantidade, sizeof(char *));

    if (classes == NULL || labels == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    layout = layouts_classes;

    while (layout != NULL) {
        if (!layout->embutida &&
            layout_eh_subtipo(layout, classe_atual)) {
            classes[i++] = layout;
        }

        layout = layout->proximo;
    }

    tag_dinamica = emitir_carregar_tag(self, contexto);

    if (tag_dinamica == NULL) {
        free(classes);
        free(labels);
        valor_liberar(self);
        return NULL;
    }

    um = emitir_int_bruto(1);
    tipo_slot = tipo_ponteiro_para("ptr<any>");
    slot = gerar_temporario();

    printf("  %s: %s = alloc %s;\n",
           slot, tipo_slot, um->nome);

    valor_liberar(um);
    free(tipo_slot);

    /*
     * Testa a tag dinamica de self.
     */
    for (i = 0; i < quantidade; i++) {
        CGValue *tag_esperada;
        char *comparacao;
        char *label_proximo;

        labels[i] = gerar_label("new_self_type");

        tag_esperada = emitir_int_bruto(classes[i]->tag);
        comparacao = gerar_temporario();
        label_proximo = gerar_label("new_self_next");

        printf("  %s: bool = eq %s %s;\n",
               comparacao,
               tag_dinamica->nome,
               tag_esperada->nome);

        printf("  br %s %s %s;\n\n",
               comparacao,
               labels[i],
               label_proximo);

        printf("%s:\n", label_proximo);

        valor_liberar(tag_esperada);
        free(comparacao);
        free(label_proximo);
    }

    label_falha = gerar_label("new_self_fail");
    label_fim = gerar_label("new_self_end");

    printf("  jmp %s;\n\n", label_falha);

    /*
     * Cada branch cria um objeto da classe dinamica correta.
     */
    for (i = 0; i < quantidade; i++) {
        CGValue *novo;

        printf("%s:\n", labels[i]);

        novo = gerar_objeto_novo(classes[i]->nome, contexto);

        if (novo != NULL) {
            printf("  store %s %s;\n", slot, novo->nome);
            valor_liberar(novo);
        }

        printf("  jmp %s;\n\n", label_fim);
    }

    printf("%s:\n", label_falha);
    {
        char *nome_abort = gerar_temporario();

        printf("  %s: ptr<any> = call @Object_abort %s;\n",
               nome_abort, self->nome);

        printf("  store %s %s;\n", slot, nome_abort);
        printf("  jmp %s;\n\n", label_fim);

        free(nome_abort);
    }

    printf("%s:\n", label_fim);

    nome_resultado = gerar_temporario();

    printf("  %s: ptr<any> = load %s;\n",
           nome_resultado, slot);

    printf("  free %s;\n", slot);

    resultado = valor_criar(
        nome_resultado,
        "ptr<any>",
        classe_atual
    );

    valor_liberar(self);
    valor_liberar(tag_dinamica);

    for (i = 0; i < quantidade; i++) {
        free(labels[i]);
    }

    free(classes);
    free(labels);
    free(slot);
    free(label_falha);
    free(label_fim);
    free(nome_resultado);

    return resultado;
}

static CGValue *gerar_new_expr(ASTNode *no) {
    const char *tipo = no->dados.valor_lexico;

    if (strcmp(tipo, "Int") == 0) {
        CGValue *zero = emitir_int_bruto(0);
        CGValue *resultado = boxear_int(zero, no);
        valor_liberar(zero);
        return resultado;
    }

    if (strcmp(tipo, "Bool") == 0) {
        CGValue *falso = emitir_bool_bruto(0);
        CGValue *resultado = boxear_bool(falso, no);
        valor_liberar(falso);
        return resultado;
    }

     if (strcmp(tipo, "String") == 0) {
        return gerar_string_vazia(no);
    }

    if (strcmp(tipo, "SELF_TYPE") == 0) {
        return gerar_new_self_type_expr(no);
    }

    return gerar_objeto_novo(tipo, no);
}

static char *montar_nome_construtor(const char *classe) {
    size_t tamanho;
    char *nome;

    if (classe == NULL) {
        classe = "Object";
    }

    tamanho = strlen("@__new_") + strlen(classe) + 1;

    nome = malloc(tamanho);

    if (nome == NULL) {
        fprintf(stderr,
                "Erro fatal: memoria insuficiente para construtor.\n");
        exit(EXIT_FAILURE);
    }

    snprintf(nome, tamanho, "@__new_%s", classe);

    return nome;
}

static int classe_usa_construtor(const LayoutClasse *layout) {
    if (layout == NULL) {
        return 0;
    }

    /*
     * Int, Bool e String possuem construcao especial:
     * Int    -> boxear_int(0)
     * Bool   -> boxear_bool(false)
     * String -> gerar_string_vazia()
     */
    if (strcmp(layout->nome, "Int") == 0 ||
        strcmp(layout->nome, "Bool") == 0 ||
        strcmp(layout->nome, "String") == 0) {
        return 0;
    }

    return 1;
}

/*
 * Representacao de objeto:
 *   [0]  = tag da classe
 *   [1+] = atributos Int, incluindo os herdados
 *
 * Exemplo para Ponto { x : Int; }:
 *   v1: int = const 2;
 *   v2: ptr<int> = alloc v1;
 *   ... store tag em v2[0]
 *   ... store x em v2[1]
 */
static CGValue *gerar_objeto_novo(const char *nome_classe,
                                  ASTNode *contexto) {
    LayoutClasse *layout;
    char *nome_construtor;
    char *nome_resultado;
    CGValue *resultado;

    layout = buscar_layout(nome_classe);

    if (layout == NULL) {
        erro_codegen(contexto,
                     "classe de new nao encontrada no layout.");
        return NULL;
    }

    if (!classe_usa_construtor(layout)) {
        erro_codegen(contexto,
                     "tentativa de usar construtor generico "
                     "para classe basica escalar.");
        return NULL;
    }

    nome_construtor = montar_nome_construtor(layout->nome);
    nome_resultado = gerar_temporario();

    /*
     * Agora "new Foo" vira somente:
     *
     * v10: ptr<any> = call @__new_Foo;
     */
    printf("  %s: ptr<any> = call %s;\n",
           nome_resultado,
           nome_construtor);

    resultado = valor_criar(nome_resultado,
                            "ptr<any>",
                            layout->nome);

    free(nome_construtor);
    free(nome_resultado);

    return resultado;
}

static void emitir_construtor_classe(LayoutClasse *layout) {
    CGValue *tamanho;
    CGValue *objeto;
    CGValue *tag;
    CampoLayout *campo;
    const char *classe_anterior;
    char *nome_construtor;
    char *nome_objeto;

    if (!classe_usa_construtor(layout)) {
        return;
    }

    nome_construtor = montar_nome_construtor(layout->nome);

    printf("%s(): ptr<any> {\n", nome_construtor);

    /*
     * Aloca o objeto.
     */
    tamanho = emitir_int_bruto(layout->tamanho);

    nome_objeto = gerar_temporario();

    printf("  %s: ptr<any> = alloc %s;\n",
           nome_objeto,
           tamanho->nome);

    objeto = valor_criar(nome_objeto,
                         "ptr<any>",
                         layout->nome);

    free(nome_objeto);
    valor_liberar(tamanho);

    /*
     * Salva a tag dinâmica.
     */
    tag = emitir_int_bruto(layout->tag);

    emitir_store_campo(objeto,
                       OFFSET_TAG,
                       tag,
                       NULL);

    valor_liberar(tag);

    /*
     * Inicializadores de atributos podem usar self.
     */
    classe_anterior = classe_atual;
    classe_atual = layout->nome;

    escopo_push();
    escopo_definir_valor("self", objeto);

    /*
     * Os campos já estão em ordem pai -> filho,
     * pois montar_layout() copia os campos herdados primeiro.
     */
    campo = layout->campos;

    while (campo != NULL) {
        CGValue *valor_inicial;

        if (campo->declaracao != NULL &&
            campo->declaracao->dados.atributo.inicializacao != NULL) {

            valor_inicial = gerar_expressao(
                campo->declaracao->dados.atributo.inicializacao
            );

        } else {
            valor_inicial = emitir_valor_padrao(
                campo->tipo_cool,
                campo->declaracao
            );
        }

        if (valor_inicial != NULL) {
            emitir_store_campo(objeto,
                               campo->offset,
                               valor_inicial,
                               campo->declaracao);

            valor_liberar(valor_inicial);
        }

        campo = campo->proximo;
    }

    printf("  ret %s;\n", objeto->nome);
    printf("}\n\n");

    escopo_pop();

    classe_atual = classe_anterior;

    valor_liberar(objeto);
    free(nome_construtor);
}

static void emitir_construtores(void) {
    LayoutClasse *layout = layouts_classes;

    while (layout != NULL) {
        emitir_construtor_classe(layout);
        layout = layout->proximo;
    }
}

static CGValue *gerar_case_expr(ASTNode *no) {
    ASTNode *branch;
    ASTNode **branches;
    char **labels_branch;
    int quantidade = 0;
    int i;

    CGValue *sujeito;
    CGValue *um;
    CGValue *tag_dinamica;
    CGValue *resultado;

    const char *tipo_resultado_cool;
    const char *tipo_resultado_bril;

    char *endereco_tag;
    char *nome_tag;
    char *tipo_slot;
    char *slot;
    char *label_falha;
    char *label_fim;
    char *nome_resultado;

    branch = no->dados.no_case.lista_cases;

    while (branch != NULL) {
        quantidade++;
        branch = branch->proximo;
    }

    if (quantidade == 0) {
        erro_codegen(no, "case sem branches.");
        return NULL;
    }

    sujeito = gerar_expressao(
        no->dados.no_case.expressao_principal
    );

    if (sujeito == NULL) {
        return NULL;
    }

    /*
    * Agora todos os valores COOL sao ptr<any>.
    * Int, Bool, String e objetos possuem tag no offset 0.
    */
    if (strcmp(sujeito->tipo_bril, "ptr<any>") != 0) {
        erro_codegen(no,
                     "case atualmente requer uma expressao de objeto ptr<any>.");
        valor_liberar(sujeito);
        return NULL;
    }

    branches = calloc((size_t)quantidade, sizeof(ASTNode *));
    labels_branch = calloc((size_t)quantidade, sizeof(char *));

    if (branches == NULL || labels_branch == NULL) {
        fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
        exit(EXIT_FAILURE);
    }

    branch = no->dados.no_case.lista_cases;

    for (i = 0; i < quantidade; i++) {
        if (branch->tipo != NODE_CASE_BRANCH) {
            erro_codegen(branch,
                         "lista de case possui no diferente de NODE_CASE_BRANCH.");
        }

        branches[i] = branch;
        branch = branch->proximo;
    }

    /*
     * A ordem textual do COOL nao basta.
     * Bar precisa ser testado antes de Razz, Foo e Bazz.
     */
    ordenar_branches_por_especificidade(branches, quantidade);

    tipo_resultado_cool = inferir_tipo_case(no);
    tipo_resultado_bril = tipo_bril_do_cool(tipo_resultado_cool);

    um = emitir_int_bruto(1);
    tipo_slot = tipo_ponteiro_para(tipo_resultado_bril);
    slot = gerar_temporario();

    printf("  %s: %s = alloc %s;\n",
           slot, tipo_slot, um->nome);

    valor_liberar(um);
    free(tipo_slot);

    endereco_tag = emitir_ptradd(sujeito, 0);
    nome_tag = gerar_temporario();

    printf("  %s: int = load %s;\n",
           nome_tag, endereco_tag);

    tag_dinamica = valor_criar(nome_tag, "int", "Int");

    free(endereco_tag);
    free(nome_tag);

    for (i = 0; i < quantidade; i++) {
        LayoutClasse *layout_it = layouts_classes;
        const char *tipo_branch = resolver_self_type(
            branches[i]->dados.case_branch.tipo_variavel
        );
        int encontrou_tag = 0;

        labels_branch[i] = gerar_label("case_branch");

        while (layout_it != NULL) {
            if (layout_eh_subtipo(layout_it, tipo_branch)) {
                CGValue *tag_esperada;
                char *nome_comparacao;
                char *label_proximo;

                tag_esperada = emitir_int_bruto(layout_it->tag);
                nome_comparacao = gerar_temporario();
                label_proximo = gerar_label("case_test");

                printf("  %s: bool = eq %s %s;\n",
                       nome_comparacao,
                       tag_dinamica->nome,
                       tag_esperada->nome);

                printf("  br %s %s %s;\n\n",
                       nome_comparacao,
                       labels_branch[i],
                       label_proximo);

                printf("%s:\n", label_proximo);

                valor_liberar(tag_esperada);
                free(nome_comparacao);
                free(label_proximo);

                encontrou_tag = 1;
            }

            layout_it = layout_it->proximo;
        }

        if (!encontrou_tag) {
            erro_codegen(
                branches[i],
                "tipo de branch nao encontrado no layout."
            );
        }
    }

    label_falha = gerar_label("case_no_match");
    label_fim = gerar_label("case_end");

    printf("  jmp %s;\n\n", label_falha);

    /*
     * Gera os corpos das branches.
     * Cada branch recebe seu proprio escopo.
     */
    for (i = 0; i < quantidade; i++) {
        const char *tipo_branch = resolver_self_type(
            branches[i]->dados.case_branch.tipo_variavel
        );

        CGValue *variavel_branch;
        CGValue *valor_branch;

        printf("%s:\n", labels_branch[i]);

        escopo_push();

        /*
         * A variavel da branch pode receber atribuicoes,
         * portanto ela entra como slot local mutavel.
         */
        variavel_branch = valor_criar(
            sujeito->nome,
            "ptr<any>",
            tipo_branch
        );

        escopo_definir_local(
            branches[i]->dados.case_branch.nome_variavel,
            variavel_branch
        );

        valor_liberar(variavel_branch);

        valor_branch = gerar_expressao(
            branches[i]->dados.case_branch.corpo
        );

        if (valor_branch == NULL) {
            erro_codegen(
                branches[i],
                "branch de case nao gerou valor."
            );
        } else if (strcmp(valor_branch->tipo_bril,
                          tipo_resultado_bril) != 0) {
            erro_codegen(
                branches[i],
                "branch produziu tipo Bril diferente do resultado do case."
            );
        } else {
            printf("  store %s %s;\n",
                   slot,
                   valor_branch->nome);
        }

        valor_liberar(valor_branch);
        escopo_pop();

        printf("  jmp %s;\n\n", label_fim);
    }

    /*
     * Caso nenhuma branch seja compativel, COOL deveria abortar.
     * O runtime atual ainda possui Object_abort como stub,
     * mas este trecho mantem o Bril estruturalmente valido.
     */
    printf("%s:\n", label_falha);

    {
        char *nome_abort = gerar_temporario();

        printf("  %s: ptr<any> = call @Object_abort %s;\n",
            nome_abort, sujeito->nome);

        printf("  store %s %s;\n", slot, nome_abort);

        free(nome_abort);
    }

    printf("  jmp %s;\n\n", label_fim);

    printf("%s:\n", label_fim);

    nome_resultado = gerar_temporario();

    printf("  %s: %s = load %s;\n",
           nome_resultado,
           tipo_resultado_bril,
           slot);

    printf("  free %s;\n", slot);

    resultado = valor_criar(
        nome_resultado,
        tipo_resultado_bril,
        tipo_resultado_cool
    );

    valor_liberar(sujeito);
    valor_liberar(tag_dinamica);

    for (i = 0; i < quantidade; i++) {
        free(labels_branch[i]);
    }

    free(branches);
    free(labels_branch);
    free(slot);
    free(label_falha);
    free(label_fim);
    free(nome_resultado);

    return resultado;
}

/* ========================================================================== */
/*                                  DISPATCH                                  */
/* ========================================================================== */

static ASTNode *buscar_metodo_declarado(LayoutClasse *classe,
                                        const char *nome_metodo,
                                        LayoutClasse **classe_implementacao) {
    ASTNode *feature;
    LayoutClasse *pai;

    if (classe == NULL) {
        return NULL;
    }

    if (classe->no_classe != NULL) {
        feature = classe->no_classe->dados.classe.lista_features;
        while (feature != NULL) {
            if (feature->tipo == NODE_METODO &&
                strcmp(feature->dados.metodo.nome_metodo, nome_metodo) == 0) {
                if (classe_implementacao != NULL) {
                    *classe_implementacao = classe;
                }
                return feature;
            }
            feature = feature->proximo;
        }
    }

    pai = buscar_layout(normalizar_pai(classe->pai));
    if (pai != NULL && pai != classe) {
        return buscar_metodo_declarado(pai, nome_metodo, classe_implementacao);
    }

    return NULL;
}

static const char *tipo_retorno_efetivo(const char *tipo_declarado,
                                        const CGValue *receptor) {
    if (tipo_declarado != NULL && strcmp(tipo_declarado, "SELF_TYPE") == 0) {
        return receptor == NULL ? "Object" : receptor->tipo_cool;
    }

    return resolver_self_type(tipo_declarado);
}

static char *montar_nome_funcao(const char *classe, const char *metodo) {
    return cg_formatar("@%s_%s", classe, metodo);
}

static char *montar_nome_dispatch(const char *classe,
                                  const char *metodo) {
    return cg_formatar("@dispatch_%s_%s", classe, metodo);
}

static int contar_formais_metodo(ASTNode *metodo) {
    ASTNode *formal;
    int quantidade = 0;

    if (metodo == NULL || metodo->tipo != NODE_METODO) {
        return 0;
    }

    formal = metodo->dados.metodo.lista_formais;

    while (formal != NULL) {
        quantidade++;
        formal = formal->proximo;
    }

    return quantidade;
}

static const char *buscar_implementacao_dinamica(
    LayoutClasse *classe,
    const char *nome_metodo
) {
    LayoutClasse *classe_impl = NULL;
    ASTNode *metodo_ast;
    Symbol *metodo_semantico;

    metodo_ast = buscar_metodo_declarado(
        classe,
        nome_metodo,
        &classe_impl
    );

    if (metodo_ast != NULL && classe_impl != NULL) {
        return classe_impl->nome;
    }

    metodo_semantico = consultar_metodo_semantico(
        nome_metodo,
        classe->nome
    );

    if (metodo_semantico != NULL &&
        metodo_semantico->info.smb_metodo.classOrigem != NULL) {
        return metodo_semantico->info.smb_metodo.classOrigem;
    }

    return NULL;
}

static Symbol *consultar_metodo_semantico(const char *nome_metodo,
                                          const char *classe_receptor) {
    if (nome_metodo == NULL || classe_receptor == NULL) {
        return NULL;
    }

    /*
     * checkProgram() já construiu tabela_metodos antes de iniciar o
     * codegen. Essa busca cobre métodos herdados e os básicos de Object,
     * IO e String, que não aparecem como NODE_METODO na AST do usuário.
     */
    return buscar_metodo((char *)nome_metodo, (char *)classe_receptor);
}

static const char *tipo_retorno_simbolo(const Symbol *metodo,
                                        const CGValue *receptor) {
    if (metodo == NULL) {
        return "Object";
    }

    return tipo_retorno_efetivo(metodo->info.smb_metodo.tipo_retorno,
                                receptor);
}

static CGValue *gerar_dispatch_expr(ASTNode *no) {
    int eh_dispatch_estatico;
    CGValue *receptor;
    const char *tipo_receptor;
    LayoutClasse *layout_receptor;
    LayoutClasse *classe_implementacao = NULL;
    ASTNode *metodo_ast;
    Symbol *metodo_semantico = NULL;
    const char *classe_implementacao_nome;
    const char *nome_metodo;
    const char *tipo_declarado_retorno;
    ASTNode *argumento;
    CGValue **valores_argumentos = NULL;
    int quantidade_argumentos = 0;
    int indice = 0;
    char *nome_funcao;
    char *resultado_nome;
    const char *tipo_retorno_cool;
    const char *tipo_retorno_bril;
    CGValue *resultado;

    if (no->tipo == NODE_DISPATCH_IMPLICITO) {
        receptor = escopo_ler("self");
    } else {
        receptor = gerar_expressao(no->dados.dispatch.expressao_base);
    }

    if (receptor == NULL) {
        erro_codegen(no, "dispatch sem receptor valido.");
        return NULL;
    }

    emitir_verificar_receptor_nao_void(receptor, no);

    if (no->dados.dispatch.tipo_estatico != NULL) {
        tipo_receptor = no->dados.dispatch.tipo_estatico;
    } else {
        tipo_receptor = receptor->tipo_cool;
    }

    if (tipo_receptor == NULL || strcmp(tipo_receptor, "SELF_TYPE") == 0) {
        tipo_receptor = classe_atual == NULL ? "Object" : classe_atual;
    }

    layout_receptor = buscar_layout(tipo_receptor);
    nome_metodo = no->dados.dispatch.nome_metodo;

    eh_dispatch_estatico =
    no->tipo == NODE_DISPATCH_EXPLICITO &&
    no->dados.dispatch.tipo_estatico != NULL;

    /*
     * Primeiro procura na AST: esse é o caminho para métodos de classes
     * declaradas pelo usuário e preserva o nome da implementação herdada.
     */
    metodo_ast = buscar_metodo_declarado(layout_receptor, nome_metodo,
                                         &classe_implementacao);

    if (metodo_ast != NULL && classe_implementacao != NULL) {
        classe_implementacao_nome = classe_implementacao->nome;
        tipo_declarado_retorno = metodo_ast->dados.metodo.tipo_retorno;
    } else {
        /*
         * Fallback para a tabela construída pela análise semântica. Isso
         * trata Object, IO e String, além de validar a mesma hierarquia que
         * o semântico usou no dispatch.
         */
        metodo_semantico = consultar_metodo_semantico(nome_metodo,
                                                      tipo_receptor);

        if (metodo_semantico == NULL) {
            erro_codegen(no,
                         "metodo nao encontrado na AST nem na tabela semantica.");
            valor_liberar(receptor);
            return NULL;
        }

        classe_implementacao_nome =
            metodo_semantico->info.smb_metodo.classOrigem;
        tipo_declarado_retorno =
            metodo_semantico->info.smb_metodo.tipo_retorno;
    }

    argumento = no->dados.dispatch.argumentos;
    while (argumento != NULL) {
        quantidade_argumentos++;
        argumento = argumento->proximo;
    }

    if (quantidade_argumentos > 0) {
        valores_argumentos = calloc((size_t)quantidade_argumentos,
                                    sizeof(CGValue *));
        if (valores_argumentos == NULL) {
            fprintf(stderr, "Erro fatal: memoria insuficiente.\n");
            exit(EXIT_FAILURE);
        }
    }

    argumento = no->dados.dispatch.argumentos;
    while (argumento != NULL) {
        valores_argumentos[indice] = gerar_expressao(argumento);
        if (valores_argumentos[indice] == NULL) {
            int j;

            for (j = 0; j <= indice; j++) {
                valor_liberar(valores_argumentos[j]);
            }

            free(valores_argumentos);
            valor_liberar(receptor);
            return NULL;
        }

        indice++;
        argumento = argumento->proximo;
    }

    if (eh_dispatch_estatico) {
    /*
     * obj@A.f()
     * Chamada direta para A.f().
     */
    nome_funcao = montar_nome_funcao(
        classe_implementacao_nome,
        nome_metodo
    );
    } else {
        /*
        * obj.f() ou f()
        * Escolhe a implementação pela tag dinâmica de self.
        */
        nome_funcao = montar_nome_dispatch(
            classe_implementacao_nome,
            nome_metodo
        );
    }

    if (metodo_semantico != NULL) {
        tipo_retorno_cool = tipo_retorno_simbolo(metodo_semantico,
                                                 receptor);
    } else {
        tipo_retorno_cool = tipo_retorno_efetivo(tipo_declarado_retorno,
                                                 receptor);
    }

    tipo_retorno_bril = tipo_bril_do_cool(tipo_retorno_cool);
    resultado_nome = gerar_temporario();

    /*
     * Gera, por exemplo:
     *   v9: int = call @Animal_comer pet v8;
     *
     * O receptor é sempre o primeiro argumento e representa self.
     */
    printf("  %s: %s = call %s %s",
           resultado_nome, tipo_retorno_bril, nome_funcao, receptor->nome);

    for (indice = 0; indice < quantidade_argumentos; indice++) {
        printf(" %s", valores_argumentos[indice]->nome);
    }
    printf(";\n");

    resultado = valor_criar(resultado_nome, tipo_retorno_bril,
                            tipo_retorno_cool);

    for (indice = 0; indice < quantidade_argumentos; indice++) {
        valor_liberar(valores_argumentos[indice]);
    }

    free(valores_argumentos);
    valor_liberar(receptor);
    free(nome_funcao);
    free(resultado_nome);

    return resultado;
}

/* ========================================================================== */
/*                     VISITOR DE CLASSES, METODOS E MAIN                     */
/* ========================================================================== */

/*
 * Runtime mínimo para os métodos básicos registrados pelo semântico.
 *
 * out_int é funcional porque Bril possui print para inteiros. out_string
 * agora percorre a representação de String no heap e converte cada código
 * para char. Os demais métodos de String/entrada continuam como stubs
 * tipados até existir um runtime COOL mais completo.
 */

static void emitir_runtime_copy_n(void) {
    printf("@Runtime_copy_n(src: ptr<any>, n: int): ptr<any> {\n");

    printf("  rcp_out: ptr<any> = alloc n;\n");
    printf("  rcp_zero: int = const 0;\n");
    printf("  rcp_one: int = const 1;\n");

    printf("  rcp_slot: ptr<int> = alloc rcp_one;\n");
    printf("  store rcp_slot rcp_zero;\n\n");

    printf(".rcp_loop:\n");
    printf("  rcp_i: int = load rcp_slot;\n");
    printf("  rcp_continue: bool = lt rcp_i n;\n");
    printf("  br rcp_continue .rcp_body .rcp_end;\n\n");

    printf(".rcp_body:\n");
    printf("  rcp_src_ptr: ptr<any> = ptradd src rcp_i;\n");
    printf("  rcp_dst_ptr: ptr<any> = ptradd rcp_out rcp_i;\n");

    /*
     * O valor pode ser int, bool ou ptr<any>.
     * Por isso carregamos como any.
     */
    printf("  rcp_value: any = load rcp_src_ptr;\n");
    printf("  store rcp_dst_ptr rcp_value;\n");

    printf("  rcp_next: int = add rcp_i rcp_one;\n");
    printf("  store rcp_slot rcp_next;\n");
    printf("  jmp .rcp_loop;\n\n");

    printf(".rcp_end:\n");
    printf("  free rcp_slot;\n");
    printf("  ret rcp_out;\n");
    printf("}\n\n");
}

static void emitir_runtime_object_copy(void) {
    LayoutClasse *layout;
    char *offset_tag;
    char *endereco_tag;
    char *tag_dinamica;

    printf("@Object_copy(self: ptr<any>): ptr<any> {\n");

    offset_tag = gerar_temporario();
    endereco_tag = gerar_temporario();
    tag_dinamica = gerar_temporario();

    printf("  %s: int = const %d;\n", offset_tag, OFFSET_TAG);
    printf("  %s: ptr<any> = ptradd self %s;\n",
           endereco_tag, offset_tag);
    printf("  %s: int = load %s;\n",
           tag_dinamica, endereco_tag);

    layout = layouts_classes;

    while (layout != NULL) {
        CGValue *tag_esperada;
        char *comparacao;
        char *label_match;
        char *label_next;
        char *tamanho_copia;
        char *resultado;

        tag_esperada = emitir_int_bruto(layout->tag);
        comparacao = gerar_temporario();
        label_match = gerar_label("copy_match");
        label_next = gerar_label("copy_next");

        printf("  %s: bool = eq %s %s;\n",
               comparacao,
               tag_dinamica,
               tag_esperada->nome);

        printf("  br %s %s %s;\n\n",
               comparacao,
               label_match,
               label_next);

        printf("%s:\n", label_match);

        tamanho_copia = gerar_temporario();

        /*
         * Int e Bool possuem:
         * [tag | payload]
         */
        if (strcmp(layout->nome, "Int") == 0 ||
            strcmp(layout->nome, "Bool") == 0) {

            printf("  %s: int = const 2;\n", tamanho_copia);

        /*
         * String possui:
         * [tag | tamanho | caracteres...]
         */
        } else if (strcmp(layout->nome, "String") == 0) {
            char *offset_len = gerar_temporario();
            char *endereco_len = gerar_temporario();
            char *comprimento = gerar_temporario();
            char *dois = gerar_temporario();

            printf("  %s: int = const 1;\n", offset_len);
            printf("  %s: ptr<any> = ptradd self %s;\n",
                   endereco_len, offset_len);
            printf("  %s: int = load %s;\n",
                   comprimento, endereco_len);
            printf("  %s: int = const 2;\n", dois);
            printf("  %s: int = add %s %s;\n",
                   tamanho_copia, comprimento, dois);

            free(offset_len);
            free(endereco_len);
            free(comprimento);
            free(dois);

        } else {
            /*
             * Object, IO e classes do usuário.
             * layout->tamanho já inclui a tag.
             */
            printf("  %s: int = const %d;\n",
                   tamanho_copia,
                   layout->tamanho);
        }

        resultado = gerar_temporario();

        printf("  %s: ptr<any> = call @Runtime_copy_n self %s;\n",
               resultado,
               tamanho_copia);

        printf("  ret %s;\n\n", resultado);

        printf("%s:\n", label_next);

        valor_liberar(tag_esperada);

        free(comparacao);
        free(label_match);
        free(label_next);
        free(tamanho_copia);
        free(resultado);

        layout = layout->proximo;
    }

    /*
     * TAG_VOID ou tag desconhecida.
     */
    printf("  copy_abort: ptr<any> = call @Object_abort self;\n");
    printf("  ret copy_abort;\n");
    printf("}\n\n");

    free(offset_tag);
    free(endereco_tag);
    free(tag_dinamica);
}

static void emitir_runtime_basico(void) {
    char *um;
    char *endereco;
    char *comprimento;
    char *zero;

    /* Temporarios usados por @IO_out_string. */
    char *indice_slot;
    char *label_loop;
    char *label_body;
    char *label_end;
    char *indice;
    char *condicao;
    char *base_caractere;
    char *deslocamento;
    char *endereco_caractere;
    char *codigo;
    char *caractere;
    char *proximo_indice;

    printf("# ================================================================\n");
    printf("# Runtime minimo de Object, IO e String\n");
    printf("# ================================================================\n\n");

    printf("@Object_abort(self: ptr<any>): ptr<any> {\n");
    printf("  abort_um: int = const 1;\n");
    printf("  abort_zero: int = const 0;\n");
    printf("  abort_erro: int = div abort_um abort_zero;\n");
    printf("  ret self;\n");
    printf("}\n\n");

    emitir_runtime_copy_n();
    emitir_runtime_string_equal();
    emitir_runtime_object_type_name();
    emitir_runtime_object_copy();

    printf("@Runtime_box_int(x: int): ptr<any> {\n");
    printf("  rb_tam: int = const 2;\n");
    printf("  rb_obj: ptr<any> = alloc rb_tam;\n");

    printf("  rb_tag: int = const 1;\n");
    printf("  rb_off0: int = const 0;\n");
    printf("  rb_end0: ptr<any> = ptradd rb_obj rb_off0;\n");
    printf("  store rb_end0 rb_tag;\n");

    printf("  rb_off1: int = const 1;\n");
    printf("  rb_end1: ptr<any> = ptradd rb_obj rb_off1;\n");
    printf("  store rb_end1 x;\n");

    printf("  ret rb_obj;\n");
    printf("}\n\n");

    printf("@Runtime_empty_string(): ptr<any> {\n");

    printf("  es_two: int = const 2;\n");
    printf("  es_zero: int = const 0;\n");
    printf("  es_one: int = const 1;\n");

    printf("  es_obj: ptr<any> = alloc es_two;\n");

    printf("  es_tag: int = const %d;\n",
        buscar_layout("String")->tag);

    printf("  es_p0: ptr<any> = ptradd es_obj es_zero;\n");
    printf("  store es_p0 es_tag;\n");

    printf("  es_p1: ptr<any> = ptradd es_obj es_one;\n");
    printf("  store es_p1 es_zero;\n");

    printf("  ret es_obj;\n");
    printf("}\n\n");

    printf("@IO_out_int(self: ptr<any>, x: ptr<any>): ptr<any> {\n");
    printf("  v_off: int = const 1;\n");
    printf("  v_end: ptr<any> = ptradd x v_off;\n");
    printf("  v_num: int = load v_end;\n");
    printf("  print v_num;\n");
    printf("  ret self;\n");
    printf("}\n\n");

    /*
     * Representacao adotada por gerar_string():
     *   x[0]     -> tag de String;
     *   x[1]     -> tamanho;
     *   x[2 + i] -> codigo do caractere i.
     *
     * A rotina percorre os caracteres da String, converte cada codigo int
     * para char com int2char e chama print. O print padrao do Bril acrescenta
     * uma quebra de linha por chamada; portanto esta etapa comprova a
     * travessia da String, mas ainda nao reproduz a saida continua de COOL.
     */
    printf("@IO_out_string(self: ptr<any>, x: ptr<any>): ptr<any> {\n");
    printf("  # String: [0] tag | [1] tamanho | [2...] codigos dos caracteres.\n");
    printf("  # print em Bril finaliza cada caractere com quebra de linha.\n");

    {
        LayoutClasse *layout_string = buscar_layout("String");
        int tag_string = layout_string == NULL ? 3 : layout_string->tag;

        printf("  ios_zero: int = const 0;\n");
        printf("  ios_tag_esperada: int = const %d;\n", tag_string);

        printf("  ios_tag_ptr: ptr<any> = ptradd x ios_zero;\n");
        printf("  ios_tag: int = load ios_tag_ptr;\n");

        printf("  ios_eh_string: bool = eq ios_tag ios_tag_esperada;\n");
        printf("  br ios_eh_string .ios_ok .ios_erro;\n\n");

        printf(".ios_erro:\n");
        printf("  ios_abort: ptr<any> = call @Object_abort x;\n");
        printf("  ret ios_abort;\n\n");

        printf(".ios_ok:\n");
    }

    um = gerar_temporario();
    endereco = gerar_temporario();
    comprimento = gerar_temporario();
    zero = gerar_temporario();
    indice_slot = gerar_temporario();

    label_loop = gerar_label("out_string_loop");
    label_body = gerar_label("out_string_body");
    label_end = gerar_label("out_string_end");

    indice = gerar_temporario();
    condicao = gerar_temporario();
    base_caractere = gerar_temporario();
    deslocamento = gerar_temporario();
    endereco_caractere = gerar_temporario();
    codigo = gerar_temporario();
    caractere = gerar_temporario();
    proximo_indice = gerar_temporario();

    /* Le tamanho em x[1] e cria um slot mutavel para o indice i. */
    printf("  %s: int = const 1;\n", um);
    printf("  %s: ptr<any> = ptradd x %s;\n", endereco, um);
    printf("  %s: int = load %s;\n", comprimento, endereco);
    printf("  %s: int = const 0;\n", zero);
    printf("  %s: ptr<int> = alloc %s;\n", indice_slot, um);
    printf("  store %s %s;\n\n", indice_slot, zero);

    printf("%s:\n", label_loop);
    printf("  %s: int = load %s;\n", indice, indice_slot);
    printf("  %s: bool = lt %s %s;\n", condicao, indice, comprimento);
    printf("  br %s %s %s;\n\n", condicao, label_body, label_end);

    printf("%s:\n", label_body);
    printf("  %s: int = const 2;\n", base_caractere);
    printf("  %s: int = add %s %s;\n",
           deslocamento, indice, base_caractere);
    printf("  %s: ptr<any> = ptradd x %s;\n",
           endereco_caractere, deslocamento);
    printf("  %s: int = load %s;\n", codigo, endereco_caractere);
    printf("  %s: char = int2char %s;\n", caractere, codigo);
    printf("  print %s;\n", caractere);
    printf("  %s: int = add %s %s;\n", proximo_indice, indice, um);
    printf("  store %s %s;\n", indice_slot, proximo_indice);
    printf("  jmp %s;\n\n", label_loop);

    printf("%s:\n", label_end);
    printf("  free %s;\n", indice_slot);
    printf("  ret self;\n");
    printf("}\n\n");

    free(um);
    free(endereco);
    free(comprimento);
    free(zero);
    free(indice_slot);
    free(label_loop);
    free(label_body);
    free(label_end);
    free(indice);
    free(condicao);
    free(base_caractere);
    free(deslocamento);
    free(endereco_caractere);
    free(codigo);
    free(caractere);
    free(proximo_indice);

    printf("@IO_in_int(self: ptr<any>): ptr<any> {\n");
    printf("  in_zero: int = const 0;\n");
    printf("  in_resultado: ptr<any> = call @Runtime_box_int in_zero;\n");
    printf("  ret in_resultado;\n");
    printf("}\n\n");

    printf("@IO_in_string(self: ptr<any>): ptr<any> {\n");
    printf("  in_string_result: ptr<any> = call @Runtime_empty_string;\n");
    printf("  ret in_string_result;\n");
    printf("}\n\n");

    printf("@String_length(self: ptr<any>): ptr<any> {\n");
    um = gerar_temporario();
    endereco = gerar_temporario();
    comprimento = gerar_temporario();
    printf("  %s: int = const 1;\n", um);
    printf("  %s: ptr<any> = ptradd self %s;\n", endereco, um);
    printf("  %s: int = load %s;\n", comprimento, endereco);
    printf("  sl_resultado: ptr<any> = call @Runtime_box_int %s;\n", comprimento);
    printf("  ret sl_resultado;\n");
    printf("}\n\n");
    free(um);
    free(endereco);
    free(comprimento);

    emitir_runtime_string_concat();
    emitir_runtime_string_substr();
}

static void gerar_lista(ASTNode *primeiro) {
    ASTNode *atual = primeiro;

    while (atual != NULL) {
        gerar_codigo(atual);
        atual = atual->proximo;
    }
}

static void gerar_classe(ASTNode *no) {
    const char *classe_anterior = classe_atual;
    ASTNode *feature;
    LayoutClasse *layout;

    classe_atual = no->dados.classe.nome_classe;
    fprintf(stderr, "[codegen] classe: %s\n", classe_atual);
    layout = buscar_layout(classe_atual);

    printf("# ================================================================\n");
    printf("# Classe COOL: %s | pai: %s | tag: %d | celulas: %d\n",
           classe_atual,
           normalizar_pai(no->dados.classe.nome_pai),
           layout == NULL ? -1 : layout->tag,
           layout == NULL ? -1 : layout->tamanho);
    printf("# ================================================================\n\n");

    feature = no->dados.classe.lista_features;
    while (feature != NULL) {
        if (feature->tipo == NODE_METODO) {
            gerar_metodo(feature);
        } else if (feature->tipo == NODE_ATRIBUTO) {
            gerar_atributo(feature);
        }
        feature = feature->proximo;
    }

    classe_atual = classe_anterior;
}

static void gerar_metodo(ASTNode *no) {
    fprintf(stderr, "[codegen] metodo: %s.%s\n",
        classe_atual,
        no->dados.metodo.nome_metodo);

    ASTNode *formal;
    CGValue *corpo;
    const char *tipo_retorno_cool;
    const char *tipo_retorno_bril;
    CGValue *self;

    if (classe_atual == NULL) {
        erro_codegen(no, "metodo sem classe atual.");
        return;
    }

    tipo_retorno_cool = resolver_self_type(no->dados.metodo.tipo_retorno);
    tipo_retorno_bril = tipo_bril_do_cool(tipo_retorno_cool);

    printf("@%s_%s(self: ptr<any>",
           classe_atual, no->dados.metodo.nome_metodo);

    formal = no->dados.metodo.lista_formais;
    while (formal != NULL) {
        printf(", %s: %s",
               formal->dados.formal.nome_parametro,
               tipo_bril_do_cool(formal->dados.formal.tipo_parametro));
        formal = formal->proximo;
    }

    printf("): %s {\n", tipo_retorno_bril);

    escopo_push();
    self = valor_criar("self", "ptr<any>", classe_atual);
    escopo_definir_valor("self", self);
    valor_liberar(self);

    formal = no->dados.metodo.lista_formais;
    while (formal != NULL) {
        CGValue *valor_formal = valor_criar(
            formal->dados.formal.nome_parametro,
            tipo_bril_do_cool(formal->dados.formal.tipo_parametro),
            resolver_self_type(formal->dados.formal.tipo_parametro)
        );

        escopo_definir_local(formal->dados.formal.nome_parametro, valor_formal);
        valor_liberar(valor_formal);
        formal = formal->proximo;
    }

    corpo = gerar_expressao(no->dados.metodo.corpo);

    if (corpo != NULL) {
        if (strcmp(corpo->tipo_bril, tipo_retorno_bril) != 0) {
            erro_codegen(no, "corpo do metodo gerou tipo Bril diferente do retorno declarado.");
        }
        printf("  ret %s;\n", corpo->nome);
        valor_liberar(corpo);
    } else if (strcmp(tipo_retorno_bril, "int") == 0) {
        CGValue *padrao = emitir_int_bruto(0);
        printf("  ret %s;\n", padrao->nome);
        valor_liberar(padrao);
    } else if (strcmp(tipo_retorno_bril, "bool") == 0) {
        CGValue *padrao = emitir_bool_bruto(0);
        printf("  ret %s;\n", padrao->nome);
        valor_liberar(padrao);
    } else {
        /* Mantem o arquivo Bril estruturalmente valido em caso de erro. */
        printf("  ret self;\n");
    }

    escopo_pop();
    printf("}\n\n");
}

static void gerar_atributo(ASTNode *no) {
    LayoutClasse *layout = buscar_layout(classe_atual);
    CampoLayout *campo = buscar_campo(layout, no->dados.atributo.nome_atributo);

    if (campo != NULL) {
        printf("# atributo %s : %s -> offset %d\n",
               campo->nome, campo->tipo_cool, campo->offset);
    }
}

static void gerar_formal(ASTNode *no) {
    (void)no;
    /* Formais sao emitidos diretamente na assinatura de gerar_metodo(). */
}

static void gerar_expressao_como_comando(ASTNode *no) {
    CGValue *resultado = gerar_expressao(no);
    valor_liberar(resultado);
}

/* Visitor publico. */
void gerar_codigo(ASTNode *no) {
    if (no == NULL) {
        return;
    }

    switch (no->tipo) {
        case NODE_PROGRAMA:
            erro_codegen(no, "NODE_PROGRAMA nao e usado pelo parser atual.");
            break;

        case NODE_CLASSE:
            gerar_classe(no);
            break;

        case NODE_METODO:
            gerar_metodo(no);
            break;

        case NODE_ATRIBUTO:
            gerar_atributo(no);
            break;

        case NODE_FORMAL:
            gerar_formal(no);
            break;

        case NODE_INTEIRO:
        case NODE_STRING:
        case NODE_BOOLEANO:
        case NODE_IDENTIFICADOR:
        case NODE_ARITMETICO:
        case NODE_RELACIONAL:
        case NODE_NOT:
        case NODE_ISVOID:
        case NODE_COMPLEMENTO:
        case NODE_ATRIBUICAO:
        case NODE_IF:
        case NODE_WHILE:
        case NODE_LET:
        case NODE_BLOCO:
        case NODE_NEW:
        case NODE_DISPATCH_IMPLICITO:
        case NODE_DISPATCH_EXPLICITO:
        case NODE_CASE:
        case NODE_CASE_BRANCH:
            gerar_expressao_como_comando(no);
            break;

        case NODE_LET_VAR:
            erro_codegen(no, "NODE_LET_VAR deve ser tratado dentro de NODE_LET.");
            break;

        default:
            erro_codegen(no, "tipo de no desconhecido no visitor.");
            break;
    }
}

static void emitir_funcao_main(void) {
    LayoutClasse *layout_main = buscar_layout("Main");
    LayoutClasse *classe_implementacao = NULL;
    ASTNode *metodo_main;
    CGValue *objeto_main;
    CGValue *resultado;
    char *nome_funcao;
    const char *tipo_retorno;

    printf("@main() {\n");

    if (layout_main == NULL || layout_main->embutida) {
        fprintf(stderr, "Codegen: classe Main nao encontrada.\n");
        printf("  ret;\n");
        printf("}\n");
        return;
    }

    metodo_main = buscar_metodo_declarado(layout_main, "main", &classe_implementacao);
    if (metodo_main == NULL || classe_implementacao == NULL) {
        fprintf(stderr, "Codegen: metodo Main.main nao encontrado.\n");
        printf("  ret;\n");
        printf("}\n");
        return;
    }

    objeto_main = gerar_objeto_novo("Main", NULL);
    if (objeto_main == NULL) {
        printf("  ret;\n");
        printf("}\n");
        return;
    }

    nome_funcao = montar_nome_funcao(classe_implementacao->nome, "main");
    tipo_retorno = tipo_retorno_efetivo(metodo_main->dados.metodo.tipo_retorno,
                                        objeto_main);

    {
        char *nome_resultado = gerar_temporario();
        printf("  %s: %s = call %s %s;\n",
               nome_resultado, tipo_bril_do_cool(tipo_retorno),
               nome_funcao, objeto_main->nome);
        resultado = valor_criar(nome_resultado, tipo_bril_do_cool(tipo_retorno),
                                tipo_retorno);
        free(nome_resultado);
    }

    /* A funcao de entrada Bril nao precisa devolver o resultado de Main.main. */
    printf("  ret;\n");
    printf("}\n");

    valor_liberar(resultado);
    valor_liberar(objeto_main);
    free(nome_funcao);
}

/* ========================================================================== */
/*                              PONTO DE ENTRADA                               */
/* ========================================================================== */

void iniciar_geracao(ASTNode *raiz) {
    contador_temporarios = 0;
    contador_labels = 0;
    erros_codegen = 0;
    classe_atual = NULL;

    while (escopo_codegen != NULL) {
        escopo_pop();
    }

    preparar_layouts(raiz);

    printf("# Arquivo Bril gerado pelo compilador COOL\n");
    printf("# Objeto: ptr<any> | offset 0: tag | offsets 1+: atributos Int\n");
    printf("# Dispatch: chamada direta @Classe_metodo(self, argumentos...)\n\n");
    
    fprintf(stderr, "[codegen] iniciando runtime\n");
    emitir_runtime_basico();
    fflush(stdout);

    /*
    * Emite os construtores antes das classes.
    * Eles podem chamar outros construtores mesmo que a função chamada
    * apareça posteriormente no arquivo Bril.
    */
    emitir_construtores();


    fprintf(stderr, "[codegen] gerando classes\n");
    gerar_lista(raiz);
    fflush(stdout);

    fprintf(stderr, "[codegen] gerando dispatchers\n");
    emitir_dispatchers_dinamicos(raiz);
    fflush(stdout);

    fprintf(stderr, "[codegen] gerando main\n");
    emitir_funcao_main();
    fflush(stdout);

    fprintf(stderr, "[codegen] finalizado\n");

    /*emitir_runtime_basico();
    gerar_lista(raiz);
    emitir_dispatchers_dinamicos(raiz);
    emitir_funcao_main();*/

    if (erros_codegen > 0) {
        fprintf(stderr, "Codegen finalizado com %d erro(s)/recurso(s) pendente(s).\n",
                erros_codegen);
    }

    limpar_layouts();
}
