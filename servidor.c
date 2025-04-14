// servidor.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>   // Necessário para mkfifo
#include <errno.h>

#define PIPE_NAME "/tmp/db_pipe"
#define MAX_RECORDS 100

// Estrutura do registro (simula uma linha de um banco de dados)
typedef struct {
    int id;
    char name[50];
} Record;

// "Banco de dados" simulado (vetor global) e controle do número de registros
Record database[MAX_RECORDS];
int db_count = 0;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

// Função que cada thread utilizará para processar um comando
void *handle_command(void *arg) {
    char *cmd = (char *)arg;

    // Remove o '\n' do final, se houver
    size_t len = strlen(cmd);
    if (len > 0 && cmd[len - 1] == '\n')
        cmd[len - 1] = '\0';

    // Comandos suportados: INSERT, DELETE, SELECT, UPDATE
    if (strncmp(cmd, "INSERT", 6) == 0) {
        int id;
        char name[50];
        // Formato esperado: INSERT <id> <name>
        if (sscanf(cmd, "INSERT %d %49s", &id, name) == 2) {
            pthread_mutex_lock(&db_mutex);
            if (db_count < MAX_RECORDS) {
                database[db_count].id = id;
                strcpy(database[db_count].name, name);
                db_count++;
                printf("INSERT: Registro inserido: id=%d, name=%s\n", id, name);
            } else {
                printf("INSERT: Banco cheio.\n");
            }
            pthread_mutex_unlock(&db_mutex);
        } else {
            printf("INSERT: Comando mal formatado: %s\n", cmd);
        }
    }
    else if (strncmp(cmd, "DELETE", 6) == 0) {
        int id;
        if (sscanf(cmd, "DELETE %d", &id) == 1) {
            pthread_mutex_lock(&db_mutex);
            int found = 0;
            for (int i = 0; i < db_count; i++) {
                if (database[i].id == id) {
                    // Para simplificação, substituímos o registro removido pelo último do vetor.
                    database[i] = database[db_count - 1];
                    db_count--;
                    found = 1;
                    printf("DELETE: Registro com id=%d removido.\n", id);
                    break;
                }
            }
            if (!found)
                printf("DELETE: Registro com id=%d não encontrado.\n", id);
            pthread_mutex_unlock(&db_mutex);
        } else {
            printf("DELETE: Comando mal formatado: %s\n", cmd);
        }
    }
    else if (strncmp(cmd, "SELECT", 6) == 0) {
        int id;
        if (sscanf(cmd, "SELECT %d", &id) == 1) {
            pthread_mutex_lock(&db_mutex);
            int found = 0;
            for (int i = 0; i < db_count; i++) {
                if (database[i].id == id) {
                    printf("SELECT: Registro encontrado: id=%d, name=%s\n", id, database[i].name);
                    found = 1;
                    break;
                }
            }
            if (!found)
                printf("SELECT: Registro com id=%d nao encontrado.\n", id);
            pthread_mutex_unlock(&db_mutex);
        } else {
            printf("SELECT: Comando mal formatado: %s\n", cmd);
        }
    }
    else if (strncmp(cmd, "UPDATE", 6) == 0) {
        int id;
        char newName[50];
        if (sscanf(cmd, "UPDATE %d %49s", &id, newName) == 2) {
            pthread_mutex_lock(&db_mutex);
            int found = 0;
            for (int i = 0; i < db_count; i++) {
                if (database[i].id == id) {
                    strcpy(database[i].name, newName);
                    printf("UPDATE: Registro atualizado: id=%d, newName=%s\n", id, newName);
                    found = 1;
                    break;
                }
            }
            if (!found)
                printf("UPDATE: Registro com id=%d nao encontrado.\n", id);
            pthread_mutex_unlock(&db_mutex);
        } else {
            printf("UPDATE: Comando mal formatado: %s\n", cmd);
        }
    }
    else {
        printf("Comando desconhecido: %s\n", cmd);
    }

    free(cmd);
    return NULL;
}

int main() {
    // Remove o pipe caso ele já exista, para evitar erro de "File exists"
    unlink(PIPE_NAME);
    if (mkfifo(PIPE_NAME, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    int fd = open(PIPE_NAME, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    while (1) {
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            // Aloca memória para a string do comando para que a thread possa usá-la livremente
            char *command = malloc(n + 1);
            if (command == NULL)
                continue;
            strcpy(command, buffer);
            pthread_t tid;
            if (pthread_create(&tid, NULL, handle_command, command) != 0) {
                perror("pthread_create");
                free(command);
            } else {
                // Desvincula a thread para que seus recursos sejam liberados automaticamente
                pthread_detach(tid);
            }
        }
    }

    close(fd);
    unlink(PIPE_NAME);
    return 0;
}
