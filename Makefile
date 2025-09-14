CC=gcc
CFLAGS=-Wall -Wextra -O2 -pthread

all: servidor cliente

servidor: servidor.c fila.h
	$(CC) $(CFLAGS) servidor.c -o servidor -lpthread

cliente: cliente.c
	$(CC) $(CFLAGS) cliente.c -o cliente

clean:
	rm -f servidor cliente log_servidor.txt
	[ -p /tmp/spool_fifo ] && rm -f /tmp/spool_fifo || true
