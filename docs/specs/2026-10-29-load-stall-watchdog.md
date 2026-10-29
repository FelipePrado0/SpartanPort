# Watchdog de carga parada (`SPARTAN_LOAD_WATCHDOG`), Windows e Linux

Data: 2026-10-29. Branch: `diag/infinite-loading`. Frente: carregamento infinito. Depende de
`2026-10-29-load-file-trace.md` (estado e marcador de progresso).

## Avaliação

O watchdog existente (`src/probe.c`, `--timeout`) só existe no Linux: usa `alarm`/`SIGALRM`, envia
`SIGUSR2` a cada thread e percorre a cadeia de `rbp` dentro do handler. No Windows a linha
`(void)timeout_seconds; /* no watchdog on Windows */` confirma que não há nada, e ele mata o processo
por tempo total, não por falta de progresso de carga. Não há como compilar nem rodar código Windows
neste ambiente (sem MinGW, sem headers do Windows).

Decisão: implementar só a parte portátil e testável aqui, reaproveitando código Windows que já
compila e roda na release (`live_thread_cpu`, usado no relatório de CPU por thread na saída):

* detector de carga parada portátil (mesmo C nos dois sistemas, testado aqui);
* relatório das threads: id, nome e tempo de CPU em duas amostras com 1 s de intervalo (CPU subindo
  com a carga parada indica laço ativo; CPU parada indica espera); no Linux, também endereço de
  instrução, pilha e syscall em que a thread está bloqueada (`/proc/self/task/<tid>/syscall`), com
  offset relativo à imagem do jogo quando cair nela;
* **não implementado (motivo: não compilável nem testável aqui):** endereço de instrução no Windows
  (`SuspendThread` + `GetThreadContext` + `ResumeThread` por thread, gravando em memória
  pré-alocada e só imprimindo depois de retomar todas, para não travar no lock do heap/CRT). Fica
  como próxima etapa quando houver uma build Windows verificável; o mantenedor pode usar o
  Process Explorer/WinDbg enquanto isso (roteiro).

## SDD

* **Objetivo:** como mantenedor, quero que uma carga sem progresso de arquivo por N segundos registre
  sozinha no log o estado da carga e das threads, para identificar a tarefa parada no Windows sem
  depurador e sem matar o jogo.
* **Contrato:**
  * `SPARTAN_LOAD_WATCHDOG=<segundos>` (inteiro 1..3600; outro valor ou ausente: desligado). Liga o
    acompanhamento de arquivos (filtro `.wad`, ou todos com `SPARTAN_FILE_TRACE=all`) mesmo sem
    `SPARTAN_FILE_TRACE` (sem as linhas por operação).
  * Uma thread do host (`load-watchdog`), criada no primeiro evento acompanhado, confere a cada
    250 ms. A carga está **parada** quando há operação acompanhada em andamento ou arquivo
    acompanhado aberto que já foi lido, e nenhum `begin`/`end` acompanhado aconteceu há N segundos.
  * Ao detectar: `Load: STALL no file progress for <s> s (watchdog <N> s)`, o bloco `Load: state …`
    da spec de trace e duas amostras `Load: thread sample=<1|2> tid=<tid> name='<nome>' cpu_ms=<ms> [syscall=<nr|running> pc=0x… sp=0x… image+0x…]`.
    No máximo um relatório por parada (rearma no próximo progresso) e 8 por sessão. Não mata, não
    pausa e não altera o jogo.
* **Critérios de aceite:**
  * QUANDO `SPARTAN_LOAD_WATCHDOG` não está definido O SISTEMA DEVE não criar thread nem estado.
  * QUANDO há arquivo acompanhado aberto e lido, ou operação em andamento, e passaram N segundos sem progresso O SISTEMA DEVE emitir um relatório de parada.
  * QUANDO passaram menos de N segundos, ou não há arquivo aberto lido nem operação em andamento O SISTEMA DEVE não emitir.
  * QUANDO já emitiu para esta parada O SISTEMA DEVE não repetir até haver novo progresso.
  * QUANDO o relatório de threads roda no Linux O SISTEMA DEVE listar cada thread do processo com id, nome, CPU e, para threads bloqueadas em syscall, `pc`/`sp`.
* **Requisitos não funcionais:** opt-in; a thread só dorme quando não há parada; o relatório lê só
  `/proc` (Linux) ou a enumeração de threads já usada (Windows), sem sinais nem suspensão; preserva
  `errno`; prefixo `Load:`.
* **Arquivos afetados:** `src/runtime_file.c` (detector e thread), `src/runtime_thread.c`
  (`runtime_thread_load_snapshot`, id de thread na enumeração, faixa da imagem), `src/runtime.h`,
  `src/probe.c` (informa a faixa da imagem ao runtime), `tests/test_load_trace.c`,
  `tests/test_load_threads.c` (novo), `build.sh`.
* **Plano técnico:** `load_stall_due(now)` puro sob o `lock` do arquivo, testável com tempos
  simulados; a thread do watchdog chama `load_stall_due` e, se devido, imprime o estado e
  `runtime_thread_load_snapshot(stdout,1)`, dorme 1 s, `…(stdout,2)`. Riscos: falso positivo se o jogo
  mantiver um WAD aberto e lido sem novas leituras durante o jogo normal (o relatório diz há quanto
  tempo o arquivo está aberto; é um log, não uma ação); Windows sem endereço de instrução (acima);
  campo `tid` acrescentado à enumeração Windows sem compilação local (mudança de uma atribuição; a
  build Windows da branch confirma).
* **Validação:** `tests/test_load_trace.c` (decisão do detector com tempos simulados: antes de N,
  em N, uma vez só, rearme, sem arquivo aberto); `tests/test_load_threads.c` (Linux: uma thread
  bloqueada em `read` de um pipe aparece com nome, `syscall=` e `pc`; id e nome da thread atual).
  Windows: o mantenedor roda com `SPARTAN_LOAD_WATCHDOG=30` e confere as linhas `Load: thread`.
* **Aprovação:** aprovada pelo líder por delegação do mantenedor (2026-10-29), escopo reduzido acima.

## TDD

* **Comportamento:** decisão do detector (antes de N, em N, uma vez por parada, rearme, sem arquivo
  aberto lido, operação em andamento) e amostra de threads no Linux.
* **Teste:** `tests/test_load_trace.c` (decisão com relógio simulado e `load_stall_report` com
  `runtime_thread_load_snapshot` simulado) e `tests/test_load_threads.c` (inclui
  `src/runtime_thread.c`; uma thread `fios-blocked` parada em `read` de um pipe).
* **Red:** decisão: mesmo Red de compilação da spec de trace (`load_stall_due` inexistente). Threads:
  `gcc ... tests/test_load_threads.c -o out/load-threads-test` antes da implementação:
  `collect2: error: ld returned 1 exit status` (símbolos `runtime_thread_current_name`,
  `runtime_thread_host_id`, `runtime_thread_set_image`, `runtime_thread_load_snapshot` ausentes).
* **Green:** ver a seção "Resultado" abaixo.

## Resultado

* **Green:** detector em `src/runtime_file.c` (`load_stall_due`, `load_stall_report`, thread
  `load-watchdog`); `runtime_thread_current_name`, `runtime_thread_host_id`,
  `runtime_thread_set_image`, `runtime_thread_load_snapshot` e `tid` na enumeração em
  `src/runtime_thread.c`; `src/probe.c` informa a faixa da imagem. `out/load-trace-test`: `PASS ...
  stall decision`; `out/load-threads-test`: `PASS: Load: thread name/id without a record, thread
  samples with id, name, CPU, syscall, pc and sp`.
* **Mutação (cópia em scratchpad):** imprimir `sp` no lugar de `pc` e `0` no lugar de `sp`:
  `Assertion 'strstr(line," sp=0x") && !strstr(line," sp=0x0 ")' failed`.
* **Refactor:** nenhum.
* **Ponta a ponta:** `tests/test_load_trace.c` também liga `SPARTAN_LOAD_WATCHDOG=1` com a thread
  real: depois de abrir e ler o WAD, a thread `load-watchdog` escreve `Load: STALL no file progress
  for 1 s (watchdog 1 s)` e as duas amostras (passou em `build.sh --test`).
* **Validação final:** `bash build.sh --test` (via `heavy.sh`) na branch: `build exit=0`, 15 linhas
  PASS, entre elas os dois testes `Load:`, `file-mods`, semáforos e AppContent; nenhuma falha.
* **Não testado:** Windows (nem a compilação: não há MinGW aqui; a build Windows da branch é o
  primeiro teste) e o jogo.
