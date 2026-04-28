# Questão 2: Produtor-Consumidor com Memória Compartilhada (IPC)

## Visão Geral

Esta implementação usa **processos separados** que se comunicam via **POSIX Shared Memory** e **semáforos nomeados**. Esta é a abordagem clássica de IPC (Inter-Process Communication) em Sistemas Distribuídos.

### Diferença da Versão com Threads

| Aspecto | Threads (backup) | Processos (esta versão) |
|---------|------------------|-------------------------|
| Memória | Compartilhada automaticamente | Precisa de shm_open/mmap |
| Semáforos | sem_init (anônimos) | sem_open (nomeados) |
| Criação | pthread_create | fork() + execl() |
| Isolamento | Menor (mesmo processo) | Maior (processos separados) |
| IPC Real | Não | Sim |

## Arquivos

```
questao2_shm/
├── common.h              # Definições compartilhadas (estruturas, constantes)
├── controlador.c         # Programa principal (cria recursos e processos)
├── produtor.c            # Processo produtor (executável independente)
├── consumidor.c          # Processo consumidor (executável independente)
├── Makefile              # Compilação
├── executar_testes.sh    # Automação de testes
├── gerar_graficos.py     # Geração de gráficos
└── README.md             # Este arquivo
```

## Compilação

```bash
make
```

Isto gera três executáveis:
- `controlador` - Programa principal
- `produtor` - Processo produtor
- `consumidor` - Processo consumidor

## Execução

```bash
./controlador <N> <Np> <Nc> [M]
```

Onde:
- `N` = Tamanho do buffer (1 a 10000)
- `Np` = Número de processos produtores
- `Nc` = Número de processos consumidores
- `M` = Total de números a processar (default: 100000)

### Exemplos

```bash
# Teste simples
./controlador 10 2 2 1000

# Configuração do enunciado
./controlador 100 4 4 100000
```

## Como Funciona

### 1. Controlador (controlador.c)

1. Cria a memória compartilhada com `shm_open()` + `mmap()`
2. Cria semáforos nomeados com `sem_open()`
3. Inicializa estruturas de dados
4. Faz `fork()` + `execl()` para criar Np produtores
5. Faz `fork()` + `execl()` para criar Nc consumidores
6. Aguarda todos terminarem com `waitpid()`
7. Limpa recursos IPC

### 2. Produtor (produtor.c)

```
Enquanto não terminou:
    1. Verificar se já produziu M números
    2. Gerar número aleatório
    3. sem_wait(empty)     <- Aguarda posição livre
    4. sem_wait(mutex)     <- Exclusão mútua
    5. Inserir no buffer
    6. sem_post(mutex)
    7. sem_post(full)      <- Sinaliza posição ocupada
```

### 3. Consumidor (consumidor.c)

```
Enquanto não terminou:
    1. Verificar se já consumiu M números
    2. sem_wait(full)      <- Aguarda posição ocupada
    3. sem_wait(mutex)     <- Exclusão mútua
    4. Remover do buffer
    5. sem_post(mutex)
    6. sem_post(empty)     <- Sinaliza posição livre
    7. Verificar se é primo (fora da região crítica)
```

## Recursos IPC Utilizados

### Memória Compartilhada (POSIX)

```c
// Criação (controlador)
int fd = shm_open("/prodcons_shm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, sizeof(MemoriaCompartilhada));
void *ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

// Abertura (produtor/consumidor)
int fd = shm_open("/prodcons_shm", O_RDWR, 0666);
void *ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

// Limpeza
shm_unlink("/prodcons_shm");
```

### Semáforos Nomeados (POSIX)

```c
// Criação (controlador)
sem_t *sem = sem_open("/nome_sem", O_CREAT | O_EXCL, 0666, valor_inicial);

// Abertura (produtor/consumidor)
sem_t *sem = sem_open("/nome_sem", 0);

// Operações
sem_wait(sem);   // P() - decrementa ou bloqueia
sem_post(sem);   // V() - incrementa

// Limpeza
sem_close(sem);
sem_unlink("/nome_sem");
```

## Semáforos

| Nome | Tipo | Valor Inicial | Função |
|------|------|---------------|--------|
| sem_empty | Contador | N | Conta posições LIVRES |
| sem_full | Contador | 0 | Conta posições OCUPADAS |
| sem_mutex | Binário | 1 | Exclusão mútua no buffer |
| sem_pmutex | Binário | 1 | Mutex para contador de produção |
| sem_cmutex | Binário | 1 | Mutex para contador de consumo |

## Automação de Testes

```bash
chmod +x executar_testes.sh
./executar_testes.sh
```

Executa todos os cenários do enunciado e salva resultados em:
- `resultados_testes.csv`
- `resultados_testes.txt`

## Geração de Gráficos

```bash
pip install matplotlib pandas numpy
python3 gerar_graficos.py
```

## Limpeza de Recursos IPC Órfãos

Se o programa terminar abruptamente (Ctrl+C, crash), os recursos IPC podem ficar órfãos. Para limpá-los:

```bash
make limpar_ipc
```

Ou manualmente:
```bash
rm -f /dev/shm/prodcons_shm
rm -f /dev/shm/sem.prodcons_*
```

## Verificação dos Recursos IPC

Para ver os recursos IPC ativos no sistema:

```bash
# Memória compartilhada
ls -la /dev/shm/

# Semáforos
ls -la /dev/shm/sem.*
```

## Troubleshooting

### "Memória compartilhada não existe"
Execute primeiro o controlador. Produtor/consumidor não podem ser executados sozinhos.

### Programa trava
Pressione Ctrl+C e execute `make limpar_ipc` para remover recursos órfãos.

### Permissão negada
Verifique se você tem permissão para criar arquivos em `/dev/shm/`.
