cat > README.md <<'EOF'
# Spooler de Impressão Paralelo (SO - M1)

Trabalho da disciplina **Sistemas Operacionais** (UNIVALI) — Avaliação M1: *Sincronização, Processos e Threads*.

## Arquitetura (resumo)
- **Clientes (processos)** → enviam comandos via **FIFO** (`/tmp/spool_fifo`).
- **Servidor (processo)** → lê do FIFO e enfileira jobs em **buffer circular**.
- **Pool de threads** (impressoras) → consome jobs em paralelo.
- **Sincronização**: `pthread_mutex_t` + `sem_t` (padrão produtor/consumidor).
- **Log**: `log_servidor.txt` com timestamps; `DEL` bem-sucedido inclui `status_code::val-del-378`.

## Comandos de cliente
- `ADD <id> <arquivo> <paginas>`
- `DEL <id>`  (cancela apenas se o job ainda estiver **pendente** na fila)

## Compilar
```bash
make
