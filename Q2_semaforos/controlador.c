/*
 * ============================================================================
 * QUESTAO 2 - CONTROLADOR
 * ============================================================================
 * 
 * Arquivo: controlador.c
 * Descricao: Cria recursos IPC, lanca processos produtor e consumidor,
 *            aguarda termino e coleta resultados
 * 
 * Uso: ./controlador <N> <Np> <Nc> [M]
 *      N  - Tamanho do buffer compartilhado
 *      Np - Numero de threads produtoras
 *      Nc - Numero de threads consumidoras
 *      M  - Meta de numeros a processar (opcional, padrao: 100000)
 * 
 * Exemplo: ./controlador 100 4 4 100000
 * ============================================================================
 */

#include "common.h"
#include <stdarg.h>

/* ============================================================================
 * VARIAVEIS GLOBAIS
 * ============================================================================ */

static pid_t pid_produtor = -1;
static pid_t pid_consumidor = -1;
static int shm_fd = -1;
static SharedMemory *shm = NULL;
static sem_t *sem_empty = SEM_FAILED;
static sem_t *sem_full = SEM_FAILED;
static sem_t *sem_mutex = SEM_FAILED;
static sem_t *sem_print = SEM_FAILED;

/* ============================================================================
 * FUNCOES DE LIMPEZA
 * ============================================================================ */

/*
 * Limpa todos os recursos IPC
 */
void limpar_recursos(void) {
    /* Fecha semaforos */
    if (sem_empty != SEM_FAILED) sem_close(sem_empty);
    if (sem_full != SEM_FAILED) sem_close(sem_full);
    if (sem_mutex != SEM_FAILED) sem_close(sem_mutex);
    if (sem_print != SEM_FAILED) sem_close(sem_print);
    
    /* Remove semaforos nomeados */
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_PRINT_NAME);
    
    /* Fecha e remove memoria compartilhada */
    if (shm && shm != MAP_FAILED) {
        munmap(shm, sizeof(SharedMemory));
    }
    if (shm_fd != -1) {
        close(shm_fd);
    }
    shm_unlink(SHM_NAME);
}

/*
 * Handler para SIGINT (Ctrl+C)
 */
void handler_sigint(int sig) {
    (void)sig;
    printf("\n\n[CONTROLADOR] Recebido SIGINT. Encerrando processos...\n");
    
    /* Sinaliza encerramento */
    if (shm) {
        atomic_store(&shm->sistema_ativo, 0);
    }
    
    /* Envia SIGTERM para os processos filhos */
    if (pid_produtor > 0) {
        kill(pid_produtor, SIGTERM);
    }
    if (pid_consumidor > 0) {
        kill(pid_consumidor, SIGTERM);
    }
    
    /* Aguarda processos terminarem */
    if (pid_produtor > 0) waitpid(pid_produtor, NULL, 0);
    if (pid_consumidor > 0) waitpid(pid_consumidor, NULL, 0);
    
    limpar_recursos();
    exit(1);
}

/* ============================================================================
 * FUNCOES AUXILIARES
 * ============================================================================ */

/*
 * Exibe ajuda de uso
 */
void exibir_uso(const char *programa) {
    printf("Uso: %s <N> <Np> <Nc> [M]\n", programa);
    printf("  N  - Tamanho do buffer compartilhado (1-%d)\n", MAX_BUFFER_SIZE);
    printf("  Np - Numero de threads produtoras (>= 1)\n");
    printf("  Nc - Numero de threads consumidoras (>= 1)\n");
    printf("  M  - Meta de numeros a processar (opcional, padrao: %d)\n", META_PADRAO);
    printf("\nExemplo: %s 100 4 4 100000\n", programa);
}

/*
 * Cria a memoria compartilhada
 */
int criar_shm(int N, int Np, int Nc, int M) {
    /* Remove memoria compartilhada anterior se existir */
    shm_unlink(SHM_NAME);
    
    /* Cria nova memoria compartilhada */
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("[CONTROLADOR] Erro ao criar memoria compartilhada");
        return -1;
    }
    
    /* Define o tamanho */
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("[CONTROLADOR] Erro ao definir tamanho da memoria");
        return -1;
    }
    
    /* Mapeia a memoria */
    shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("[CONTROLADOR] Erro ao mapear memoria");
        return -1;
    }
    
    /* Inicializa a estrutura */
    memset(shm, 0, sizeof(SharedMemory));
    shm->N = N;
    shm->M = M;
    shm->Np = Np;
    shm->Nc = Nc;
    shm->head = 0;
    shm->tail = 0;
    shm->count = 0;
    atomic_store(&shm->produzidos, 0);
    atomic_store(&shm->consumidos, 0);
    atomic_store(&shm->sistema_ativo, 1);
    atomic_store(&shm->produtores_prontos, 0);
    atomic_store(&shm->consumidores_prontos, 0);
    atomic_store(&shm->historico_idx, 0);
    
    printf("[CONTROLADOR] Memoria compartilhada criada (%zu bytes)\n",
           sizeof(SharedMemory));
    
    return 0;
}

/*
 * Cria os semaforos nomeados
 */
int criar_semaforos(int N) {
    /* Remove semaforos anteriores */
    sem_unlink(SEM_EMPTY_NAME);
    sem_unlink(SEM_FULL_NAME);
    sem_unlink(SEM_MUTEX_NAME);
    sem_unlink(SEM_PRINT_NAME);
    
    /* Cria semaforo de posicoes vazias (inicializado com N) */
    sem_empty = sem_open(SEM_EMPTY_NAME, O_CREAT | O_EXCL, 0666, N);
    if (sem_empty == SEM_FAILED) {
        perror("[CONTROLADOR] Erro ao criar sem_empty");
        return -1;
    }
    
    /* Cria semaforo de posicoes cheias (inicializado com 0) */
    sem_full = sem_open(SEM_FULL_NAME, O_CREAT | O_EXCL, 0666, 0);
    if (sem_full == SEM_FAILED) {
        perror("[CONTROLADOR] Erro ao criar sem_full");
        return -1;
    }
    
    /* Cria mutex para acesso ao buffer */
    sem_mutex = sem_open(SEM_MUTEX_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_mutex == SEM_FAILED) {
        perror("[CONTROLADOR] Erro ao criar sem_mutex");
        return -1;
    }
    
    /* Cria mutex para impressao */
    sem_print = sem_open(SEM_PRINT_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (sem_print == SEM_FAILED) {
        perror("[CONTROLADOR] Erro ao criar sem_print");
        return -1;
    }
    
    printf("[CONTROLADOR] Semaforos criados (empty=%d, full=0, mutex=1)\n", N);
    
    return 0;
}

/*
 * Salva o historico de ocupacao em arquivo
 */
void salvar_historico(int N, int Np, int Nc) {
    char filename[256];
    snprintf(filename, sizeof(filename), 
             "ocupacao_N%d_Np%d_Nc%d.csv", N, Np, Nc);
    
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("[CONTROLADOR] Erro ao criar arquivo de historico");
        return;
    }
    
    fprintf(fp, "operacao,ocupacao\n");
    
    int total = atomic_load(&shm->historico_idx);
    if (total > MAX_HISTORICO) total = MAX_HISTORICO;
    
    for (int i = 0; i < total; i++) {
        fprintf(fp, "%d,%d\n", i, shm->historico_ocupacao[i]);
    }
    
    fclose(fp);
    printf("[CONTROLADOR] Historico salvo em: %s (%d registros)\n", 
           filename, total);
}

/* ============================================================================
 * FUNCAO PRINCIPAL
 * ============================================================================ */

int main(int argc, char *argv[]) {
    /* Verifica argumentos */
    if (argc < 4) {
        exibir_uso(argv[0]);
        return 1;
    }
    
    int N = atoi(argv[1]);
    int Np = atoi(argv[2]);
    int Nc = atoi(argv[3]);
    int M = (argc > 4) ? atoi(argv[4]) : META_PADRAO;
    
    /* Valida parametros */
    if (N < 1 || N > MAX_BUFFER_SIZE) {
        fprintf(stderr, "Erro: N deve estar entre 1 e %d\n", MAX_BUFFER_SIZE);
        return 1;
    }
    if (Np < 1) {
        fprintf(stderr, "Erro: Np deve ser >= 1\n");
        return 1;
    }
    if (Nc < 1) {
        fprintf(stderr, "Erro: Nc deve ser >= 1\n");
        return 1;
    }
    if (M < 1) {
        fprintf(stderr, "Erro: M deve ser >= 1\n");
        return 1;
    }
    
    printf("\n");
    printf("============================================================\n");
    printf("  PRODUTOR-CONSUMIDOR COM PROCESSOS E THREADS\n");
    printf("============================================================\n");
    printf("  Buffer (N):           %d\n", N);
    printf("  Threads produtoras:   %d\n", Np);
    printf("  Threads consumidoras: %d\n", Nc);
    printf("  Meta (M):             %d numeros\n", M);
    printf("============================================================\n\n");
    
    /* Configura handler para SIGINT */
    struct sigaction sa;
    sa.sa_handler = handler_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    /* Cria recursos IPC */
    if (criar_shm(N, Np, Nc, M) != 0) {
        limpar_recursos();
        return 1;
    }
    
    if (criar_semaforos(N) != 0) {
        limpar_recursos();
        return 1;
    }
    
    /* Registra tempo de inicio */
    clock_gettime(CLOCK_MONOTONIC, &shm->tempo_inicio);
    
    /* Lanca processo consumidor primeiro (para estar pronto antes do produtor) */
    pid_consumidor = fork();
    if (pid_consumidor == -1) {
        perror("[CONTROLADOR] Erro ao criar processo consumidor");
        limpar_recursos();
        return 1;
    }
    
    if (pid_consumidor == 0) {
        /* Processo filho - executa consumidor */
        char nc_str[16];
        snprintf(nc_str, sizeof(nc_str), "%d", Nc);
        execl("./consumidor", "consumidor", nc_str, NULL);
        perror("[CONTROLADOR] Erro ao executar consumidor");
        exit(1);
    }
    
    printf("[CONTROLADOR] Processo consumidor iniciado (PID: %d)\n", pid_consumidor);
    
    /* Pequena pausa para consumidor inicializar */
    usleep(10000);
    
    /* Lanca processo produtor */
    pid_produtor = fork();
    if (pid_produtor == -1) {
        perror("[CONTROLADOR] Erro ao criar processo produtor");
        kill(pid_consumidor, SIGTERM);
        waitpid(pid_consumidor, NULL, 0);
        limpar_recursos();
        return 1;
    }
    
    if (pid_produtor == 0) {
        /* Processo filho - executa produtor */
        char np_str[16];
        snprintf(np_str, sizeof(np_str), "%d", Np);
        execl("./produtor", "produtor", np_str, NULL);
        perror("[CONTROLADOR] Erro ao executar produtor");
        exit(1);
    }
    
    printf("[CONTROLADOR] Processo produtor iniciado (PID: %d)\n", pid_produtor);
    printf("[CONTROLADOR] Aguardando processamento de %d numeros...\n\n", M);
    
    /* Aguarda termino dos processos */
    int status_produtor, status_consumidor;
    
    waitpid(pid_consumidor, &status_consumidor, 0);
    printf("[CONTROLADOR] Consumidor terminou\n");
    
    /* Envia sinal para produtor encerrar se ainda estiver rodando */
    atomic_store(&shm->sistema_ativo, 0);
    kill(pid_produtor, SIGTERM);
    
    waitpid(pid_produtor, &status_produtor, 0);
    printf("[CONTROLADOR] Produtor terminou\n");
    
    /* Registra tempo de fim */
    clock_gettime(CLOCK_MONOTONIC, &shm->tempo_fim);
    
    /* Calcula tempo de execucao */
    double tempo_execucao = 
        (shm->tempo_fim.tv_sec - shm->tempo_inicio.tv_sec) +
        (shm->tempo_fim.tv_nsec - shm->tempo_inicio.tv_nsec) / 1e9;
    
    /* Exibe resultados */
    printf("\n");
    printf("============================================================\n");
    printf("  RESULTADOS\n");
    printf("============================================================\n");
    printf("  Numeros produzidos:   %d\n", atomic_load(&shm->produzidos));
    printf("  Numeros consumidos:   %d\n", atomic_load(&shm->consumidos));
    printf("  Tempo de execucao:    %.6f segundos\n", tempo_execucao);
    printf("  Throughput:           %.2f numeros/segundo\n", 
           atomic_load(&shm->consumidos) / tempo_execucao);
    printf("============================================================\n");
    
    /* Imprime tempo em formato CSV para scripts de teste */
    printf("CSV:%d,%d,%d,%.6f\n", N, Np, Nc, tempo_execucao);
    
    /* Salva historico de ocupacao */
    salvar_historico(N, Np, Nc);
    
    /* Limpa recursos */
    limpar_recursos();
    
    printf("\n[CONTROLADOR] Encerrado com sucesso.\n");
    
    return 0;
}
