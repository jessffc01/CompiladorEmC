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
static int campo_eh_suportado(const char *tipo_cool);
static char *tipo_ponteiro_para(const char *tipo_bril);
static const char *inferir_tipo_cool(ASTNode *no);

static CGValue *emitir_constante_int(int valor);
static CGValue *emitir_constante_bool(int valor);
static CGValue *emitir_valor_padrao(const char *tipo_cool, ASTNode *contexto);
static char *emitir_ptradd(const CGValue *objeto, int offset);
static void emitir_store_campo(const CGValue *objeto, int offset,
                               const CGValue *valor, ASTNode *contexto);
static CGValue *emitir_load_campo(const CGValue *objeto, CampoLayout *campo,
                                  ASTNode *contexto);

static CGValue *gerar_expressao(ASTNode *no);
static CGValue *gerar_inteiro(ASTNode *no);
static CGValue *gerar_booleano(ASTNode *no);
static CGValue *gerar_string(ASTNode *no);
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

    um = emitir_constante_int(1);
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
    tipo_cool = resolver_self_type(tipo_cool);

    if (strcmp(tipo_cool, "Int") == 0) {
        return "int";
    }

    if (strcmp(tipo_cool, "Bool") == 0) {
        return "bool";
    }

    return "ptr<int>";
}


/*
 * O objeto atual e ptr<int>; uma alocacao Bril e homogenea.
 * Portanto, nesta primeira representacao, atributos de objeto podem ser Int.
 * Bool e referencias precisam de uma camada de boxing/tagged values posterior.
 */
static int campo_eh_suportado(const char *tipo_cool) {
    tipo_cool = resolver_self_type(tipo_cool);
    return strcmp(tipo_cool, "Int") == 0;
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
            return strcmp(tipo_then, tipo_else) == 0 ? tipo_then : "Object";
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
        default:
            return "Object";
    }
}

/* ========================================================================== */
/*                          EMISSAO DE INSTRUCOES                              */
/* ========================================================================== */

static CGValue *emitir_constante_int(int valor) {
    char *nome = gerar_temporario();
    CGValue *resultado;

    printf("  %s: int = const %d;\n", nome, valor);
    resultado = valor_criar(nome, "int", "Int");
    free(nome);
    return resultado;
}

static CGValue *emitir_constante_bool(int valor) {
    char *nome = gerar_temporario();
    CGValue *resultado;

    printf("  %s: bool = const %s;\n", nome, valor ? "true" : "false");
    resultado = valor_criar(nome, "bool", "Bool");
    free(nome);
    return resultado;
}


static CGValue *emitir_valor_padrao(const char *tipo_cool, ASTNode *contexto) {
    tipo_cool = resolver_self_type(tipo_cool);

    if (strcmp(tipo_cool, "Int") == 0) {
        return emitir_constante_int(0);
    }

    if (strcmp(tipo_cool, "Bool") == 0) {
        return emitir_constante_bool(0);
    }

    /*
     * O Bril de memoria nao possui uma constante nula tipada.
     * Logo, "void" de COOL ainda precisa de uma extensao de runtime.
     */
    erro_codegen(contexto,
                 "valor padrao de referencia (void) ainda nao possui representacao no runtime Bril.");
    return NULL;
}

static char *emitir_ptradd(const CGValue *objeto, int offset) {
    CGValue *offset_bril;
    char *endereco;

    offset_bril = emitir_constante_int(offset);
    endereco = gerar_temporario();

    printf("  %s: ptr<int> = ptradd %s %s;\n",
           endereco, objeto->nome, offset_bril->nome);

    valor_liberar(offset_bril);
    return endereco;
}

static void emitir_store_campo(const CGValue *objeto, int offset,
                               const CGValue *valor, ASTNode *contexto) {
    char *endereco;

    if (objeto == NULL || valor == NULL) {
        erro_codegen(contexto, "tentativa de store com valor inexistente.");
        return;
    }

    if (strcmp(objeto->tipo_bril, "ptr<int>") != 0) {
        erro_codegen(contexto, "objeto nao possui representacao ptr<int>.");
        return;
    }

    if (strcmp(valor->tipo_bril, "int") != 0) {
        erro_codegen(contexto,
                     "o layout ptr<int> desta etapa permite store somente de Int.");
        return;
    }

    endereco = emitir_ptradd(objeto, offset);
    printf("  store %s %s;\n", endereco, valor->nome);
    free(endereco);
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

    if (!campo_eh_suportado(campo->tipo_cool)) {
        erro_codegen(contexto,
                     "leitura de atributo de referencia ainda nao e suportada pelo layout ptr<int>.");
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
        case NODE_CASE_BRANCH:
            erro_codegen(no, "NODE_CASE ainda nao foi implementado no gerador Bril.");
            return NULL;

        default:
            erro_codegen(no, "tipo de expressao nao suportado.");
            return NULL;
    }
}

static CGValue *gerar_inteiro(ASTNode *no) {
    long numero = strtol(no->dados.valor_lexico, NULL, 10);
    return emitir_constante_int((int)numero);
}

static CGValue *gerar_booleano(ASTNode *no) {
    return emitir_constante_bool(no->dados.valor_booleano);
}

/*
 * String simplificada no heap:
 *   offset 0 -> tag String
 *   offset 1 -> tamanho
 *   offset 2.. -> codigo ASCII de cada caractere
 *
 * Isso permite passar "Racao" como ptr<int> para um metodo do usuario.
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
    tamanho = emitir_constante_int((int)tamanho_texto + 2);

    {
        char *nome_objeto = gerar_temporario();

        printf("  %s: ptr<int> = alloc %s;\n", nome_objeto, tamanho->nome);

        objeto = valor_criar(nome_objeto, "ptr<int>", "String");
        free(nome_objeto);
    }

    tag = emitir_constante_int(layout_string->tag);
    emitir_store_campo(objeto, 0, tag, no);
    valor_liberar(tag);

    {
        CGValue *comprimento = emitir_constante_int((int)tamanho_texto);

        emitir_store_campo(objeto, 1, comprimento, no);
        valor_liberar(comprimento);
    }

    for (i = 0; texto[i] != '\0'; i++) {
        unsigned char codigo;
        CGValue *caractere;

        codigo = decodificar_caractere_string(texto, &i);

        caractere = emitir_constante_int((int)codigo);
        emitir_store_campo(objeto, offset, caractere, no);

        valor_liberar(caractere);
        offset++;
    }

    valor_liberar(tamanho);
    return objeto;
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

    if (esquerdo == NULL || direito == NULL || opcode == NULL) {
        erro_codegen(no, "operacao aritmetica invalida.");
        valor_liberar(esquerdo);
        valor_liberar(direito);
        return NULL;
    }

    if (strcmp(esquerdo->tipo_bril, "int") != 0 ||
        strcmp(direito->tipo_bril, "int") != 0) {
        erro_codegen(no, "operacao aritmetica exige dois inteiros.");
        valor_liberar(esquerdo);
        valor_liberar(direito);
        return NULL;
    }

    resultado_nome = gerar_temporario();

    /* Exato Bril: v3: int = add v1 v2; */
    printf("  %s: int = %s %s %s;\n",
           resultado_nome, opcode, esquerdo->nome, direito->nome);

    resultado = valor_criar(resultado_nome, "int", "Int");
    free(resultado_nome);
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

    if (esquerdo == NULL || direito == NULL || opcode == NULL) {
        erro_codegen(no, "operacao relacional invalida.");
        valor_liberar(esquerdo);
        valor_liberar(direito);
        return NULL;
    }

    if (strcmp(esquerdo->tipo_bril, direito->tipo_bril) != 0) {
        erro_codegen(no, "comparacao entre valores com tipos Bril diferentes.");
        valor_liberar(esquerdo);
        valor_liberar(direito);
        return NULL;
    }

    resultado_nome = gerar_temporario();

    /* Exato Bril: v3: bool = lt v1 v2; */
    printf("  %s: bool = %s %s %s;\n",
           resultado_nome, opcode, esquerdo->nome, direito->nome);

    resultado = valor_criar(resultado_nome, "bool", "Bool");
    free(resultado_nome);
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

    if (no->tipo == NODE_NOT) {
        if (strcmp(operando->tipo_bril, "bool") != 0) {
            erro_codegen(no, "not exige um Bool.");
            valor_liberar(operando);
            return NULL;
        }

        nome = gerar_temporario();
        printf("  %s: bool = not %s;\n", nome, operando->nome);
        resultado = valor_criar(nome, "bool", "Bool");
        free(nome);
        valor_liberar(operando);
        return resultado;
    }

    if (no->tipo == NODE_COMPLEMENTO) {
        CGValue *zero;

        if (strcmp(operando->tipo_bril, "int") != 0) {
            erro_codegen(no, "~ exige um Int.");
            valor_liberar(operando);
            return NULL;
        }

        /* ~x em COOL e -x. Usamos sub para nao depender de uma opcode extra. */
        zero = emitir_constante_int(0);
        nome = gerar_temporario();
        printf("  %s: int = sub %s %s;\n", nome, zero->nome, operando->nome);
        resultado = valor_criar(nome, "int", "Int");
        free(nome);
        valor_liberar(zero);
        valor_liberar(operando);
        return resultado;
    }

    /* isvoid requer uma convencao de null/void que o runtime desta etapa nao tem. */
    erro_codegen(no, "isvoid depende da representacao de void e ainda nao foi implementado.");
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
        if (!campo_eh_suportado(campo->tipo_cool)) {
            erro_codegen(no,
                         "atribuicao em atributo de referencia nao e suportada pelo layout ptr<int>.");
            valor_liberar(direito);
            return NULL;
        }

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
    tipo_resultado_cool = inferir_tipo_cool(no->dados.no_if.bloco_then);
    tipo_resultado_bril = tipo_bril_do_cool(tipo_resultado_cool);

    condicao = gerar_expressao(no->dados.no_if.condicao);
    if (condicao == NULL || strcmp(condicao->tipo_bril, "bool") != 0) {
        erro_codegen(no, "condicao de if precisa gerar Bool.");
        valor_liberar(condicao);
        return NULL;
    }

    um = emitir_constante_int(1);
    tipo_slot = tipo_ponteiro_para(tipo_resultado_bril);
    slot = gerar_temporario();
    printf("  %s: %s = alloc %s;\n", slot, tipo_slot, um->nome);

    label_then = gerar_label("if_true");
    label_else = gerar_label("if_false");
    label_fim = gerar_label("if_end");

    printf("  br %s %s %s;\n\n", condicao->nome, label_then, label_else);

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
    if (condicao == NULL || strcmp(condicao->tipo_bril, "bool") != 0) {
        erro_codegen(no, "condicao de while precisa gerar Bool.");
        valor_liberar(condicao);
        free(label_inicio); free(label_corpo); free(label_fim);
        return NULL;
    }

    printf("  br %s %s %s;\n\n", condicao->nome, label_corpo, label_fim);

    printf("%s:\n", label_corpo);
    corpo = gerar_expressao(no->dados.no_while.corpo);
    valor_liberar(corpo);
    printf("  jmp %s;\n\n", label_inicio);

    printf("%s:\n", label_fim);

    valor_liberar(condicao);
    free(label_inicio);
    free(label_corpo);
    free(label_fim);

    /*
     * Enquanto for usado como comando dentro de um bloco, NULL e suficiente:
     * o proximo comando (ou o ultimo comando valido) fornece o valor do bloco.
     * Se ele for usado onde um valor e obrigatorio, o no pai sinalizara erro.
     */
    return NULL;
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

static CGValue *gerar_new_expr(ASTNode *no) {
    const char *tipo = no->dados.valor_lexico;

    if (strcmp(tipo, "SELF_TYPE") == 0) {
        tipo = classe_atual;
    }

    return gerar_objeto_novo(tipo, no);
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
static CGValue *gerar_objeto_novo(const char *nome_classe, ASTNode *contexto) {
    LayoutClasse *layout = buscar_layout(nome_classe);
    CGValue *tamanho;
    CGValue *objeto;
    CGValue *tag;
    CampoLayout *campo;
    const char *classe_anterior;

    if (layout == NULL) {
        erro_codegen(contexto, "classe de new nao encontrada no layout.");
        return NULL;
    }

    tamanho = emitir_constante_int(layout->tamanho);
    {
        char *nome_objeto = gerar_temporario();
        printf("  %s: ptr<int> = alloc %s;\n", nome_objeto, tamanho->nome);
        objeto = valor_criar(nome_objeto, "ptr<int>", layout->nome);
        free(nome_objeto);
    }
    valor_liberar(tamanho);

    tag = emitir_constante_int(layout->tag);
    emitir_store_campo(objeto, 0, tag, contexto);
    valor_liberar(tag);

    /* Inicializadores de atributos devem enxergar self como o novo objeto. */
    classe_anterior = classe_atual;
    classe_atual = layout->nome;
    escopo_push();
    escopo_definir_valor("self", objeto);

    campo = layout->campos;
    while (campo != NULL) {
        CGValue *valor_inicial;

        if (!campo_eh_suportado(campo->tipo_cool)) {
            erro_codegen(campo->declaracao,
                         "atributo de referencia nao e suportado no layout ptr<int> atual.");
            campo = campo->proximo;
            continue;
        }

        if (campo->declaracao != NULL &&
            campo->declaracao->dados.atributo.inicializacao != NULL) {
            valor_inicial = gerar_expressao(
                campo->declaracao->dados.atributo.inicializacao
            );
        } else {
            valor_inicial = emitir_valor_padrao(campo->tipo_cool, campo->declaracao);
        }

        if (valor_inicial != NULL) {
            emitir_store_campo(objeto, campo->offset, valor_inicial, campo->declaracao);
            valor_liberar(valor_inicial);
        }

        campo = campo->proximo;
    }

    escopo_pop();
    classe_atual = classe_anterior;
    return objeto;
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

    nome_funcao = montar_nome_funcao(classe_implementacao_nome,
                                     nome_metodo);

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

    printf("@Object_abort(self: ptr<int>): ptr<int> {\n");
    printf("  # Runtime completo deve interromper a execucao aqui.\n");
    printf("  ret self;\n");
    printf("}\n\n");

    printf("@Object_type_name(self: ptr<int>): ptr<int> {\n");
    printf("  # Runtime completo deve construir a String do nome dinamico.\n");
    printf("  ret self;\n");
    printf("}\n\n");

    printf("@Object_copy(self: ptr<int>): ptr<int> {\n");
    printf("  # Runtime completo deve copiar o objeto; por ora retorna self.\n");
    printf("  ret self;\n");
    printf("}\n\n");

    printf("@IO_out_int(self: ptr<int>, x: int): ptr<int> {\n");
    printf("  print x;\n");
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
    printf("@IO_out_string(self: ptr<int>, x: ptr<int>): ptr<int> {\n");
    printf("  # String: [0] tag | [1] tamanho | [2...] codigos dos caracteres.\n");
    printf("  # print em Bril finaliza cada caractere com quebra de linha.\n");

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
    printf("  %s: ptr<int> = ptradd x %s;\n", endereco, um);
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
    printf("  %s: ptr<int> = ptradd x %s;\n",
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

    printf("@IO_in_int(self: ptr<int>): int {\n");
    zero = gerar_temporario();
    printf("  %s: int = const 0;\n", zero);
    printf("  ret %s;\n", zero);
    printf("}\n\n");
    free(zero);

    printf("@IO_in_string(self: ptr<int>): ptr<int> {\n");
    printf("  # Stub temporario para leitura de String.\n");
    printf("  ret self;\n");
    printf("}\n\n");

    printf("@String_length(self: ptr<int>): int {\n");
    um = gerar_temporario();
    endereco = gerar_temporario();
    comprimento = gerar_temporario();
    printf("  %s: int = const 1;\n", um);
    printf("  %s: ptr<int> = ptradd self %s;\n", endereco, um);
    printf("  %s: int = load %s;\n", comprimento, endereco);
    printf("  ret %s;\n", comprimento);
    printf("}\n\n");
    free(um);
    free(endereco);
    free(comprimento);

    printf("@String_concat(self: ptr<int>, s: ptr<int>): ptr<int> {\n");
    printf("  # Stub temporario: runtime completo deve alocar a concatenacao.\n");
    printf("  ret self;\n");
    printf("}\n\n");

    printf("@String_substr(self: ptr<int>, i: int, l: int): ptr<int> {\n");
    printf("  # Stub temporario: runtime completo deve alocar a substring.\n");
    printf("  ret self;\n");
    printf("}\n\n");
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

    printf("@%s_%s(self: ptr<int>",
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
    self = valor_criar("self", "ptr<int>", classe_atual);
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
        CGValue *padrao = emitir_constante_int(0);
        printf("  ret %s;\n", padrao->nome);
        valor_liberar(padrao);
    } else if (strcmp(tipo_retorno_bril, "bool") == 0) {
        CGValue *padrao = emitir_constante_bool(0);
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
    printf("# Objeto: ptr<int> | offset 0: tag | offsets 1+: atributos Int\n");
    printf("# Dispatch: chamada direta @Classe_metodo(self, argumentos...)\n\n");

    emitir_runtime_basico();
    gerar_lista(raiz);
    emitir_funcao_main();

    if (erros_codegen > 0) {
        fprintf(stderr, "Codegen finalizado com %d erro(s)/recurso(s) pendente(s).\n",
                erros_codegen);
    }

    limpar_layouts();
}
