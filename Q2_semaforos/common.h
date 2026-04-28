/*
 * ============================================================================
 * QUESTAO 2 - PRODUTOR-CONSUMIDOR COM PROCESSOS SEPARADOS E THREADS
 * ============================================================================
 * 
 * Arquivo: common.h
 * Descricao: Definicoes compartilhadas entre produtor, consumidor e controlador
 * 
 * Arquitetura:
 *   - Controlador: cria recursos IPC e lanca processos
 *   - Produtor: processo com Np threads produtoras
 *   - Consumidor: processo com Nc threads consumidoras
 * 
 * IPC utilizado:
 *   - POSIX Shared Memory (shm_open/mmap)
 *   - POSIX Named Semaphores (sem_open)
 * 
 * Sincronizacao:
 *   - sem_empty: conta posicoes livres no buffer
 *   - sem_full: conta posicoes ocupadas no buffer
 *   - sem_mutex: exclusao mutua para acesso ao buffer
 *   - sem_print: exclusao mutua para impressao no terminal
 * ============================================================================
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdatomic.h>

/* ============================================================================
 * CONSTANTES
 * ============================================================================ */

#define SHM_NAME        "/prod_cons_shm"
#define SEM_EMPTY_NAME  "/prod_cons_empty"
#define SEM_FULL_NAME   "/prod_cons_full"
#define SEM_MUTEX_NAME  "/prod_cons_mutex"
#define SEM_PRINT_NAME  "/prod_cons_print"

/* Tamanho maximo do buffer */
#define MAX_BUFFER_SIZE 10000

/* Tamanho maximo do historico de ocupacao */
#define MAX_HISTORICO   1000000

/* Valor maximo para numeros aleatorios (10^7) */
#define MAX_RANDOM      10000000

/* Meta padrao de numeros a processar (10^5) */
#define META_PADRAO     100000

/* ============================================================================
 * ESTRUTURAS
 * ============================================================================ */

/*
 * Estrutura da memoria compartilhada
 */
typedef struct {
    /* Parametros do sistema */
    int N;                          /* Tamanho do buffer */
    int M;                          /* Meta de numeros a processar */
    int Np;                         /* Numero de threads produtoras */
    int Nc;                         /* Numero de threads consumidoras */
    
    /* Buffer circular */
    int buffer[MAX_BUFFER_SIZE];    /* Buffer de numeros */
    int head;                       /* Indice de escrita (produtor) */
    int tail;                       /* Indice de leitura (consumidor) */
    int count;                      /* Ocupacao atual do buffer */
    
    /* Contadores atomicos */
    atomic_int produzidos;          /* Total de numeros produzidos */
    atomic_int consumidos;          /* Total de numeros consumidos */
    
    /* Controle do sistema */
    atomic_int sistema_ativo;       /* Flag para encerramento */
    atomic_int produtores_prontos;  /* Produtores que iniciaram */
    atomic_int consumidores_prontos;/* Consumidores que iniciaram */
    
    /* Historico de ocupacao do buffer para graficos */
    int historico_ocupacao[MAX_HISTORICO];
    atomic_int historico_idx;
    
    /* Timestamps */
    struct timespec tempo_inicio;
    struct timespec tempo_fim;
    
} SharedMemory;

/* ============================================================================
 * FUNCOES UTILITARIAS
 * ============================================================================ */

/*
 * Verifica se um numero eh primo
 * Usa otimizacao: verifica divisibilidade ate raiz quadrada
 */
static inline int eh_primo(int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) {
            return 0;
        }
    }
    return 1;
}

/*
 * Gera numero aleatorio entre 1 e MAX_RANDOM
 * Thread-safe usando rand_r
 */
static inline int gerar_numero_aleatorio(unsigned int *seed) {
    return (rand_r(seed) % MAX_RANDOM) + 1;
}

/*
 * Abre a memoria compartilhada existente
 * Retorna ponteiro para SharedMemory ou NULL em caso de erro
 */
static inline SharedMemory* abrir_shm(void) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return NULL;
    }
    
    SharedMemory *shm = mmap(NULL, sizeof(SharedMemory),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
    close(shm_fd);
    
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    
    return shm;
}

/*
 * Fecha o mapeamento da memoria compartilhada
 */
static inline void fechar_shm(SharedMemory *shm) {
    if (shm && shm != MAP_FAILED) {
        munmap(shm, sizeof(SharedMemory));
    }
}

/*
 * Abre os semaforos nomeados
 * Retorna 0 em sucesso, -1 em erro
 */
static inline int abrir_semaforos(sem_t **sem_empty, sem_t **sem_full,
                                   sem_t **sem_mutex, sem_t **sem_print) {
    *sem_empty = sem_open(SEM_EMPTY_NAME, 0);
    *sem_full = sem_open(SEM_FULL_NAME, 0);
    *sem_mutex = sem_open(SEM_MUTEX_NAME, 0);
    *sem_print = sem_open(SEM_PRINT_NAME, 0);
    
    if (*sem_empty == SEM_FAILED || *sem_full == SEM_FAILED ||
        *sem_mutex == SEM_FAILED || *sem_print == SEM_FAILED) {
        perror("sem_open");
        return -1;
    }
    
    return 0;
}

/*
 * Fecha os semaforos
 */
static inline void fechar_semaforos(sem_t *sem_empty, sem_t *sem_full,
                                     sem_t *sem_mutex, sem_t *sem_print) {
    if (sem_empty != SEM_FAILED) sem_close(sem_empty);
    if (sem_full != SEM_FAILED) sem_close(sem_full);
    if (sem_mutex != SEM_FAILED) sem_close(sem_mutex);
    if (sem_print != SEM_FAILED) sem_close(sem_print);
}

/*
 * Registra a ocupacao atual do buffer no historico
 */
static inline void registrar_ocupacao(SharedMemory *shm) {
    int idx = atomic_fetch_add(&shm->historico_idx, 1);
    if (idx < MAX_HISTORICO) {
        shm->historico_ocupacao[idx] = shm->count;
    }
}

/*
 * Imprime mensagem de forma thread-safe
 */
static inline void print_safe(sem_t *sem_print, const char *fmt, ...) {
    va_list args;
    sem_wait(sem_print);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
    sem_post(sem_print);
}

#endif /* COMMON_H */
