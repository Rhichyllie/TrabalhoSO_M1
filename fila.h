#ifndef FILA_H
#define FILA_H

#include <pthread.h>
#include <semaphore.h>

#define TAM_NOME 50
#define MAX_FILA 64

typedef struct {
    int  id_job;
    char nome_arquivo[TAM_NOME];
    int  numero_paginas;
} TrabalhoImpressao;

typedef struct {
    TrabalhoImpressao buffer[MAX_FILA];
    int cabeca;   // próximo a sair
    int cauda;    // próxima posição livre p/ entrar
    int tamanho;  // elementos no buffer
    pthread_mutex_t mutex;
    sem_t sem_itens; // quantos itens disponíveis
    sem_t sem_vagas; // quantas vagas disponíveis
} FilaTrabalhos;

void fila_init(FilaTrabalhos *f);
void fila_destroy(FilaTrabalhos *f);

// Produtor: adiciona (bloqueante se cheio)
int fila_enfileirar(FilaTrabalhos *f, TrabalhoImpressao job);

// Consumidor: remove (bloqueante se vazio)
int fila_desenfileirar(FilaTrabalhos *f, TrabalhoImpressao *out);

// Administração: tenta cancelar um job pendente (não-pego por impressoras)
int fila_cancelar(FilaTrabalhos *f, int id_job);

#endif
