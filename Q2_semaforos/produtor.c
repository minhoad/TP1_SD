/*
 * ============================================================================
 * QUESTAO 2 - PRODUTOR (PROCESSO COM MULTIPLAS THREADS)
 * ============================================================================
 * 
 * Arquivo: produtor.c
 * Descricao: Processo produtor que cria Np threads para gerar numeros
 *            aleatorios e inserir no buffer compartilhado
 * 
 * Uso: ./produtor <Np>
 *      Np - Numero de threads produtoras (lido da memoria compartilhada)
 * 
 * Comunicacao:
 *   - Memoria compartilhada: acesso ao buffer circular
 *   - Semaforos nomeados: sincronizacao com consumidor
 * 
 * Funcionamento de cada thread:
 *   1. Aguarda posicao livre (sem_wait em sem_empty)
 *   2. Obtem mutex para acessar buffer
 *   3. Insere numero na posicao head
 *   4. Registra ocupacao no historico
 *   5. Libera mutex
 *   6. Sinaliza posicao ocupada (sem_post em sem_full)
 * ============================================================================
 */

#include "common.h"

/* ============================================================================
 * VARIAVEIS GLOBAIS
 * ============================================================================ */

static SharedMemory *shm = NULL;
static sem_t *sem_empty = SEM_FAILED;
static sem_t *sem_full = SEM_FAILED;
static sem_t *sem_mutex = SEM_FAILED;
static sem_t *sem_print = SEM_FAILED;

/* ============================================================================
 * HANDLER DE SINAIS
 * ============================================================================ */

void handler_sigterm(int sig) {
    (void)sig;
    if (shm) {
        atomic_store(&shm->sistema_ativo, 0);
    }
}

/* ============================================================================
 * FUNCAO DA THREAD PRODUTORA
 * ============================================================================ */

/*
 * Argumentos para a thread
 */
typedef struct {
    int thread_id;
    unsigned int seed;
} ThreadArgs;

/*
 * Funcao executada por cada thread produtora
 */
void* thread_produtor(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int thread_id = args->thread_id;
    unsigned int seed = args->seed;
    
    int numeros_produzidos_local = 0;
    
    while (atomic_load(&shm->sistema_ativo)) {
        /* Verifica se ja atingiu a meta */
        int atual = atomic_load(&shm->produzidos);
        if (atual >= shm->M) {
            break;
        }
        
        /* Gera numero aleatorio */
        int numero = gerar_numero_aleatorio(&seed);
        
        /* Aguarda posicao livre (sem_empty > 0) */
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += 100000000; /* 100ms */
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }
        
        if (sem_timedwait(sem_empty, &timeout) == -1) {
            if (errno == ETIMEDOUT) {
                continue; /* Verifica novamente se deve continuar */
            }
            break;
        }
        
        /* Verifica novamente apos acordar */
        if (!atomic_load(&shm->sistema_ativo)) {
            sem_post(sem_empty); /* Devolve o recurso */
            break;
        }
        
        /* Obtem mutex para acessar o buffer */
        sem_wait(sem_mutex);
        
        /* Verifica se ja atingiu a meta (verificacao dupla) */
        if (atomic_load(&shm->produzidos) >= shm->M) {
            sem_post(sem_mutex);
            sem_post(sem_empty);
            break;
        }
        
        /* Insere numero no buffer */
        shm->buffer[shm->head] = numero;
        shm->head = (shm->head + 1) % shm->N;
        shm->count++;
        
        /* Incrementa contador de produzidos */
        atomic_fetch_add(&shm->produzidos, 1);
        
        /* Registra ocupacao no historico */
        registrar_ocupacao(shm);
        
        numeros_produzidos_local++;
        
        /* Libera mutex */
        sem_post(sem_mutex);
        
        /* Sinaliza que ha uma posicao ocupada */
        sem_post(sem_full);
    }
    
    sem_wait(sem_print);
    printf("[PRODUTOR T%d] Thread encerrada. Produziu %d numeros.\n", 
           thread_id, numeros_produzidos_local);
    fflush(stdout);
    sem_post(sem_print);
    
    free(args);
    return NULL;
}

/* ============================================================================
 * FUNCAO PRINCIPAL
 * ============================================================================ */

int main(int argc, char *argv[]) {
    /* Abre memoria compartilhada */
    shm = abrir_shm();
    if (!shm) {
        fprintf(stderr, "[PRODUTOR] Erro ao abrir memoria compartilhada\n");
        return 1;
    }
    
    /* Abre semaforos */
    if (abrir_semaforos(&sem_empty, &sem_full, &sem_mutex, &sem_print) != 0) {
        fprintf(stderr, "[PRODUTOR] Erro ao abrir semaforos\n");
        fechar_shm(shm);
        return 1;
    }
    
    /* Obtem numero de threads do argumento ou da memoria compartilhada */
    int Np = (argc > 1) ? atoi(argv[1]) : shm->Np;
    
    /* Configura handler para SIGTERM */
    struct sigaction sa;
    sa.sa_handler = handler_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    
    sem_wait(sem_print);
    printf("[PRODUTOR] Iniciado com %d threads\n", Np);
    fflush(stdout);
    sem_post(sem_print);
    
    /* Cria threads produtoras */
    pthread_t *threads = malloc(Np * sizeof(pthread_t));
    if (!threads) {
        perror("[PRODUTOR] Erro ao alocar threads");
        fechar_semaforos(sem_empty, sem_full, sem_mutex, sem_print);
        fechar_shm(shm);
        return 1;
    }
    
    /* Inicializa gerador de numeros aleatorios */
    unsigned int base_seed = time(NULL) ^ getpid();
    
    /* Lanca threads */
    for (int i = 0; i < Np; i++) {
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        if (!args) {
            perror("[PRODUTOR] Erro ao alocar argumentos");
            continue;
        }
        args->thread_id = i;
        args->seed = base_seed + i * 1000;
        
        if (pthread_create(&threads[i], NULL, thread_produtor, args) != 0) {
            perror("[PRODUTOR] Erro ao criar thread");
            free(args);
        }
    }
    
    /* Sinaliza que produtor esta pronto */
    atomic_fetch_add(&shm->produtores_prontos, 1);
    
    /* Aguarda todas as threads terminarem */
    for (int i = 0; i < Np; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    
    sem_wait(sem_print);
    printf("[PRODUTOR] Processo encerrado. Total produzido: %d\n", 
           atomic_load(&shm->produzidos));
    fflush(stdout);
    sem_post(sem_print);
    
    /* Fecha recursos */
    fechar_semaforos(sem_empty, sem_full, sem_mutex, sem_print);
    fechar_shm(shm);
    
    return 0;
}
