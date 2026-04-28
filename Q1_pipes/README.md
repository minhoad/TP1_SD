# Questão 1: Produtor-Consumidor com Pipes

## Visão Geral

Este programa implementa o padrão Produtor-Consumidor usando **pipes anônimos** para comunicação entre processos (IPC) no Linux/Unix.

## Arquitetura

```
┌─────────────────┐                    ┌─────────────────┐
│    PRODUTOR     │                    │   CONSUMIDOR    │
│  (Processo Pai) │                    │ (Processo Filho)│
│                 │                    │                 │
│  Gera números   │  ──── PIPE ────►   │  Verifica se    │
│  aleatórios     │   (write end)      │  é primo        │
│  crescentes     │                    │                 │
└─────────────────┘                    └─────────────────┘
```

## Conceitos Utilizados

### 1. Pipe (Tubo)
- Canal de comunicação **unidirecional** entre processos
- Criado com `pipe(int fd[2])`:
  - `fd[0]` = read end (ponta de leitura)
  - `fd[1]` = write end (ponta de escrita)
- Dados escritos em `fd[1]` podem ser lidos em `fd[0]`

### 2. Fork
- `fork()` cria uma cópia do processo atual
- Processo pai recebe o PID do filho
- Processo filho recebe 0
- Ambos herdam os file descriptors do pipe

### 3. Comunicação
- Mensagens de tamanho fixo (20 bytes)
- Números convertidos para strings preenchidas com zeros
- Evita problemas de representação binária entre processos

## Compilação

```bash
# Usando make
make

# Ou diretamente
gcc -Wall -o produtor_consumidor produtor_consumidor_pipes.c -lm
```

## Execução

```bash
# Executar com N números
./produtor_consumidor <quantidade>

# Exemplos e testes feitos:
./produtor_consumidor 10      # Gera 10 números
./produtor_consumidor 100     # Gera 100 números
./produtor_consumidor 1000    # Gera 1000 números
```

## Saída Esperada

```
========================================
PRODUTOR-CONSUMIDOR COM PIPES
Quantidade de números: 100
========================================

Pipe criado com sucesso.
  Read end (fd): 3
  Write end (fd): 4

[PAI] PID: 12345, Filho PID: 12346

[FILHO] PID: 12346, PPID: 12345
[CONSUMIDOR] Aguardando números...
[PRODUTOR] Iniciando produção de 100 números...
[PRODUTOR] Enviou número 1: 1
[CONSUMIDOR] Número 1: 1 -> NÃO PRIMO
...
[CONSUMIDOR] Número 5: 89 -> PRIMO
...

========================================
[CONSUMIDOR] RESUMO:
  Total de números processados: 100
  Números primos encontrados: 15
  Porcentagem de primos: 15.00%
========================================
```

## Fórmula de Geração

Os números são gerados seguindo:
- **N₀ = 1** (primeiro número é 1)
- **Nᵢ = Nᵢ₋₁ + Δ**, onde **Δ ∈ [1, 100]** (aleatório)

Isso garante que os números sejam sempre crescentes.

## Tratamento de Erros

- Verifica retorno de `pipe()`, `fork()`, `read()`, `write()`
- Usa `perror()` para mensagens de erro descritivas
- Códigos de saída apropriados (`EXIT_SUCCESS`, `EXIT_FAILURE`)
