/* ============================================================================
 * QUESTAO 2 - CONSUMIDOR (PROCESSO COM MULTIPLAS THREADS)
 * ============================================================================
 * 
 * Arquivo: consumidor.c
 * Descricao: Processo consumidor que cria Nc threads para remover numeros
 *            do buffer compartilhado e verificar se sao primos
 * 
 * Uso: ./consumidor <Nc>
 *      Nc - Numero de threads consumidoras (lido da memoria compartilhada)
 * 
 * Comunicacao:
 *   - Memoria compartilhada: acesso ao buffer circular
 *   - Semaforos nomeados: sincronizacao com produtor
 * 
 * Funcionamento de cada thread:
 *   1. Aguarda posicao ocupada (sem_wait em sem_full)
 *   2. Obtem mutex para acessar buffer
 *   3. Remove numero da posicao tail
 *   4. Registra ocupacao no historico
 *   5. Libera mutex
 *   6. Sinaliza posicao livre (sem_post em sem_empty)
 *   7. Verifica se o numero eh primo (FORA da regiao critica)
 * 
 * ============================================================================
 */

#include "common.h"

static SharedMemory *shm = NULL;
static sem_t *sem_empty = SEM_FAILED;
static sem_t *sem_full = SEM_FAILED;
static sem_t *sem_mutex = SEM_FAILED;
static sem_t *sem_print = SEM_FAILED;

/* Handler para SIGTERM */
void handler_sigterm(int sig) {
    (void)sig;
    if (shm) atomic_store(&shm->sistema_ativo, 0);
}

/* Função da thread consumidora */
typedef struct {
    int thread_id;
} ThreadArgs;

void* thread_consumidor(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int thread_id = args->thread_id;
    int numeros_consumidos_local = 0;
    int primos_local = 0;
    
    while (atomic_load(&shm->sistema_ativo)) {
        /* Verifica se já atingiu a meta */
        int atual = atomic_load(&shm->consumidos);
        if (atual >= shm->M) break;
        
        /* Aguarda posição ocupada (sem_full > 0) */
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_nsec += 1000000000;
        if (timeout.tv_nsec >= 1000000000) {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }
        
        if (sem_timedwait(sem_full, &timeout) == -1) {
            if (errno == ETIMEDOUT) continue;
            break;
        }
        
        if (!atomic_load(&shm->sistema_ativo)) {
            sem_post(sem_full);
            break;
        }
        
        /* Obtém mutex para acessar buffer */
        sem_wait(sem_mutex);
        
        /* Verifica dupla */
        if (atomic_load(&shm->consumidos) >= shm->M) {
            sem_post(sem_mutex);
            sem_post(sem_full);
            break;
        }
        
        /* Remove do buffer */
        int numero = shm->buffer[shm->tail];
        shm->tail = (shm->tail + 1) % shm->N;
        shm->count--;
        
        atomic_fetch_add(&shm->consumidos, 1);
        registrar_ocupacao(shm);
        
        sem_post(sem_mutex);
        
        /* Sinaliza posição livre */
        sem_post(sem_empty);
        
        /* Verifica primo (fora da região crítica) */
        if (eh_primo(numero)) {
            primos_local++;
        }
        
        numeros_consumidos_local++;
    }
    
    sem_wait(sem_print);
    printf("[CONSUMIDOR T%d] Thread encerrada. Consumiu %d, primos=%d\n",
           thread_id, numeros_consumidos_local, primos_local);
    sem_post(sem_print);
    
    free(args);
    return NULL;
}

int main(int argc, char *argv[]) {
    /* Abre memória compartilhada */
    shm = abrir_shm();
    if (!shm) return 1;
    
    /* Abre semáforos */
    if (abrir_semaforos(&sem_empty, &sem_full, &sem_mutex, &sem_print) != 0) {
        fechar_shm(shm);
        return 1;
    }
    
    int Nc = (argc > 1) ? atoi(argv[1]) : shm->Nc;
    
    /* Configura handler SIGTERM */
    struct sigaction sa;
    sa.sa_handler = handler_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    
    sem_wait(sem_print);
    printf("[CONSUMIDOR] Iniciado com %d threads\n", Nc);
    sem_post(sem_print);
    
    /* Cria threads consumidoras */
    pthread_t *threads = malloc(Nc * sizeof(pthread_t));
    if (!threads) {
        perror("[CONSUMIDOR] Erro ao alocar threads");
        fechar_semaforos(sem_empty, sem_full, sem_mutex, sem_print);
        fechar_shm(shm);
        return 1;
    }
    
    for (int i = 0; i < Nc; i++) {
        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->thread_id = i;
        pthread_create(&threads[i], NULL, thread_consumidor, args);
    }
    
    /* Sinaliza que consumidor está pronto */
    atomic_fetch_add(&shm->consumidores_prontos, 1);
    
    /* Aguarda threads */
    for (int i = 0; i < Nc; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    
    sem_wait(sem_print);
    printf("[CONSUMIDOR] Processo encerrado. Total consumido: %d\n",
           atomic_load(&shm->consumidos));
    sem_post(sem_print);
    
    /* Fecha recursos */
    fechar_semaforos(sem_empty, sem_full, sem_mutex, sem_print);
    fechar_shm(shm);
    
    return 0;
}