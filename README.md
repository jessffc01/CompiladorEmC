# CompiladorEmC - Compilador da Linguagem COOL

Um **compilador completo** implementado em C puro que traduz programas escritos na linguagem **COOL** (Classroom Object-Oriented Language) para uma representação intermediária com validação semântica.

## 🎯 Visão Geral

Este projeto implementa as principais etapas de um compilador real:

- **Análise Léxica** — tokenização e reconhecimento de palavras-chave, números, strings
- **Análise Sintática** — construção de uma Árvore Sintática Abstrata (AST) usando parsing descendente
- **Análise Semântica** — verificação de tipos, herança, escopo e validade de métodos e atributos

O compilador valida programas COOL e reporta erros com informações de linha e coluna, funcionando como uma ferramenta educacional de alta qualidade.

## 📁 Estrutura do Projeto

```
.
├── LEX.c / LEX.h           # Análise Léxica — tokenização
├── parser.c / parser.h     # Análise Sintática — parsing descendente
├── ast.c / ast.h           # Estrutura de dados e impressão da AST
├── semantic.c / semantic.h # Análise Semântica — type checking
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

**`symtab.c`** — Tabela de Símbolos  
- Armazena informações sobre classes, métodos, atributos e variáveis
- Suporta busca rápida por nome
- Integra-se com a análise semântica para validação de tipos

## 🚀 Como Usar

### Compilação

```bash
gcc -o meu_compilador main.c LEX.c parser.c ast.c semantic.c symtab.c -lm
```

### Execução

Edite `arquivo.cl` com seu código COOL e execute:

```bash
./meu_compilador
```

### Saída

O compilador imprime:

1. **Erros de parsing** (se houver) — com linha e coluna
2. **Árvore Sintática** (se parsing bem-sucedido) — impressão hierárquica dos nós
3. **Tabelas de símbolos** — informações sobre classes, métodos e atributos definidos
4. **Erros semânticos** (se houver) — detalhes sobre type checking, herança, escopo

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
```

## 📝 Exemplo de Código COOL

O arquivo `arquivo.cl` contém um exemplo complexo com:

- **Classes com herança** — `class Foo inherits Bazz`
- **Atributos com inicialização** — `a : Razz <- new Razz`
- **Métodos com parâmetros e retorno** — `doh() : Int { ... }`
- **Expressões** — operações aritméticas, lógicas, comparações
- **Controle de fluxo** — `if-then-else`, `while`, `let`, `case`
- **Dispatch (chamadas de método)** — `self.method()`, `obj@Type.method()`

```cool
class Bazz inherits IO {
  h : Int <- 1;
  printh() : Int { { out_int(h); 0; } };
};

class Main inherits IO {
  a : Bazz <- new Bazz;
  main(): String { { out_string("\n"); "do nothing"; } };
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

✅ **Análise Semântica**  
- Validação de tipos
- Verificação de ciclos em herança
- Detecção de métodos/atributos indefinidos
- Type checking de argumentos e retornos

## 📚 Material de Referência

- `Docs e material/cool-manual.pdf` — Especificação completa da linguagem COOL
- `Docs e material/Modern_Compiler_Implementation_in_Java.pdf` — Técnicas avançadas de design de compiladores

## 🛠️ Melhorias Futuras

- [ ] Geração de código intermediário (three-address code)
- [ ] Otimizações (constant folding, dead code elimination)
- [ ] Suporte a múltiplas unidades de compilação
- [ ] Testes automatizados com suite de casos de teste
- [ ] Mensagens de erro mais amigáveis
- [ ] Integração com ferramentas de debugging

## 👤 Autor

**jessffc01** — Projeto acadêmico de Compiladores

## 📄 Licença

Este projeto é fornecido para fins educacionais. Consulte `Docs e material/` para referências técnicas.

---

**Para começar**: Edite `arquivo.cl`, compile com `gcc` e execute. O compilador validará e reportará qualquer erro com precisão de localização.
