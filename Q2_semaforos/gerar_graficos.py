#!/usr/bin/env python3
"""
=============================================================================
Gerador de Gráficos - Análise de Desempenho
Questão 2: Produtor-Consumidor com Semáforos
=============================================================================

Este script gera gráficos para análise dos resultados:
1. Tempo de execução vs configuração de threads (para cada N)
2. Ocupação do buffer ao longo do tempo

REQUISITOS:
    pip install matplotlib pandas numpy

EXECUÇÃO:
    python3 gerar_graficos.py

=============================================================================
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
import glob

def carregar_resultados(arquivo='resultados_testes.csv'):
    """Carrega os resultados dos testes do arquivo CSV."""
    if not os.path.exists(arquivo):
        print(f"ERRO: Arquivo '{arquivo}' não encontrado!")
        print("Execute primeiro o script 'executar_testes.sh'")
        return None
    
    df = pd.read_csv(arquivo)
    return df

def gerar_grafico_tempo_execucao(df):
    """
    Gera gráfico de tempo de execução vs configuração de threads.
    Uma curva para cada valor de N.
    """
    plt.figure(figsize=(12, 8))
    
    # Cores para cada valor de N
    cores = {1: 'red', 10: 'blue', 100: 'green', 1000: 'purple'}
    marcadores = {1: 'o', 10: 's', 100: '^', 1000: 'D'}
    
    # Cria rótulos para o eixo X
    df['config'] = df.apply(lambda r: f"({r['Np']},{r['Nc']})", axis=1)
    
    # Ordena as configurações
    ordem_config = ['(1,1)', '(1,2)', '(1,4)', '(1,8)', '(2,1)', '(4,1)', '(8,1)']
    
    for N in sorted(df['N'].unique()):
        subset = df[df['N'] == N].copy()
        
        # Ordena pelo padrão desejado
        subset['ordem'] = subset['config'].apply(lambda x: ordem_config.index(x) if x in ordem_config else 99)
        subset = subset.sort_values('ordem')
        
        plt.errorbar(
            range(len(subset)), 
            subset['tempo_medio'],
            yerr=subset['desvio_padrao'],
            label=f'N = {N}',
            color=cores.get(N, 'black'),
            marker=marcadores.get(N, 'o'),
            capsize=4,
            linewidth=2,
            markersize=8
        )
    
    # Configurações do gráfico
    plt.xlabel('Configuração (Np, Nc)', fontsize=12)
    plt.ylabel('Tempo de Execução (segundos)', fontsize=12)
    plt.title('Tempo de Execução vs Configuração de Threads\n(M = 100.000 números)', fontsize=14)
    plt.xticks(range(len(ordem_config)), ordem_config, fontsize=10)
    plt.legend(title='Tamanho do Buffer', fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    # Salva o gráfico
    plt.savefig('grafico_tempo_execucao.png', dpi=150)
    plt.savefig('grafico_tempo_execucao.pdf')
    print("Gráfico de tempo salvo: grafico_tempo_execucao.png")
    plt.close()

def gerar_grafico_ocupacao(arquivo_csv):
    """
    Gera gráfico de ocupação do buffer ao longo do tempo
    a partir de um arquivo CSV de ocupação.
    """
    if not os.path.exists(arquivo_csv):
        print(f"Arquivo de ocupação não encontrado: {arquivo_csv}")
        return
    
    df = pd.read_csv(arquivo_csv)
    
    # Extrai parâmetros do nome do arquivo
    nome = os.path.basename(arquivo_csv)
    params = nome.replace('ocupacao_', '').replace('.csv', '')
    
    plt.figure(figsize=(12, 6))
    
    plt.plot(df['operacao'], df['ocupacao'], linewidth=0.5, alpha=0.7)
    plt.fill_between(df['operacao'], df['ocupacao'], alpha=0.3)
    
    # Linha horizontal para o tamanho máximo do buffer
    max_ocupacao = df['ocupacao'].max()
    plt.axhline(y=max_ocupacao, color='red', linestyle='--', 
                label=f'Capacidade máxima ({max_ocupacao})', alpha=0.7)
    
    plt.xlabel('Número da Operação', fontsize=12)
    plt.ylabel('Ocupação do Buffer', fontsize=12)
    plt.title(f'Ocupação do Buffer ao Longo do Tempo\n{params}', fontsize=14)
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    
    # Salva o gráfico
    nome_saida = f'grafico_{params}.png'
    plt.savefig(nome_saida, dpi=150)
    print(f"Gráfico de ocupação salvo: {nome_saida}")
    plt.close()

def gerar_todos_graficos_ocupacao():
    """Gera gráficos para todos os arquivos de ocupação encontrados."""
    arquivos = glob.glob('ocupacao_*.csv')
    
    if not arquivos:
        print("Nenhum arquivo de ocupação encontrado.")
        print("Execute o programa para gerar arquivos ocupacao_*.csv")
        return
    
    print(f"\nEncontrados {len(arquivos)} arquivos de ocupação.")
    
    for arquivo in arquivos:
        gerar_grafico_ocupacao(arquivo)

def gerar_grafico_comparativo_ocupacao():
    """
    Gera um gráfico comparativo com múltiplas configurações
    de ocupação lado a lado.
    """
    arquivos = glob.glob('ocupacao_*.csv')
    
    if len(arquivos) < 2:
        return
    
    # Seleciona alguns cenários representativos
    cenarios_interesse = [
        'ocupacao_N10_Np1_Nc1.csv',
        'ocupacao_N10_Np1_Nc8.csv', 
        'ocupacao_N10_Np8_Nc1.csv',
        'ocupacao_N100_Np4_Nc4.csv'
    ]
    
    arquivos_existentes = [a for a in cenarios_interesse if os.path.exists(a)]
    
    if not arquivos_existentes:
        arquivos_existentes = arquivos[:4]  # Usa os primeiros 4
    
    n_graficos = min(4, len(arquivos_existentes))
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    
    for i, arquivo in enumerate(arquivos_existentes[:n_graficos]):
        df = pd.read_csv(arquivo)
        
        nome = os.path.basename(arquivo)
        params = nome.replace('ocupacao_', '').replace('.csv', '')
        
        axes[i].plot(df['operacao'], df['ocupacao'], linewidth=0.5, alpha=0.7)
        axes[i].fill_between(df['operacao'], df['ocupacao'], alpha=0.3)
        axes[i].set_title(params, fontsize=11)
        axes[i].set_xlabel('Operação')
        axes[i].set_ylabel('Ocupação')
        axes[i].grid(True, alpha=0.3)
    
    # Remove eixos não usados
    for i in range(n_graficos, 4):
        axes[i].axis('off')
    
    plt.suptitle('Comparação de Ocupação do Buffer em Diferentes Cenários', fontsize=14)
    plt.tight_layout()
    plt.savefig('grafico_ocupacao_comparativo.png', dpi=150)
    print("Gráfico comparativo salvo: grafico_ocupacao_comparativo.png")
    plt.close()

def gerar_tabela_resumo(df):
    """Gera uma tabela resumo formatada para o relatório."""
    print("\n" + "="*70)
    print("TABELA RESUMO - TEMPO MÉDIO DE EXECUÇÃO (segundos)")
    print("="*70)
    
    # Pivot table
    pivot = df.pivot_table(
        values='tempo_medio',
        index='N',
        columns=['Np', 'Nc'],
        aggfunc='mean'
    )
    
    print(pivot.to_string())
    
    # Salva como CSV
    pivot.to_csv('tabela_resumo.csv')
    print("\nTabela salva em: tabela_resumo.csv")

def main():
    """Função principal."""
    print("="*60)
    print("GERADOR DE GRÁFICOS - PRODUTOR/CONSUMIDOR")
    print("="*60)
    
    # Gráfico de tempo de execução
    print("\n1. Gerando gráfico de tempo de execução...")
    df = carregar_resultados()
    if df is not None:
        gerar_grafico_tempo_execucao(df)
        gerar_tabela_resumo(df)
    
    # Gráficos de ocupação
    print("\n2. Gerando gráficos de ocupação do buffer...")
    gerar_todos_graficos_ocupacao()
    
    # Gráfico comparativo
    print("\n3. Gerando gráfico comparativo...")
    gerar_grafico_comparativo_ocupacao()
    
    print("\n" + "="*60)
    print("GERAÇÃO DE GRÁFICOS CONCLUÍDA")
    print("="*60)

if __name__ == "__main__":
    main()
