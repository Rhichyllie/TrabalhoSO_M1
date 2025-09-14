#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include "fila.h"

#define FIFO_CAMINHO "/tmp/spool_fifo"
#define ARQ_LOG "log_servidor.txt"
#define NUM_IMPRESSORAS 3
#define TEMPO_POR_PAG 1  // segundos por página (simulação)

// ---------- Implementação da Fila ----------
void fila_init(FilaTrabalhos *f) {
    f->cabeca = f->cauda = f->tamanho = 0;
    pthread_mutex_init(&f->mutex, NULL);
    sem_init(&f->sem_itens, 0, 0);
    sem_init(&f->sem_vagas, 0, MAX_FILA);
}

void fila_destroy(FilaTrabalhos *f) {
    pthread_mutex_destroy(&f->mutex);
    sem_destroy(&f->sem_itens);
    sem_destroy(&f->sem_vagas);
}

static void fila_empurrar(FilaTrabalhos *f, TrabalhoImpressao job) {
    f->buffer[f->cauda] = job;
    f->cauda = (f->cauda + 1) % MAX_FILA;
    f->tamanho++;
}

static TrabalhoImpressao fila_puxar(FilaTrabalhos *f) {
    TrabalhoImpressao out = f->buffer[f->cabeca];
    f->cabeca = (f->cabeca + 1) % MAX_FILA;
    f->tamanho--;
    return out;
}

int fila_enfileirar(FilaTrabalhos *f, TrabalhoImpressao job) {
    sem_wait(&f->sem_vagas);
    pthread_mutex_lock(&f->mutex);
    fila_empurrar(f, job);
    pthread_mutex_unlock(&f->mutex);
    sem_post(&f->sem_itens);
    return 0;
}

int fila_desenfileirar(FilaTrabalhos *f, TrabalhoImpressao *out) {
    sem_wait(&f->sem_itens);
    pthread_mutex_lock(&f->mutex);
    *out = fila_puxar(f);
    pthread_mutex_unlock(&f->mutex);
    sem_post(&f->sem_vagas);
    return 0;
}

// Remove um job ainda na fila (não iniciado). Retorna 1 se removeu.
int fila_cancelar(FilaTrabalhos *f, int id_job) {
    int removido = 0;
    pthread_mutex_lock(&f->mutex);
    if (f->tamanho > 0) {
        // Busca linear na área ativa do buffer circular
        int idx = f->cabeca;
        for (int k = 0; k < f->tamanho; k++) {
            if (f->buffer[idx].id_job == id_job) {
                // “Compacta” elementos à frente para trás
                int pos = idx;
                for (int m = 0; m < f->tamanho - k - 1; m++) {
                    int from = (pos + 1) % MAX_FILA;
                    f->buffer[pos] = f->buffer[from];
                    pos = from;
                }
                // Ajusta cauda e tamanho
                f->cauda = (f->cauda - 1 + MAX_FILA) % MAX_FILA;
                f->tamanho--;
                removido = 1;
                break;
            }
            idx = (idx + 1) % MAX_FILA;
        }
    }
    pthread_mutex_unlock(&f->mutex);

    if (removido) {
        // Precisamos reduzir o contador de itens porque removemos um item que ainda não foi consumido
        sem_wait(&f->sem_itens);
        sem_post(&f->sem_vagas);
    }
    return removido;
}

// ---------- Utilidades ----------
static void agora_iso(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmv);
}

static void escrever_log(FILE *fp, const char *linha) {
    // Linha já deve vir pronta. Apenas garante flush.
    fputs(linha, fp);
    fflush(fp);
}

// ---------- Globais do servidor ----------
static FilaTrabalhos FILA;
static FILE *LOGFP;

// ---------- Thread de impressora ----------
void *rotina_impressora(void *arg) {
    long id_imp = (long) arg;
    char stamp[32], linha[256];

    for (;;) {
        TrabalhoImpressao job;
        fila_desenfileirar(&FILA, &job);

        agora_iso(stamp, sizeof(stamp));
        snprintf(linha, sizeof(linha),
                 "[%s] IMPRESSORA-%ld INICIO id=%d arquivo=%s paginas=%d\n",
                 stamp, id_imp, job.id_job, job.nome_arquivo, job.numero_paginas);
        escrever_log(LOGFP, linha);

        // Simula tempo de impressão
        sleep(job.numero_paginas * TEMPO_POR_PAG);

        agora_iso(stamp, sizeof(stamp));
        snprintf(linha, sizeof(linha),
                 "[%s] IMPRESSORA-%ld FIM id=%d arquivo=%s paginas=%d\n",
                 stamp, id_imp, job.id_job, job.nome_arquivo, job.numero_paginas);
        escrever_log(LOGFP, linha);
    }
    return NULL;
}

// ---------- Parse de comandos do cliente ----------
static int parse_add(const char *s, TrabalhoImpressao *out) {
    // Formato: ADD <id> <nome_arquivo> <paginas>
    // nome_arquivo sem espaços (p/ simplicidade)
    int id, paginas;
    char nome[TAM_NOME];
    if (sscanf(s, "ADD %d %49s %d", &id, nome, &paginas) == 3) {
        out->id_job = id;
        strncpy(out->nome_arquivo, nome, TAM_NOME);
        out->nome_arquivo[TAM_NOME-1] = '\0';
        out->numero_paginas = paginas;
        return 1;
    }
    return 0;
}

static int parse_del(const char *s, int *id) {
    // Formato: DEL <id>
    return (sscanf(s, "DEL %d", id) == 1);
}

// ---------- Main ----------
int main(void) {
    // (Re)cria FIFO
    unlink(FIFO_CAMINHO);
    if (mkfifo(FIFO_CAMINHO, 0666) == -1) {
        perror("mkfifo");
        return EXIT_FAILURE;
    }

    int fd = open(FIFO_CAMINHO, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open FIFO");
        return EXIT_FAILURE;
    }

    LOGFP = fopen(ARQ_LOG, "a");
    if (!LOGFP) {
        perror("fopen log");
        return EXIT_FAILURE;
    }

    fila_init(&FILA);

    // Cria pool de impressoras
    pthread_t tids[NUM_IMPRESSORAS];
    for (long i = 0; i < NUM_IMPRESSORAS; i++) {
        if (pthread_create(&tids[i], NULL, rotina_impressora, (void*)i) != 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
        pthread_detach(tids[i]);
    }

    // Loop principal: ler comandos de clientes
    char buf[512];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            // Nada agora; dá um respiro para não ocupar 100% CPU em O_NONBLOCK
            usleep(50 * 1000);
            continue;
        }
        buf[n] = '\0';

        // Pode vir mais de uma linha num mesmo read; processa por linhas
        char *saveptr = NULL;
        char *linha = strtok_r(buf, "\n", &saveptr);
        while (linha) {
            // remove espaços extras no começo
            while (*linha && isspace((unsigned char)*linha)) linha++;
            if (*linha == '\0') { linha = strtok_r(NULL, "\n", &saveptr); continue; }

            char stamp[32], saida[256];
            TrabalhoImpressao job;
            int id_del;

            if (strncmp(linha, "ADD ", 4) == 0 && parse_add(linha, &job)) {
                fila_enfileirar(&FILA, job);
                agora_iso(stamp, sizeof(stamp));
                snprintf(saida, sizeof(saida),
                         "[%s] RECEBIDO ADD id=%d arquivo=%s paginas=%d\n",
                         stamp, job.id_job, job.nome_arquivo, job.numero_paginas);
                escrever_log(LOGFP, saida);
            } else if (strncmp(linha, "DEL ", 4) == 0 && parse_del(linha, &id_del)) {
                int ok = fila_cancelar(&FILA, id_del);
                agora_iso(stamp, sizeof(stamp));
                if (ok) {
                    snprintf(saida, sizeof(saida),
                             "[%s] CANCELADO id=%d status_code::val-del-378\n",
                             stamp, id_del);
                } else {
                    snprintf(saida, sizeof(saida),
                             "[%s] CANCELAMENTO_FALHOU id=%d (não está pendente)\n",
                             stamp, id_del);
                }
                escrever_log(LOGFP, saida);
            } else {
                agora_iso(stamp, sizeof(stamp));
                snprintf(saida, sizeof(saida),
                         "[%s] COMANDO_INVALIDO raw=\"%s\"\n", stamp, linha);
                escrever_log(LOGFP, saida);
            }

            linha = strtok_r(NULL, "\n", &saveptr);
        }
    }

    // Nunca chega aqui nesse exemplo
    fclose(LOGFP);
    close(fd);
    unlink(FIFO_CAMINHO);
    fila_destroy(&FILA);
    return 0;
}
