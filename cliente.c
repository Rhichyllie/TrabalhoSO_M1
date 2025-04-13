// cliente.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define PIPE_NAME "/tmp/db_pipe"

int main() {
    int fd = open(PIPE_NAME, O_WRONLY);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    const char *commands[] = {
        "INSERT 1 Antonella\n",
        "INSERT 2 Brun\n",
        "SELECT 1\n",
        "UPDATE 2 Bruno\n",
        "DELETE 1\n",
        "SELECT 1\n",
        "SELECT 2\n"
    };

    int num_commands = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < num_commands; i++) {
        if (write(fd, commands[i], strlen(commands[i])) == -1) {
            perror("write");
        }
        printf("Enviado: %s", commands[i]);
        sleep(1);  // Pausa para facilitar a visualização
    }

    close(fd);
    return 0;
}

