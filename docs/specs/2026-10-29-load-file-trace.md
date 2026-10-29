# Trace de arquivos `Load:` e marcador de último progresso

Data: 2026-10-29. Branch: `diag/infinite-loading`. Frente: carregamento infinito. Base: a build
histórica `windows-v2.6.0-issue345-test1` (commit `397c307`, "Feat(Runtime): added optional WAD file
tracing"), que nunca entrou na `main`; esta spec a refaz sobre a base atual com prefixo `Load:`,
thread, estado por arquivo e marcador de progresso. Reutilizável pelas frentes de colisão e Alto
FPS (F3) para saber qual WAD de área foi lido e quando.

## SDD

* **Objetivo:** como mantenedor investigando um carregamento infinito, quero um registro opcional de
  cada abertura/leitura/fechamento de arquivo do jogo, com início e fim, para ver se o WAD da área foi
  lido por inteiro e qual operação ou arquivo ficou parado.
* **Contrato (variáveis de ambiente, lidas uma vez):**
  * `SPARTAN_FILE_TRACE=1`: registra arquivos `.wad` (maiúsculas ou minúsculas), como a build
    histórica. `SPARTAN_FILE_TRACE=all`: registra todo arquivo aberto pelo jogo. Qualquer outro valor
    ou ausente: desligado.
  * `SPARTAN_FILE_TRACE_LIMIT=<n>`: máximo de linhas `Load:` de operação (padrão 20000; 0 = sem
    limite). Ao atingir, uma linha `Load: trace limit <n> reached` e nenhuma outra linha de operação;
    o estado em memória continua sendo atualizado.
  * Linhas (stdout, que o launcher grava em `user/last_run.log`; `fflush` a cada linha para sobreviver
    a um travamento):
    * `Load: trace on (mode=wad|all, limit=N)` uma vez.
    * `Load: #<id> begin <open|read|pread|close> fd=<fd> tid=<tid> thread='<nome>' t_us=<monotônico> off=<offset|-1> req=<bytes> path=<caminho do jogo>`
    * `Load: #<id> end <op> fd=<fd> tid=<tid> t_us=<…> result=<retorno> errno=<0|errno do host> us=<duração> [size=<tamanho do arquivo> (open)] [done=<bytes lidos>/<tamanho> (read/pread)]`
    * Na saída do jogo (`runtime_file_report`), e no relatório do watchdog: `Load: state …` com o
      último progresso, as operações em andamento e os arquivos rastreados ainda abertos
      (`open fd=… path=… read=<bytes>/<tamanho> reads=<n> opened <s> s ago, last <s> s ago`).
  * `read` registra o offset atual do descritor (lseek) no `begin`.
  * Retornos e `errno` das operações não mudam; o trace preserva `errno`.
* **Critérios de aceite:**
  * QUANDO `SPARTAN_FILE_TRACE` não está definido O SISTEMA DEVE não escrever nenhuma linha `Load:` nem guardar estado, com custo de uma leitura atômica por operação.
  * QUANDO `SPARTAN_FILE_TRACE=1` e o jogo lê um `.wad` O SISTEMA DEVE escrever `begin` antes e `end` depois de cada open/read/pread/close, com id, thread, tempo monotônico, offset, bytes pedidos e devolvidos e `errno`.
  * QUANDO uma leitura devolve menos que o pedido (parcial ou EOF) O SISTEMA DEVE registrar o valor devolvido real (`result`), sem completar nada.
  * QUANDO uma leitura falha O SISTEMA DEVE registrar `result=-<errno>` e `errno=<errno>` e devolver ao jogo o mesmo erro de antes.
  * QUANDO a escrita do log falha O SISTEMA DEVE preservar o `errno` que o chamador lê.
  * QUANDO o arquivo não é `.wad` e o modo é `1` O SISTEMA DEVE não registrá-lo.
  * QUANDO o limite é atingido O SISTEMA DEVE escrever uma única linha de limite e continuar atualizando o estado.
  * QUANDO o estado é pedido O SISTEMA DEVE listar operações em andamento (com idade e thread), arquivos rastreados abertos com bytes lidos/tamanho e o último progresso.
* **Requisitos não funcionais:** desligado por padrão; sem alocação no caminho de leitura; estado em
  tabelas fixas (operações em andamento: 64; arquivos: os 1024 descritores existentes); volume
  limitado; prefixo `Load:`; Windows e Linux (mesmo código, `clock_gettime(CLOCK_MONOTONIC)` já usado
  pelo runtime nos dois). O trace muda o timing (um `fflush` por linha): não usar para medir
  desempenho.
* **Arquivos afetados:** `src/runtime_file.c`, `src/runtime_thread.c` (nome e id da thread atual),
  `src/runtime.h`, `tests/test_load_trace.c` (novo), `build.sh` (o teste entra em `--test`).
* **Plano técnico:** `load_begin()/load_end()` em volta de `do_open/do_close/do_read/do_pread`; o
  estado por arquivo fica no `File` existente (protegido pelo `lock` existente); nome e id da thread
  vêm de `runtime_thread_current_name()` e `runtime_thread_host_id()` (novos, não criam registro).
  Riscos: volume de log em sessões longas (limite), contenção do `lock` só com o trace ligado.
* **Validação:** `tests/test_load_trace.c` (desligado por padrão, leitura completa, parcial, EOF, erro,
  `errno` preservado com stdout falhando, filtro `.wad`, limite, estado); build Linux completa; no
  Windows, a build da branch com `SPARTAN_FILE_TRACE=1` (roteiro do mantenedor).
* **Aprovação:** aprovada pelo líder por delegação do mantenedor (2026-10-29).

## TDD

* **Comportamento:** critérios de aceite acima (e o detector de parada da spec do watchdog, que usa
  o mesmo estado).
* **Teste:** `tests/test_load_trace.c` (inclui `src/runtime_file.c`, como `test_file_mods.c`; arquivos
  reais numa pasta temporária: `AREA.WAD` de 10000 bytes, `other.bin`, diretório `dir.wad`).
* **Red:** `gcc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_load_trace.c -o out/load-trace-test`
  antes da implementação: 17 erros, `'load_out' undeclared`, `'load_watchdog_autostart' undeclared`,
  `'load_mode' undeclared`... (o contrato não existia).
* **Green:** bloco `Load:` em `src/runtime_file.c` + declarações em `src/runtime.h`; mesmo comando e
  `out/load-trace-test`: `PASS: Load: trace off by default, begin/end of open/read/pread/close,
  partial/EOF/error reads, errno kept, WAD filter, limit, state, stall decision`.
* **Prova de que o teste detecta regressões (mutações numa cópia em scratchpad):**
  * sem `errno=saved_errno` no fim de `load_end`: `Assertion 'errno==4242' failed`;
  * `covered+=result` em vez de `covered=offset+result`: `Assertion 'has_line(log,"end pread","result=2000 errno=0","done=10000/10000")' failed`.
* **Refactor:** `tests/test_file_mods.c` ganhou stubs dos três símbolos novos de `runtime_thread.c`;
  o teste entrou em `build.sh --test` (bloco Linux).
* **Validação final:** `bash build.sh --test` (via `heavy.sh`): `build exit=0`, `spartanport-probe`
  ligado com o runtime novo, `PASS: Load: trace off by default, ...` e os demais testes C passando.
  `python3 -m unittest test_arch_pkgbuild` (lê `runtime_thread.c`): `Ran 10 tests ... OK`. Não
  testado aqui: Windows (MinGW/winpthreads) e o jogo.
