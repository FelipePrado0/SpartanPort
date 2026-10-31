# Col: escrita atrasada da geração antiga não sobrescreve a geração nova

Data: 2026-10-29. Frente: cenários ausentes e sem colisão (branch `diag/missing-collision`, base
`origin/main` = `f593377`). Roadmap: `docs/plans/2026-10-29-roadmap-cenarios-sem-colisao.md`,
seção "Testes e requisitos verificáveis".

## Objetivo

Como mantenedor, quero que uma escrita da GPU destinada a uma geração de memória já retirada (A)
não caia na geração que a substituiu no mesmo endereço guest (B), para que dados novos de uma área
não sejam sobrescritos por dados antigos durante uma transição de cenário.

## Contexto (Red observado)

`tests/test_memory.c`, `late_unmap_of_old_generation_keeps_new`: A é liberada
(`sceKernelReleaseDirectMemory`), a notificação `unmap` de A fica na fila e só roda depois que outra
thread mapeou B no mesmo endereço (`MAP_FIXED`). O readback que a GPU faz dentro dessa notificação
escreve pelo endereço guest (`runtime_memory_write_backing`) e cai nas páginas de B.

```
$ cc -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_memory.c -o out/memory-test && out/memory-test
FAIL tests/test_memory.c:197: ((unsigned char *)p)[0] == 0x22 && ((unsigned char *)p)[2 * PAGE - 1] == 0x22
1 failure(s)
exit=1
```

A intercalação exige que a notificação de A esteja em voo (fila de `flush_hooks`, que entrega sem o
lock) enquanto outra thread mapeia B. Não há prova de que isso ocorra no jogo; é um mecanismo
possível para a hipótese "dados carregados, mas sobrescritos na transição", não a causa demonstrada
de #871/#1000.

## Contrato

- Cada mapeamento guest não reservado recebe uma geração (`Vma.gen`), crescente, atribuída em
  `place()`. A divisão de um mapeamento preserva a geração.
- Enquanto uma notificação `unmap` roda (fila de `flush_hooks`, `direct_unmap` e `retire_fixed`), a
  thread que a entrega registra o intervalo e a maior geração existente quando a notificação foi
  criada.
- `runtime_memory_write_backing` chamado nessa thread, dentro desse intervalo, descarta os bytes que
  caem num mapeamento de geração maior (criado depois) e os conta. O retorno continua 1 (escrita
  tratada), para que o chamador não tente outro caminho de escrita no mesmo endereço.
- Fora de uma notificação `unmap`, ou em mapeamentos da geração antiga, nada muda.

## Critérios de aceite

- QUANDO a notificação `unmap` de A roda depois que B foi mapeada no mesmo endereço, O SISTEMA DEVE
  manter os bytes de B intactos.
- QUANDO a GPU escreve durante a retirada de A ainda mapeada (`retire_fixed`, `direct_unmap`), O
  SISTEMA DEVE gravar nas páginas de A, como hoje (`fixed_map_retires_before_replacing`).
- QUANDO uma escrita é descartada, O SISTEMA DEVE contá-la e, com `SPARTAN_MEM_TRACE=1`, emitir uma
  linha `Col: stale-write`; o relatório de saída mostra o total se for maior que zero.

## Requisitos não funcionais

- Custo fora de uma notificação `unmap`: uma leitura de variável thread-local por chamada.
- Sem novas travas; o campo `gen` é escrito só com o lock exclusivo.
- Limitação conhecida: um readback executado por outra thread (por exemplo a thread da GPU, a
  pedido da notificação) não é filtrado. Filtrar escritas de outras threads poderia descartar
  escritas legítimas em B; fica fora desta correção mínima.

## Arquivos afetados

- `src/runtime_memory.c`
- `tests/test_memory.c`

## Plano técnico

Contador `map_generation` (escrito sob o lock exclusivo, lido com `__atomic`); `PendingHook.gen`;
thread-local `retiring` salvo e restaurado ao redor de cada chamada de `hook_unmap`; filtro em
`runtime_memory_write_backing`. Risco: um caminho que dependa de escrever em B durante o `unmap` de A
na mesma thread perderia a escrita; nenhum foi encontrado (`TryWriteBacking` é usado por readbacks
e rótulos de fence, endereçados à memória que os gerou).

## Validação

- `tests/test_memory.c` inteiro (Red acima, Green depois da correção), `tests/test_memory_read_backing.c`
  e `tests/test_low_handles.c`.
- No jogo (mantenedor): nenhuma mudança visível esperada; com `SPARTAN_MEM_TRACE=1`, linhas
  `Col: stale-write` no trecho com defeito seriam evidência direta desse mecanismo.

## Aprovação

Aprovada pelo líder por delegação do mantenedor (2026-10-29).

## Registro TDD

- **Comportamento:** QUANDO a notificação `unmap` de A roda depois que B foi mapeada no mesmo
  endereço, O SISTEMA DEVE manter B intacta.
- **Teste:** `tests/test_memory.c`, `late_unmap_of_old_generation_keeps_new` (com
  `flexible_replacement_keeps_new_generation` e `refused_replacement_keeps_old_mapping`, que
  passaram já na base).
- **Red:** comando e saída na seção "Contexto" acima (base `f593377`, só os testes novos): falha na
  linha 197, B sobrescrita por `0xEE`, `exit=1`. Os outros dois testes novos passaram na base.
- **Green:** geração por mapeamento, `deliver_unmap` e o filtro em `runtime_memory_write_backing`.
  `out/memory-test`: `PASS: ... replaced generations kept, refused replacements kept (187 GPU
  notifications)`, `exit=0`; o teste também confere que a escrita descartada foi contada.
- **Refactor:** os literais `Vma` de `tests/test_memory_read_backing.c` ganharam o campo `gen`
  (`-Werror=missing-field-initializers`); `out/memory-read-backing-test` e `out/low-handles-test`
  passam.
- **Validação final:** ver o commit e o relatório da frente; `build.sh --test` completo listado lá.
