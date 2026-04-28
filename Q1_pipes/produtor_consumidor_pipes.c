/**
 * =============================================================================
 * TRABALHO PRÁTICO - SISTEMAS DISTRIBUÍDOS
 * Questão 1: Produtor-Consumidor com Pipes
 * =============================================================================
 * 
 * Este programa demonstra a comunicação entre processos (IPC) usando pipes
 * anônimos (anonymous pipes) no Linux/Unix.
 * 
 * CONCEITOS IMPORTANTES:
 * 
 * 1. PIPE: É um mecanismo de IPC unidirecional que conecta a saída de um 
 *    processo à entrada de outro. Funciona como um "tubo" onde dados entram
 *    por uma ponta (write end) e saem pela outra (read end).
 * 
 * 2. FORK: Cria uma cópia do processo atual. Após o fork(), temos dois
 *    processos: pai e filho, ambos com acesso às duas pontas do pipe.
 * 
 * 3. FLUXO:
 *    - Criar pipe (antes do fork!)
 *    - Fazer fork()
 *    - Processo pai: fecha read end, usa write end (PRODUTOR)
 *    - Processo filho: fecha write end, usa read end (CONSUMIDOR)
 * 
 * COMPILAÇÃO:
 *    gcc -o produtor_consumidor produtor_consumidor_pipes.c -lm
 * 
 * EXECUÇÃO:
 *    ./produtor_consumidor <quantidade_de_numeros>
 *    Exemplo: ./produtor_consumidor 1000
 * 
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>

/* Tamanho fixo da mensagem para comunicação via pipe */
#define MSG_SIZE 20

/**
 * Verifica se um número é primo
 * 
 * Algoritmo: Testa divisibilidade por todos os números de 2 até sqrt(n)
 * Complexidade: O(sqrt(n))
 * 
 * @param n Número a ser verificado
 * @return 1 se primo, 0 caso contrário
 */
int eh_primo(long long n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    
    /* 
     * Otimização: Todo primo > 3 pode ser escrito como 6k ± 1
     * Então só precisamos testar divisores da forma 6k ± 1
     */
    for (long long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return 0;
        }
    }
    return 1;
}

/**
 * Converte um número inteiro para string de tamanho fixo
 * 
 * IMPORTANTE: Usamos strings de tamanho fixo para garantir que cada
 * mensagem tenha exatamente MSG_SIZE bytes. Isso facilita a leitura
 * do pipe, pois sabemos exatamente quantos bytes ler por mensagem.
 * 
 * @param num Número a converter
 * @param buffer Buffer de saída (deve ter pelo menos MSG_SIZE bytes)
 */
void numero_para_string(long long num, char *buffer) {
    /* Preenche com zeros à esquerda para garantir tamanho fixo */
    snprintf(buffer, MSG_SIZE, "%019lld", num);
}

/**
 * Converte string de tamanho fixo para número inteiro
 * 
 * @param buffer String a converter
 * @return Número convertido
 */
long long string_para_numero(const char *buffer) {
    return atoll(buffer);
}

/**
 * PROCESSO PRODUTOR
 * 
 * Responsável por:
 * 1. Gerar números aleatórios crescentes
 * 2. Enviar os números pelo pipe (write end)
 * 3. Enviar 0 para sinalizar término
 * 
 * Fórmula: Ni = Ni-1 + Δ, onde N0 = 1 e Δ ∈ [1, 100]
 * 
 * @param write_fd File descriptor para escrita no pipe
 * @param quantidade Quantidade de números a gerar
 */
void produtor(int write_fd, int quantidade) {
    char buffer[MSG_SIZE];
    long long numero_atual = 1;  /* N0 = 1 */
    int delta;
    
    printf("[PRODUTOR] Iniciando produção de %d números...\n", quantidade);
    
    /* Inicializa gerador de números aleatórios */
    srand(time(NULL) ^ getpid());
    
    for (int i = 0; i < quantidade; i++) {
        /* Converte número para string de tamanho fixo */
        numero_para_string(numero_atual, buffer);
        
        /* Escreve no pipe */
        ssize_t bytes_escritos = write(write_fd, buffer, MSG_SIZE);
        if (bytes_escritos != MSG_SIZE) {
            perror("[PRODUTOR] Erro ao escrever no pipe");
            exit(EXIT_FAILURE);
        }
        
        /* Log a cada 100 números para acompanhamento */
        if ((i + 1) % 100 == 0 || i < 10) {
            printf("[PRODUTOR] Enviou número %d: %lld\n", i + 1, numero_atual);
        }
        
        /* Calcula próximo número: Ni = Ni-1 + Δ, onde Δ ∈ [1, 100] */
        delta = (rand() % 100) + 1;
        numero_atual += delta;
    }
    
    /* Envia 0 para sinalizar término */
    numero_para_string(0, buffer);
    if (write(write_fd, buffer, MSG_SIZE) != MSG_SIZE) {
        perror("[PRODUTOR] Erro ao enviar sinal de término");
    }
    printf("[PRODUTOR] Enviou sinal de término (0)\n");
    
    /* Fecha o write end do pipe */
    close(write_fd);
    printf("[PRODUTOR] Finalizando.\n");
}

/**
 * PROCESSO CONSUMIDOR
 * 
 * Responsável por:
 * 1. Receber números pelo pipe (read end)
 * 2. Verificar se cada número é primo
 * 3. Imprimir resultado
 * 4. Terminar quando receber 0
 * 
 * @param read_fd File descriptor para leitura do pipe
 */
void consumidor(int read_fd) {
    char buffer[MSG_SIZE];
    long long numero;
    int contador = 0;
    int primos_encontrados = 0;
    
    printf("[CONSUMIDOR] Aguardando números...\n");
    
    while (1) {
        /* Lê exatamente MSG_SIZE bytes do pipe */
        ssize_t bytes_lidos = read(read_fd, buffer, MSG_SIZE);
        
        if (bytes_lidos == 0) {
            /* EOF - pipe fechado pelo produtor */
            printf("[CONSUMIDOR] Pipe fechado pelo produtor.\n");
            break;
        }
        
        if (bytes_lidos != MSG_SIZE) {
            perror("[CONSUMIDOR] Erro ao ler do pipe");
            exit(EXIT_FAILURE);
        }
        
        /* Converte string para número */
        numero = string_para_numero(buffer);
        
        /* Verifica sinal de término */
        if (numero == 0) {
            printf("[CONSUMIDOR] Recebeu sinal de término (0).\n");
            break;
        }
        
        contador++;
        
        /* Verifica se é primo e imprime resultado */
        if (eh_primo(numero)) {
            primos_encontrados++;
            /* Imprime apenas os primeiros 20 primos para não poluir o terminal */
            if (primos_encontrados <= 20) {
                printf("[CONSUMIDOR] Número %d: %lld -> PRIMO\n", contador, numero);
            }
        } else {
            /* Imprime apenas os primeiros 10 não-primos */
            if (contador <= 10 && primos_encontrados < 20) {
                printf("[CONSUMIDOR] Número %d: %lld -> NÃO PRIMO\n", contador, numero);
            }
        }
    }
    
    /* Fecha o read end do pipe */
    close(read_fd);
    
    printf("\n========================================\n");
    printf("[CONSUMIDOR] RESUMO:\n");
    printf("  Total de números processados: %d\n", contador);
    printf("  Números primos encontrados: %d\n", primos_encontrados);
    printf("  Porcentagem de primos: %.2f%%\n", 
           contador > 0 ? (100.0 * primos_encontrados / contador) : 0.0);
    printf("========================================\n");
}

/**
 * FUNÇÃO PRINCIPAL
 * 
 * Fluxo de execução:
 * 1. Valida argumentos de linha de comando
 * 2. Cria o pipe (ANTES do fork!)
 * 3. Faz fork() para criar processo filho
 * 4. Pai -> Produtor (usa write end, fecha read end)
 * 5. Filho -> Consumidor (usa read end, fecha write end)
 * 6. Pai aguarda filho terminar
 */
int main(int argc, char *argv[]) {
    int pipe_fd[2];  /* pipe_fd[0] = read end, pipe_fd[1] = write end */
    pid_t pid;
    int quantidade;
    
    /* Valida argumentos */
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <quantidade_de_numeros>\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 1000\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    quantidade = atoi(argv[1]);
    if (quantidade <= 0) {
        fprintf(stderr, "Erro: quantidade deve ser um número positivo.\n");
        exit(EXIT_FAILURE);
    }
    
    printf("========================================\n");
    printf("PRODUTOR-CONSUMIDOR COM PIPES\n");
    printf("Quantidade de números: %d\n", quantidade);
    printf("========================================\n\n");
    
    /* 
     * PASSO 1: Criar o pipe
     * 
     * pipe() cria um canal de comunicação unidirecional.
     * pipe_fd[0] = file descriptor para leitura (read end)
     * pipe_fd[1] = file descriptor para escrita (write end)
     * 
     * IMPORTANTE: O pipe deve ser criado ANTES do fork() para que
     * ambos os processos (pai e filho) herdem os file descriptors.
     */
    if (pipe(pipe_fd) == -1) {
        perror("Erro ao criar pipe");
        exit(EXIT_FAILURE);
    }
    
    printf("Pipe criado com sucesso.\n");
    printf("  Read end (fd): %d\n", pipe_fd[0]);
    printf("  Write end (fd): %d\n\n", pipe_fd[1]);
    
    /* 
     * PASSO 2: Fazer fork()
     * 
     * fork() cria uma cópia do processo atual.
     * - No processo pai, fork() retorna o PID do filho
     * - No processo filho, fork() retorna 0
     * - Em caso de erro, fork() retorna -1
     * 
     * Após o fork, ambos os processos têm acesso aos dois file
     * descriptors do pipe. Cada processo deve fechar a ponta
     * que não vai usar.
     */
    pid = fork();
    
    if (pid == -1) {
        perror("Erro ao fazer fork");
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        /* 
         * PROCESSO FILHO (CONSUMIDOR)
         * 
         * O filho vai LER do pipe, então:
         * - Fecha o write end (não precisa escrever)
         * - Usa o read end para receber dados
         */
        printf("[FILHO] PID: %d, PPID: %d\n", getpid(), getppid());
        
        close(pipe_fd[1]);  /* Fecha write end */
        
        consumidor(pipe_fd[0]);
        
        printf("[FILHO] Processo consumidor finalizado.\n");
        exit(EXIT_SUCCESS);
        
    } else {
        /* 
         * PROCESSO PAI (PRODUTOR)
         * 
         * O pai vai ESCREVER no pipe, então:
         * - Fecha o read end (não precisa ler)
         * - Usa o write end para enviar dados
         */
        printf("[PAI] PID: %d, Filho PID: %d\n\n", getpid(), pid);
        
        close(pipe_fd[0]);  /* Fecha read end */
        
        produtor(pipe_fd[1], quantidade);
        
        /* 
         * PASSO 3: Aguardar o filho terminar
         * 
         * wait() bloqueia o processo pai até que o filho termine.
         * Isso evita que o pai termine antes do filho (orphan process)
         * e garante que possamos ver a saída do consumidor.
         */
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            printf("\n[PAI] Filho terminou com código: %d\n", WEXITSTATUS(status));
        }
        
        printf("[PAI] Processo produtor finalizado.\n");
    }
    
    return EXIT_SUCCESS;
}
