# CompiladorEmC - Compilador da Linguagem COOL

Um **compilador completo** implementado em C puro que traduz programas escritos na linguagem **COOL** (Classroom Object-Oriented Language) para código executável com validação semântica rigorosa.

## 🎯 Visão Geral

Este projeto implementa as principais etapas de um compilador real:

- **Análise Léxica** — tokenização e reconhecimento de palavras-chave, números, strings
- **Análise Sintática** — construção de uma Árvore Sintática Abstrata (AST) usando parsing descendente
- **Análise Semântica** — verificação de tipos, herança, escopo e validade de métodos e atributos
- **Geração de Código** — tradução para código intermediário/executável com suporte a strings

O compilador valida programas COOL e reporta erros com informações de linha e coluna, funcionando como uma ferramenta educacional de alta qualidade.

## 📁 Estrutura do Projeto

```
.
├── LEX.c / LEX.h           # Análise Léxica — tokenização
├── parser.c / parser.h     # Análise Sintática — parsing descendente
├── ast.c / ast.h           # Estrutura de dados e impressão da AST
├── semantic.c / semantic.h # Análise Semântica — type checking
├── codegen.c / codegen.h   # Geração de Código — tradução e emissão
├── symtab.c / symtab.h     # Tabela de Símbolos — gerenciamento de escopo
├── main.c                  # Ponto de entrada
├── arquivo.cl              # Arquivo de teste COOL
├── testePJ.cl              # Arquivo de teste adicional
├── Docs e material/        # Material de referência (COOL Manual, Compiler Design)
└── meu_compilador          # Executável compilado
```

### Componentes Principais

**`LEX.c`** — Análise Léxica  
- Lê caractere por caractere do arquivo de entrada
- Reconhece palavras-chave, identificadores, números, strings, operadores e símbolos
- Retorna tokens com localização (linha, coluna)

**`parser.c`** — Análise Sintática  
- Implementa parsing descendente recursivo
- Segue uma hierarquia de precedência (expressões aritméticas, atribuição, controle de fluxo)
- Constrói a AST seguindo a estrutura de classes, métodos e expressões COOL

**`ast.c`** — Árvore Sintática Abstrata  
- Define nós para cada construção COOL (classes, métodos, atributos, expressões, controle de fluxo)
- Implementa funções para criar, imprimir e liberar a árvore
- Usa `union` para armazenar dados heterogêneos eficientemente

**`semantic.c`** — Análise Semântica  
- Valida hierarquia de classes e herança
- Verifica tipos de expressões, atribuições e retornos de métodos
- Gerencia escopo de variáveis (globais, atributos, locais)
- Detecta duplicações e indefinições de símbolos
- Otimiza busca de métodos com cache inteligente

**`codegen.c`** — Geração de Código  
- Traduz a AST validada para código intermediário/executável
- Suporte completo a strings e operações sobre objetos
- Aloca e gerencia memória para instâncias de classe
- Emite código para dispatch de métodos (virtual table)
- Gera código para construções de controle de fluxo

**`symtab.c`** — Tabela de Símbolos  
- Armazena informações sobre classes, métodos, atributos e variáveis
- Suporta busca rápida por nome
- Integra-se com análise semântica e geração de código para validação de tipos

## 🚀 Como Usar

### Compilação

```bash
gcc -o meu_compilador main.c LEX.c parser.c ast.c semantic.c codegen.c symtab.c -lm
```

### Execução

Edite `arquivo.cl` com seu código COOL e execute:

```bash
./meu_compilador
```

### Saída

O compilador realiza todas as etapas e imprime:

1. **Erros de lexing** (se houver) — com linha e coluna
2. **Erros de parsing** (se houver) — com linha e coluna
3. **Árvore Sintática** (se parsing bem-sucedido) — impressão hierárquica dos nós
4. **Tabelas de símbolos** — informações sobre classes, métodos e atributos definidos
5. **Erros semânticos** (se houver) — detalhes sobre type checking, herança, escopo
6. **Código gerado** (se bem-sucedido) — representação intermediária ou executável

Exemplo:

```
Parsing realizado com sucesso!

--- ÁRVORE SINTÁTICA ---
PROGRAMA
  CLASSE: Main (herança: IO)
    ATRIBUTO: a (tipo: Bazz)
    MÉTODO: main() -> String
      ...

----------
Analise semantica realizada com sucesso!

--- CÓDIGO GERADO ---
[instruções de código intermediário]
```

## 📝 Exemplo de Código COOL

O arquivo `arquivo.cl` contém um exemplo complexo com:

- **Classes com herança** — `class Foo inherits Bazz`
- **Atributos com inicialização** — `a : Razz <- new Razz`
- **Métodos com parâmetros e retorno** — `doh() : Int { ... }`
- **Expressões** — operações aritméticas, lógicas, comparações
- **Controle de fluxo** — `if-then-else`, `while`, `let`, `case`
- **Dispatch (chamadas de método)** — `self.method()`, `obj@Type.method()`
- **Strings** — Manipulação e operações com strings

```cool
class Bazz inherits IO {
  h : Int <- 1;
  msg : String <- "Hello";
  printh() : Int { { out_int(h); 0; } };
  printmsg() : Object { { out_string(msg); } };
};

class Main inherits IO {
  a : Bazz <- new Bazz;
  main(): String { { a.printmsg(); a.printh(); "do nothing"; } };
};
```

## 🔍 Recursos Suportados

✅ **Definição de Classes**  
- Herança simples (sem múltipla herança)
- Atributos com tipos e inicialização opcional

✅ **Definição de Métodos**  
- Parâmetros formais com tipos
- Tipo de retorno explícito
- Corpo com expressões

✅ **Expressões**  
- Aritmética: `+`, `-`, `*`, `/`
- Relacional: `<`, `<=`, `>`, `>=`, `=`
- Lógica: `not`, `and` (implícito em `;`)
- Unária: `~` (complemento), `isvoid`, `new`

✅ **Controle de Fluxo**  
- Condicional: `if cond then expr1 else expr2 fi`
- Iteração: `while cond loop expr pool`
- Blocos: `{ expr1; expr2; ... }`
- Atribuição: `var <- valor`

✅ **Construções Avançadas**  
- `let x : Type <- init in expr` — variáveis locais
- `case expr of x : Type => expr; ... esac` — pattern matching
- `obj.method(args)` — chamada de método (dispatch implícito)
- `obj@Type.method(args)` — dispatch estático

✅ **Análise Semântica Completa**  
- Validação de tipos
- Verificação de ciclos em herança
- Detecção de métodos/atributos indefinidos
- Type checking de argumentos e retornos
- Validação de escopo de variáveis

✅ **Geração de Código**  
- Tradução de AST para código intermediário
- Suporte a strings e operações sobre objetos
- Gerenciamento automático de memória
- Dispatch virtual de métodos

## 📚 Material de Referência

- `Docs e material/cool-manual.pdf` — Especificação completa da linguagem COOL
- `Docs e material/Modern_Compiler_Implementation_in_Java.pdf` — Técnicas avançadas de design de compiladores

## 🛠️ Melhorias Futuras

- [ ] Otimizações (constant folding, dead code elimination)
- [ ] Suporte a múltiplas unidades de compilação
- [ ] Testes automatizados com suite de casos de teste
- [ ] Mensagens de erro mais amigáveis e sugestões de correção
- [ ] Integração com ferramentas de debugging
- [ ] Backend de compilação para arquitetura nativa (x86-64, ARM)
- [ ] Análise de alcançabilidade de código
- [ ] Advertências sobre variáveis não utilizadas

## 📊 Status do Projeto

- ✅ **Análise Léxica** — Completa
- ✅ **Análise Sintática** — Completa com AST
- ✅ **Análise Semântica** — Completa com type checking rigoroso
- ✅ **Geração de Código** — Finalizada (v1)
- 🔄 **Otimizações** — Em desenvolvimento
- 📋 **Testes** — Em progresso

## 👤 Autores

**jessffc01** — Projeto acadêmico de Compiladores  
**PedroLucas** — Projeto acadêmico de Compiladores  
**YgorGenaio** — Projeto acadêmico de Compiladores

## 📄 Licença

Este projeto é fornecido para fins educacionais. Consulte `Docs e material/` para referências técnicas.

---

**Para começar**: Edite `arquivo.cl`, compile com `gcc` e execute. O compilador validará e reportará qualquer erro com precisão de localização, além de gerar o código intermediário.
