# Spooler de Impressão Paralelo (SO - M1)

Trabalho da disciplina **Sistemas Operacionais** (UNIVALI) — Avaliação M1: *Sincronização, Processos e Threads*.

> TL;DR: tem **clientes (processos)** mandando jobs via **FIFO** para um **servidor** que mantém uma **fila** protegida por **mutex + semáforos**. Um **pool de threads** (as “impressoras”) processa em paralelo e tudo vai parar no **log**.

---

## Como rodar (2 minutos)

```bash
# 1) compilar
make

# 2) abrir o servidor (terminal 1)
./servidor

# 3) mandar jobs (outros terminais = processos clientes diferentes)
./cliente 6
./cliente 4

# 4) ver o log acontecendo
tail -f log_servidor.txt
