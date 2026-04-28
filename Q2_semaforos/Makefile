# =============================================================================
# TRABALHO PRÁTICO - SISTEMAS DISTRIBUÍDOS
# Questão 2: Produtor-Consumidor com Memória Compartilhada (IPC)
# Makefile
# =============================================================================

# Compilador e flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Bibliotecas necessárias
# -lrt: POSIX real-time (shm_open, sem_open)
# -lpthread: POSIX threads (necessário para semáforos)
# -lm: math library
LIBS = -lrt -lpthread -lm

# Alvos
CONTROLADOR = controlador
PRODUTOR = produtor
CONSUMIDOR = consumidor

# Regras
.PHONY: all clean test help limpar_ipc

all: $(CONTROLADOR) $(PRODUTOR) $(CONSUMIDOR)
	@echo ""
	@echo "=== Compilação concluída com sucesso! ==="
	@echo ""
	@echo "Executáveis criados:"
	@echo "  - controlador : Programa principal (inicia produtores e consumidores)"
	@echo "  - produtor    : Processo produtor (executado pelo controlador)"
	@echo "  - consumidor  : Processo consumidor (executado pelo controlador)"
	@echo ""
	@echo "Para executar:"
	@echo "  ./controlador <N> <Np> <Nc> [M]"
	@echo ""

$(CONTROLADOR): controlador.c common.h
	$(CC) $(CFLAGS) -o $@ controlador.c $(LIBS)

$(PRODUTOR): produtor.c common.h
	$(CC) $(CFLAGS) -o $@ produtor.c $(LIBS)

$(CONSUMIDOR): consumidor.c common.h
	$(CC) $(CFLAGS) -o $@ consumidor.c $(LIBS)

# Executa um teste rápido
test: all
	@echo "=== Executando teste rápido ==="
	@echo "Parâmetros: N=10, Np=2, Nc=2, M=1000"
	./$(CONTROLADOR) 10 2 2 1000

# Limpa recursos IPC órfãos (caso o programa tenha terminado abruptamente)
limpar_ipc:
	@echo "Removendo recursos IPC..."
	-rm -f /dev/shm/prodcons_shm
	-rm -f /dev/shm/sem.prodcons_*
	@echo "Recursos removidos."

# Limpa arquivos compilados e resultados
clean:
	rm -f $(CONTROLADOR) $(PRODUTOR) $(CONSUMIDOR)
	rm -f ocupacao_*.csv
	rm -f resultados_*.csv resultados_*.txt
	rm -f *.png *.pdf
	@echo "Arquivos limpos."

# Ajuda
help:
	@echo "=== Produtor-Consumidor com Shared Memory ==="
	@echo ""
	@echo "Comandos disponíveis:"
	@echo "  make          - Compila todos os executáveis"
	@echo "  make test     - Executa um teste rápido"
	@echo "  make clean    - Remove executáveis e arquivos gerados"
	@echo "  make limpar_ipc - Remove recursos IPC órfãos"
	@echo "  make help     - Mostra esta ajuda"
	@echo ""
	@echo "Execução:"
	@echo "  ./controlador <N> <Np> <Nc> [M]"
	@echo ""
	@echo "  N  = Tamanho do buffer"
	@echo "  Np = Número de processos produtores"
	@echo "  Nc = Número de processos consumidores"
	@echo "  M  = Total de números a processar (default: 100000)"
	@echo ""
	@echo "Exemplos:"
	@echo "  ./controlador 100 2 4 100000"
	@echo "  ./controlador 10 1 1 1000"
