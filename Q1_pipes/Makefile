# =============================================================================
# Makefile para Questão 1 - Produtor-Consumidor com Pipes
# =============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = -lm

TARGET = produtor_consumidor
SRC = produtor_consumidor_pipes.c

.PHONY: all clean run test

# Compilar o programa
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "Compilação concluída: $(TARGET)"

# Limpar arquivos gerados
clean:
	rm -f $(TARGET)
	@echo "Arquivos limpos."

# Executar com 100 números (teste rápido)
run: $(TARGET)
	./$(TARGET) 100

# Testes com diferentes quantidades
test: $(TARGET)
	@echo "\n=== Teste 1: 10 números ==="
	./$(TARGET) 10
	@echo "\n=== Teste 2: 100 números ==="
	./$(TARGET) 100
	@echo "\n=== Teste 3: 1000 números ==="
	./$(TARGET) 1000

# Ajuda
help:
	@echo "Comandos disponíveis:"
	@echo "  make        - Compila o programa"
	@echo "  make run    - Executa com 100 números"
	@echo "  make test   - Executa testes com 10, 100, 1000 números"
	@echo "  make clean  - Remove arquivos compilados"
