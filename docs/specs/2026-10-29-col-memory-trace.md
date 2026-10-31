# Col: trace de alocações, mapeamentos e retiradas da memória guest

Data: 2026-10-29. Frente: cenários ausentes e sem colisão (branch `diag/missing-collision`).
Roadmap: `docs/plans/2026-10-29-roadmap-cenarios-sem-colisao.md`, "Observabilidade necessária".

## Objetivo

Como mantenedor, quero ligar por variável de ambiente um registro curto das mudanças da memória
guest (alocação, mapeamento, substituição, retirada, unmap e liberação), com endereço, tamanho,
geração e thread, para correlacionar a área que some com as cargas de WAD (`Load:`, da branch
`diag/infinite-loading`) e com os writebacks da GPU.

## Contrato

- `SPARTAN_MEM_TRACE=1` liga; qualquer outro valor ou ausência deixa desligado.
- `SPARTAN_MEM_TRACE_MIN_KB` (padrão 64): mudanças menores que isso não são registradas, exceto
  `stale-write` e `map-failed`, sempre registradas.
- `SPARTAN_MEM_TRACE_MAX` (padrão 20000): número máximo de linhas; ao atingir, uma linha
  `Col: memory trace limit reached` e mais nada.
- Uma linha de cabeçalho ao ligar: `Col: memory trace on (min=<KiB> KiB, max=<n> lines)`.
- Formato: `Col: t=<ms monotônico> tid=<thread do SO> <op> <campos chave=valor>`. Operações:
  - `alloc phys= size= type=`
  - `release phys= size=`
  - `map kind=direct|flexible addr= size= phys= prot= gen= [replaced=<n> oldgen=<a>-<b>]`
  - `map-failed kind= addr= size= result= retired=0|1`
  - `retire addr= size= gen=<a>-<b>` (a GPU esquece a geração antiga antes de um `MAP_FIXED`)
  - `unmap addr= size= gen=<a>-<b> src=munmap|batch|release`
  - `stale-write addr= size= gen=<nova> retiring=<antiga>` (ver spec `col-stale-unmap-writeback`)
- Só mudanças (nenhuma consulta é registrada). Linhas são formadas sob o lock e impressas depois de
  soltá-lo.

## Critérios de aceite

- QUANDO `SPARTAN_MEM_TRACE` não é `1`, O SISTEMA DEVE não emitir nenhuma linha `Col:`.
- QUANDO ligado, O SISTEMA DEVE emitir, para alocar, mapear, substituir com `MAP_FIXED`, desmapear e
  liberar, uma linha por operação com endereço, tamanho, geração e thread.
- QUANDO uma substituição ocorre, O SISTEMA DEVE mostrar a geração nova e as gerações substituídas.
- QUANDO a mudança é menor que o mínimo, O SISTEMA DEVE omiti-la.
- QUANDO o limite de linhas é atingido, O SISTEMA DEVE emitir uma única linha de limite e parar.

## Requisitos não funcionais

- Desligado: um teste de inteiro por operação de memória (operações raras; nada no caminho de
  consulta por draw).
- Volume limitado por tamanho mínimo e número máximo de linhas.
- Sem dados do jogador: só endereços, tamanhos e contadores.
- Sem mudança de comportamento.

## Arquivos afetados

- `src/runtime_memory.c`
- `tests/test_memory_trace.c` (novo) e a linha correspondente em `build.sh --test`

## Plano técnico

Buffer thread-local de linhas pendentes (`col_note`), descarregado por `col_flush` depois que o lock
é solto. Thread: `gettid` (Linux), `GetCurrentThreadId` (Windows), `pthread_threadid_np` (macOS).
Tempo: `CLOCK_MONOTONIC` (Windows: `QueryPerformanceCounter`). A geração é a da spec
`col-stale-unmap-writeback`. Risco: o buffer thread-local tem tamanho fixo; linhas que não cabem são
descartadas e contadas como truncadas no fim do bloco.

## Validação

`tests/test_memory_trace.c`: desligado não imprime; ligado imprime a sequência esperada; filtro de
tamanho; limite de linhas. No jogo, o roteiro do mantenedor em
`docs/investigations/colisao/roteiro-mantenedor.md`.

## Aprovação

Aprovada pelo líder por delegação do mantenedor (2026-10-29).
