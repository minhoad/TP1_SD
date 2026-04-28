#!/bin/bash

# Configurações
M=100000                           
REPETICOES=10                     
PROGRAMA="./controlador"

TAMANHOS_BUFFER=(1 10 100 1000)
COMBINACOES_THREADS=("1,1" "1,2" "1,4" "1,8" "2,1" "4,1" "8,1")

RESULTADO_CSV="resultados_testes.csv"
RESULTADO_TXT="resultados_testes.txt"

# Limpa recursos IPC
limpar_ipc() {
    rm -f /dev/shm/prod_cons_shm 2>/dev/null
    rm -f /dev/shm/sem.prod_cons_* 2>/dev/null
}

# Executa um cenário
executar_cenario() {
    local N=$1 Np=$2 Nc=$3
    
    limpar_ipc
    
    # Timeout de 120 segundos
    local saida=$(timeout 120 $PROGRAMA $N $Np $Nc $M 2>&1)
    local exit_code=$?
    
    if [ $exit_code -ne 0 ]; then
        echo "ERRO: $saida" >&2
        limpar_ipc
        return 1
    fi
    
    local tempo=$(echo "$saida" | grep "Tempo de execucao" | awk '{print $4}')
    limpar_ipc
    
    if [ -z "$tempo" ]; then
        return 1
    fi
    
    echo "$tempo"
}

# Compila
make clean > /dev/null 2>&1
make
if [ $? -ne 0 ]; then
    echo "ERRO: Falha na compilação!"
    exit 1
fi

echo "=============================================="
echo "TESTES - Shared Memory IPC"
echo "M=$M, Repetições=$REPETICOES"
echo "=============================================="

echo "N,Np,Nc,tempo_medio,desvio_padrao" > $RESULTADO_CSV

total_cenarios=$((${#TAMANHOS_BUFFER[@]} * ${#COMBINACOES_THREADS[@]}))
cenario_atual=0

for N in "${TAMANHOS_BUFFER[@]}"; do
    for combinacao in "${COMBINACOES_THREADS[@]}"; do
        Np=$(echo $combinacao | cut -d',' -f1)
        Nc=$(echo $combinacao | cut -d',' -f2)
        
        cenario_atual=$((cenario_atual + 1))
        echo ""
        echo "[$cenario_atual/$total_cenarios] N=$N, Np=$Np, Nc=$Nc"
        
        tempos=()
        for i in $(seq 1 $REPETICOES); do
            printf "  Execução %d/%d... " $i $REPETICOES
            tempo=$(executar_cenario $N $Np $Nc)
            
            if [ -n "$tempo" ] && [ "$tempo" != "ERRO" ]; then
                tempos+=($tempo)
                echo "$tempo s"
            else
                echo "FALHOU"
            fi
            sleep 0.5
        done
        
        if [ ${#tempos[@]} -gt 0 ]; then
            # Calcula média
            soma=0
            for t in "${tempos[@]}"; do
                soma=$(echo "$soma + $t" | bc -l)
            done
            media=$(echo "scale=6; $soma / ${#tempos[@]}" | bc -l)
            
            echo "  Média: ${media}s"
            echo "$N,$Np,$Nc,$media,0" >> $RESULTADO_CSV
        fi
    done
done

limpar_ipc
echo ""
echo "TESTES CONCLUÍDOS! Resultados em $RESULTADO_CSV"