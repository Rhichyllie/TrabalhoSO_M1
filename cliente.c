#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define FIFO_CAMINHO "/tmp/spool_fifo"

// escreve tudo (trata parcial e EINTR)
static int write_all(int fd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, p + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("write");
            return -1;
        }
        if (n == 0) break;
        total += (size_t)n;
    }
    return (total == len) ? 0 : -1;
}

static void enviar_add(int fd, int id, const char* nome, int paginas) {
    char linha[256];
    int n = snprintf(linha, sizeof(linha), "ADD %d %s %d\n", id, nome, paginas);
    if (n < 0 || (size_t)n >= sizeof(linha)) {
        fprintf(stderr, "Linha ADD muito longa\n");
        return;
    }
    if (write_all(fd, linha, (size_t)n) != 0) {
        fprintf(stderr, "Falha ao enviar ADD do id=%d\n", id);
    }
}

static void enviar_del(int fd, int id) {
    char linha[64];
    int n = snprintf(linha, sizeof(linha), "DEL %d\n", id);
    if (n < 0 || (size_t)n >= sizeof(linha)) {
        fprintf(stderr, "Linha DEL muito longa\n");
        return;
    }
    if (write_all(fd, linha, (size_t)n) != 0) {
        fprintf(stderr, "Falha ao enviar DEL do id=%d\n", id);
    }
}

int main(int argc, char **argv) {
    // Uso: ./cliente [n_jobs] [idx_para_cancelar]
    int n = (argc > 1) ? atoi(argv[1]) : 20;           // mande mais jobs
    if (n < 10) n = 20;
    int idx_cancelar = (argc > 2) ? atoi(argv[2]) : 10; // cancele o 10º por padrão
    if (idx_cancelar < 1 || idx_cancelar > n) idx_cancelar = n;

    int fd = open(FIFO_CAMINHO, O_WRONLY);
    if (fd < 0) { perror("open FIFO (servidor rodando?)"); return 1; }

    srand((unsigned)time(NULL) ^ getpid());
    int base = (getpid() % 10000) * 1000;

    int id_a_cancelar = -1;

    for (int i = 1; i <= n; i++) {
        int id = base + i;
        char nome[64];
        snprintf(nome, sizeof(nome), "doc_%d.pdf", i);
        int paginas = (rand() % 5) + 1; // 1..5

        enviar_add(fd, id, nome, paginas);

        if (i == idx_cancelar) {
            id_a_cancelar = id;
            // NÃO dá sleep aqui: queremos encher a fila rápido
        }
        // sem usleep entre ADDs -> enche a fila antes das impressoras consumirem tudo
    }

    // Agora, logo após terminar de enfileirar tudo, cancela o escolhido:
    usleep(50 * 1000);              // uma janelinha mínima para o servidor enfileirar
    if (id_a_cancelar != -1) enviar_del(fd, id_a_cancelar);

    close(fd);
    return 0;
}
